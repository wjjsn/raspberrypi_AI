/**
 * Copyright (c) 2019-2024 Hailo Technologies Ltd. All rights reserved.
 * Distributed under the MIT license (https://opensource.org/licenses/MIT)
 **/
/**
 * @file vstreams_example
 * This example demonstrates using virtual streams over c++
 **/

#include "hailo/hailort.hpp"
#include "opencv2/opencv.hpp"
#include <cstddef>
#include <cstdint>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <chrono>

#define HEF_FILE ("/home/wjjsn/code/yolov8n.hef")
constexpr auto video_path = "/home/wjjsn/test.mp4";
constexpr size_t MAX_LAYER_EDGES = 16;

using namespace hailort;

Expected<std::shared_ptr<ConfiguredNetworkGroup> > configure_network_group(VDevice &vdevice)
{
    auto hef = Hef::create(HEF_FILE);
    if (!hef) {
        return make_unexpected(hef.status());
    }

    auto configure_params = vdevice.create_configure_params(hef.value());
    if (!configure_params) {
        return make_unexpected(configure_params.status());
    }

    auto network_groups = vdevice.configure(hef.value(), configure_params.value());
    if (!network_groups) {
        return make_unexpected(network_groups.status());
    }

    if (1 != network_groups->size()) {
        std::cerr << "Invalid amount of network groups" << std::endl;
        return make_unexpected(HAILO_INTERNAL_FAILURE);
    }

    return std::move(network_groups->at(0));
}

std::atomic<bool> g_stop_requested{ false };
int main()
{
    std::signal(SIGINT, [](int signal) {
        if (signal == SIGINT) {
            g_stop_requested = true;
        }
    });

    auto vdevice = VDevice::create();
    if (!vdevice) {
        std::cerr << "Failed create vdevice, status = " << vdevice.status() << std::endl;
        return vdevice.status();
    }

    auto network_group = configure_network_group(*vdevice.value());
    if (!network_group) {
        std::cerr << "Failed to configure network group " << HEF_FILE << std::endl;
        return network_group.status();
    }

    // Set input format type to auto - libhailort will not scale the data before writing to the HW
    auto input_vstream_params = network_group.value()->make_input_vstream_params({}, HAILO_FORMAT_TYPE_AUTO, HAILO_DEFAULT_VSTREAM_TIMEOUT_MS,
                                                                                 HAILO_DEFAULT_VSTREAM_QUEUE_SIZE);
    if (!input_vstream_params) {
        std::cerr << "Failed creating input vstreams params " << input_vstream_params.status() << std::endl;
        return input_vstream_params.status();
    }

    /* The input format order in the example HEF is NHWC in the user-side (may be seen using 'hailortcli parse-hef <HEF_PATH>).
	   Here we override the user-side format order to be NCHW */
    for (auto &params_pair : *input_vstream_params) {
        params_pair.second.user_buffer_format.order = HAILO_FORMAT_ORDER_NHWC;
    }

    auto input_vstreams = VStreamsBuilder::create_input_vstreams(*network_group.value(), *input_vstream_params);
    if (!input_vstreams) {
        std::cerr << "Failed creating input vstreams " << input_vstreams.status() << std::endl;
        return input_vstreams.status();
    }

    // Set output format type to float32 - libhailort will de-quantize the data after reading from the HW
    // Note: this process might affect the overall performance
    auto output_vstream_params = network_group.value()->make_output_vstream_params({}, HAILO_FORMAT_TYPE_FLOAT32, HAILO_DEFAULT_VSTREAM_TIMEOUT_MS,
                                                                                   HAILO_DEFAULT_VSTREAM_QUEUE_SIZE);
    if (!output_vstream_params) {
        std::cerr << "Failed creating output vstreams params " << output_vstream_params.status() << std::endl;
        return output_vstream_params.status();
    }
    auto output_vstreams = VStreamsBuilder::create_output_vstreams(*network_group.value(), *output_vstream_params);
    if (!output_vstreams) {
        std::cerr << "Failed creating output vstreams " << output_vstreams.status() << std::endl;
        return output_vstreams.status();
    }

    if (input_vstreams->size() > MAX_LAYER_EDGES || output_vstreams->size() > MAX_LAYER_EDGES) {
        std::cerr << "Trying to infer network with too many input/output virtual streams, Maximum amount is " << MAX_LAYER_EDGES << " (either change HEF or change the definition of MAX_LAYER_EDGES)" << std::endl;
        return HAILO_INVALID_OPERATION;
    }

    cv::VideoCapture cap(video_path);
    if (!cap.isOpened()) {
        std::cerr << "Failed to open video file " << video_path << std::endl;
        return -1;
    }

    using namespace std::chrono_literals;
    std::size_t frame_count = 0;
    auto before_while = std::chrono::high_resolution_clock::now();
    while (true) {
        if (g_stop_requested) {
            std::cout << "\n收到 Ctrl+C，准备退出...\n";
            std::cout << "共推理" << frame_count << "帧。" << "平均一帧耗时:" << (std::chrono::high_resolution_clock::now() - before_while) / 1ms / frame_count << "ms" << std::endl;
            exit(0);
        }

        // auto status = infer(*input_vstreams, *output_vstreams);
        std::cout << "第" << frame_count++ << "帧" << std::endl;
        auto all_start = std::chrono::high_resolution_clock::now();
        auto status = HAILO_SUCCESS;

        cv::Mat frame;
        cap >> frame;
        if (frame.empty()) {
            std::cout << "End of video file" << std::endl;
            return 0;
        }
        auto write_frame = [&frame, &all_start](InputVStream &input, hailo_status &status) {
            auto opencv_start = std::chrono::high_resolution_clock::now();

            cv::Mat processed;
            cv::resize(frame, processed, cv::Size(640, 640));
            cv::cvtColor(processed, processed, cv::COLOR_BGR2RGB);
            auto opencv_time = std::chrono::high_resolution_clock::now();
            auto write_time = opencv_time - opencv_start;
            std::cout << "OpenCV预处理耗时：" << write_time / 1ms << "ms" << std::endl;
            std::vector<uint8_t>
                data(input.get_frame_size());

            // 检查 Mat 是否连续（通常是连续的）
            if (!processed.isContinuous()) {
                processed = processed.clone();
            }

            // 将 Mat 数据复制到 vector 中
            std::memcpy(data.data(), processed.data, input.get_frame_size());

            status = input.write(MemoryView(data.data(), data.size()));
            if (HAILO_SUCCESS != status) {
                return;
            }

            // Flushing is not mandatory here
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
            auto opencv_start = std::chrono::high_resolution_clock::now();
            std::vector<float> out(output.get_frame_size() / sizeof(float));
            status = output.read(MemoryView(out.data(), output.get_frame_size()));
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
                }
            }
            std::cout << "画框耗时：" << (std::chrono::high_resolution_clock::now() - opencv_start) / 1ms << "ms" << std::endl;
        };

        /*向NPU写入数据*/
        write_frame(input_vstreams.value()[0], status);

        /*从NPU读取数据*/
        read_output(output_vstreams.value()[0], status);

        std::cout << "推理全过程耗时：" << (std::chrono::high_resolution_clock::now() - all_start) / 1ms << "ms" << std::endl;
        cv::imshow("frame", frame);
        if (cv::waitKey(1) == 'q')
            break;

        std::cout << "从读入一帧到显示耗时：" << (std::chrono::high_resolution_clock::now() - all_start) / 1ms << "ms" << std::endl;
        if (HAILO_SUCCESS != status) {
            std::cerr << "Inference failed " << status << std::endl;
            return status;
        }
    }

    return HAILO_SUCCESS;
}
