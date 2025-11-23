#include <iostream>
#include <vector>
#include <onnxruntime_cxx_api.h>

int main() {
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "yolo_test");
    Ort::SessionOptions session_options;
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

    Ort::Session session(env, "/home/wjjsn/yolov8n.onnx", session_options);

    // 输入 shape
    std::vector<int64_t> input_shape = {1, 3, 640, 640};
    std::vector<float> input_tensor_values(1 * 3 * 640 * 640, 1.0f);

    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);

    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info,
        input_tensor_values.data(),
        input_tensor_values.size(),
        input_shape.data(),
        input_shape.size()
    );

    // 读取输入输出名（适配 ONNX Runtime 1.20+）
    Ort::AllocatorWithDefaultOptions allocator;
    auto input_name_alloc = session.GetInputNameAllocated(0, allocator);
    auto output_name_alloc = session.GetOutputNameAllocated(0, allocator);

    const char* input_name = input_name_alloc.get();
    const char* output_name = output_name_alloc.get();

    std::vector<const char*> input_names = {input_name};
    std::vector<const char*> output_names = {output_name};

    // 推理
    auto output_tensors = session.Run(
        Ort::RunOptions{nullptr},
        input_names.data(),
        &input_tensor,
        1,
        output_names.data(),
        1
    );

    std::cout << "推理成功！输出元素数量："
              << output_tensors[0].GetTensorTypeAndShapeInfo().GetElementCount()
              << std::endl;

    return 0;
}
