#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>

static const int INPUT_W = 640;
static const int INPUT_H = 640;
static const float CONF_THRESH = 0.55f; // 和你 Python 一样
static const float NMS_THRESH = 0.45f;

struct Detection {
    cv::Rect box;
    float score;
    int class_id;
};

std::vector<int> nms(const std::vector<Detection> &dets)
{
    std::vector<int> keep;
    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    for (auto &d : dets) {
        boxes.push_back(d.box);
        scores.push_back(d.score);
    }

    // 这里只做几何上的 NMS，分数阈值设为 0，
    // 置信度过滤已经在 decode 阶段完成
    cv::dnn::NMSBoxes(boxes, scores, 0.0f, NMS_THRESH, keep);
    return keep;
}

// 🔁 和 Python 完全对齐的预处理：resize -> RGB -> /255 -> CHW
void preprocess(const cv::Mat &frame, std::vector<float> &input_tensor_values)
{
    cv::Mat resized, rgb, float_img;

    // 注意：不要 letterbox，只做简单 resize，和 Python 完全一致
    cv::resize(frame, resized, cv::Size(INPUT_W, INPUT_H));
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    rgb.convertTo(float_img, CV_32F, 1.0f / 255.0f);

    input_tensor_values.resize(1 * 3 * INPUT_H * INPUT_W);

    // HWC -> CHW
    int idx = 0;
    for (int c = 0; c < 3; ++c) {
        for (int y = 0; y < INPUT_H; ++y) {
            for (int x = 0; x < INPUT_W; ++x) {
                input_tensor_values[idx++] = float_img.at<cv::Vec3f>(y, x)[c];
            }
        }
    }
    std::cout << float_img.at<cv::Vec3f>(0, 0) << std::endl;
}

// 解析 YOLOv8 输出 (1,84,8400)，不转置，按通道优先方式直接访问
std::vector<Detection> decode(
    const float *data,
    int num_rows, // 8400
    int num_cols, // 84
    int orig_w,
    int orig_h)
{
    std::vector<Detection> dets;
    float max_obj = 0.0f, max_cls = 0.0f;

    // data layout: [1, C=84, N=8400]
    // 索引: data[c * N + i]
    int C = num_cols;
    int N = num_rows;

    for (int i = 0; i < N; ++i) {
        float cx = data[0 * N + i];
        float cy = data[1 * N + i];
        float w = data[2 * N + i];
        float h = data[3 * N + i];
        float obj = data[4 * N + i];

        if (obj < CONF_THRESH)
            continue;

        // 找最大类别分数
        int best_cls_id = -1;
        float best_cls_score = 0.0f;

        for (int c = 5; c < C; ++c) {
            // 模型已经输出的是概率（和 Python 一样），不需要再 sigmoid
            float cls_score = data[c * N + i];

            if (cls_score > best_cls_score) {
                best_cls_score = cls_score;
                best_cls_id = c - 5; // 类别索引
            }
        }

        max_obj = std::max(max_obj, obj);
        max_cls = std::max(max_cls, best_cls_score);

        // 只用 obj 做一次阈值过滤，与 Python 对齐
        float final_score = best_cls_score;

        // cxcywh -> xyxy（坐标在 0~640）
        float x1 = cx - w / 2.0f;
        float y1 = cy - h / 2.0f;
        float x2 = cx + w / 2.0f;
        float y2 = cy + h / 2.0f;

        // 映射回原图尺寸（和你 Python 完全一致）
        x1 = x1 * orig_w / INPUT_W;
        y1 = y1 * orig_h / INPUT_H;
        x2 = x2 * orig_w / INPUT_W;
        y2 = y2 * orig_h / INPUT_H;

        cv::Rect box(
            cv::Point(std::max(int(x1), 0), std::max(int(y1), 0)),
            cv::Point(std::min(int(x2), orig_w - 1), std::min(int(y2), orig_h - 1)));

        dets.push_back({ box, final_score, best_cls_id });
    }

    std::cout << "max obj=" << max_obj << " max cls=" << max_cls << std::endl;

    return dets;
}

int main()
{
    std::string model_path = "/home/wjjsn/yolov8n.onnx";
    std::string video_path = "/home/wjjsn/test.mp4";

    cv::VideoCapture cap(video_path);
    if (!cap.isOpened()) {
        std::cerr << "读取视频失败: " << video_path << std::endl;
        return -1;
    }

    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "yolo_video");
    Ort::SessionOptions options;
    options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

    // 如果你有编译好的 arm64 onnxruntime，可以在这里设置线程数等
    // options.SetIntraOpNumThreads(2);

    Ort::Session session(env, model_path.c_str(), options);

    Ort::AllocatorWithDefaultOptions allocator;
    auto input_name_alloc = session.GetInputNameAllocated(0, allocator);
    auto output_name_alloc = session.GetOutputNameAllocated(0, allocator);

    const char *input_name = input_name_alloc.get();
    const char *output_name = output_name_alloc.get();

    std::vector<int64_t> input_shape = { 1, 3, INPUT_H, INPUT_W };
    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);

    // cv::namedWindow("YOLOv8", cv::WINDOW_NORMAL);

    cv::Mat frame;
    while (cap.read(frame)) {
        int orig_w = frame.cols;
        int orig_h = frame.rows;

        std::vector<float> input_tensor;
        preprocess(frame, input_tensor);

        Ort::Value input = Ort::Value::CreateTensor<float>(
            memory_info,
            input_tensor.data(),
            input_tensor.size(),
            input_shape.data(),
            input_shape.size());

        std::vector<const char *> input_names = { input_name };
        std::vector<const char *> output_names = { output_name };

        auto start = cv::getTickCount();

        auto output_tensors = session.Run(
            Ort::RunOptions{ nullptr },
            input_names.data(), &input, 1,
            output_names.data(), 1);

        float ms = (cv::getTickCount() - start) * 1000.0f / cv::getTickFrequency();
        std::cout << "推理耗时: " << ms << " ms" << std::endl;

        // 解析输出
        auto &out = output_tensors[0];
        auto info = out.GetTensorTypeAndShapeInfo();
        auto shape = info.GetShape(); // {1, 84, 8400}

        if (shape.size() != 3 || shape[0] != 1) {
            std::cerr << "Unexpected output shape: ";
            for (auto s : shape)
                std::cerr << s << " ";
            std::cerr << std::endl;
            break;
        }

        int C = static_cast<int>(shape[1]); // 84
        int N = static_cast<int>(shape[2]); // 8400

        const float *data = out.GetTensorData<float>();

        // 调试：打印前几个值看是不是全 0
        /*
		for (int i = 0; i < 20; ++i) {
			std::cout << data[i] << " ";
		}
		std::cout << std::endl;
		*/

        auto dets = decode(data, N, C, orig_w, orig_h);
        auto keep = nms(dets);

        // 画框
        for (int idx : keep) {
            auto &d = dets[idx];
            cv::rectangle(frame, d.box, cv::Scalar(0, 255, 0), 2);
            char text[64];
            sprintf(text, "%d: %.2f", d.class_id, d.score);
            cv::putText(frame, text, d.box.tl(), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                        cv::Scalar(0, 255, 0), 1);
        }

        cv::putText(frame, cv::format("FPS: %.1f", 1000.0f / ms),
                    cv::Point(20, 40), cv::FONT_HERSHEY_SIMPLEX, 1.0,
                    cv::Scalar(0, 255, 255), 2);

        cv::imshow("YOLOv8", frame);
        if (cv::waitKey(1) == 'q')
            break;
    }

    return 0;
}
