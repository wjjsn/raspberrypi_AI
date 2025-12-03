#include "hailo/hailort.hpp"
#include "thread_safe_queue.hpp"
#include <cstdlib>
#include <iostream>
#include <opencv2/imgcodecs.hpp>
#include "opencv2/opencv.hpp"

using namespace hailort;
using namespace std::chrono_literals;

extern std::atomic<bool> g_stop_requested;
extern thread_safe_queue<cv::Mat> g_capture_queue;
extern thread_safe_queue<cv::Mat> g_imshow_queue;

void infer_thread(Expected<std::vector<InputVStream> > input_vstreams, Expected<std::vector<OutputVStream> > output_vstreams)
{
    auto status = HAILO_SUCCESS;
    std::size_t frame_count = 0;
    auto before_while = std::chrono::high_resolution_clock::now();
    while (true) {
        if (g_stop_requested) {
            std::cout << "\n收到 Ctrl+C 或推理结束，准备退出...\n";
            std::cout << "共推理" << --frame_count << "帧。" << "平均一帧耗时:" << (std::chrono::high_resolution_clock::now() - before_while) / 1ms / frame_count << "ms" << std::endl;
            break;
        }

        std::cout << "第" << frame_count++ << "帧" << std::endl;
        auto all_start = std::chrono::high_resolution_clock::now();

        auto get_frame_start = std::chrono::high_resolution_clock::now();

        cv::Mat frame;
        g_capture_queue.front_pop(frame);
        cv::imwrite("test.jpg", frame);
        std::cout << "获取一帧耗时：" << (std::chrono::high_resolution_clock::now() - get_frame_start) / 1ms << "ms" << std::endl;

        if (frame.empty()) {
            std::cout << "End of video file" << std::endl;
            g_stop_requested = true;
            continue;
        }
        auto write_frame = [&frame, &all_start](InputVStream &input, hailo_status &status) {
            auto opencv_start = std::chrono::high_resolution_clock::now();

            cv::Mat processed;
            cv::resize(frame, processed, cv::Size(640, 640));
            // cv::cvtColor(processed, processed, cv::COLOR_BGR2RGB);
            auto opencv_time = std::chrono::high_resolution_clock::now();
            auto write_time = opencv_time - opencv_start;
            std::cout << "OpenCV预处理耗时：" << write_time / 1ms << "ms" << std::endl;

            // 检查 Mat 是否连续（通常是连续的）
            if (!processed.isContinuous()) {
                processed = processed.clone();
            }

            // 将 Mat 数据复制到 vector 中
            // std::cout << "processed.size" << processed.total() * processed.elemSize() << std::endl;
            // std::cout << "input.get_frame_size()" << input.get_frame_size() << std::endl;
            // std::memcpy(data.data(), processed.data, input.get_frame_size());
            if (processed.total() * processed.elemSize() != input.get_frame_size()) {
                g_stop_requested = true;
                std::cerr << "Mat数据大小不匹配" << std::endl;
                return;
            }
            status = input.write(MemoryView(processed.data, input.get_frame_size()));
            if (HAILO_SUCCESS != status) {
                std::cerr << "Failed writing to input vstream: " << status << std::endl;
                return;
            }

            // Flushing is not mandatory here - removed to prevent timeout issues
            status = input.flush();
            if (HAILO_SUCCESS != status) {
                std::cerr << "Failed flushing input vstream" << std::endl;
                return;
            }
            std::cout << "内存复制耗时：" << (std::chrono::high_resolution_clock::now() - opencv_time) / 1ms << "ms" << std::endl;
            status = HAILO_SUCCESS;
        };
        auto read_output = [&frame](OutputVStream &output, hailo_status &status) {
            // 1. 读取完整的数据 (160320 bytes)
            auto read_time = std::chrono::high_resolution_clock::now();
            std::vector<float> out(output.get_frame_size() / sizeof(float));
            status = output.read(MemoryView(out.data(), output.get_frame_size()));
            std::cout << "NPU推理耗时：" << (std::chrono::high_resolution_clock::now() - read_time) / 1ms << "ms" << std::endl;
            auto opencv_start = std::chrono::high_resolution_clock::now();
            if (status != HAILO_SUCCESS)
                return;

            int width = frame.cols;
            int height = frame.rows;

            // 定义常量
            const int NUM_CLASSES = 80;
            const int MAX_BOXES_PER_CLASS = 100;
            const int BOX_DIM = 5; // (ymin, xmin, ymax, xmax, score) 注意没有class_id，因为class_id由外层循环决定

            // 每一个Class占用的 float 数量 = 1个计数器 + 100个框 * 5个数据
            const int CLASS_STRIDE = 1 + (MAX_BOXES_PER_CLASS * BOX_DIM);

            // 外层循环：遍历所有 80 个类别
            for (int class_id = 0; class_id < NUM_CLASSES; class_id++) {
                // 计算当前类别的起始位置
                int class_offset = class_id * CLASS_STRIDE;

                // 获取当前类别检测到的数量
                int count = static_cast<int>(out[class_offset]);

                // 如果这个类别没有检测到东西，直接跳过，处理下一个类别
                if (count <= 0)
                    continue;

                // 内层循环：只读取有效数量的框
                for (int i = 0; i < count; i++) {
                    // 计算具体某个框的起始位置
                    // class_offset + 1 (跳过计数器) + i * 5
                    int box_idx = class_offset + 1 + i * BOX_DIM;

                    float y_min_norm = out[box_idx + 0];
                    float x_min_norm = out[box_idx + 1];
                    float y_max_norm = out[box_idx + 2];
                    float x_max_norm = out[box_idx + 3];
                    float score = out[box_idx + 4];

                    // 过滤低置信度 (Log里说阈值是0.2，这里可以再次过滤)
                    if (score < 0.25f)
                        continue;

                    // 坐标还原 (反归一化)
                    int x1 = (int)(x_min_norm * width);
                    int y1 = (int)(y_min_norm * height);
                    int x2 = (int)(x_max_norm * width);
                    int y2 = (int)(y_max_norm * height);

                    // 绘制矩形
                    cv::rectangle(frame, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(0, 255, 0), 2);

                    // 绘制标签
                    // 这里可以直接用 class_id，比如 0 就是 Person
                    std::string label = std::to_string(class_id) + " " + std::to_string(score).substr(0, 4);

                    int text_y = y1 - 5;
                    if (text_y < 20)
                        text_y = y1 + 20;

                    cv::putText(frame, label, cv::Point(x1, text_y),
                                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 1);

                    // auto tp = std::chrono::system_clock::now();
                    // auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
                    // std::time_t t = std::chrono::system_clock::to_time_t(tp);
                    // cv::putText(frame, (std::stringstream{} << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << ms.count()).str(),
                    //             { 10, 30 }, cv::FONT_HERSHEY_SIMPLEX, 1.0, { 0, 255, 0 }, 2);
                }
            }
            std::cout << "画框耗时：" << (std::chrono::high_resolution_clock::now() - opencv_start) / 1ms << "ms" << std::endl;
            g_imshow_queue.push(std::move(frame));
        };

        /*向NPU写入数据*/
        write_frame(input_vstreams.value()[0], status);

        /*从NPU读取数据*/
        read_output(output_vstreams.value()[0], status);

        std::cout << "全过程耗时：" << (std::chrono::high_resolution_clock::now() - all_start) / 1ms << "ms" << std::endl;

        if (HAILO_SUCCESS != status) {
            std::cerr << "Inference failed " << status << std::endl;
            g_stop_requested = true;
            break;
        }
    }
}