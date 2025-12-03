#include <condition_variable>
#include <csignal>
#include <atomic>
#include <iostream>
#include <thread>
#include "hailo/hailort.hpp"
#include "opencv2/opencv.hpp"

#include "config.hpp"
#include "thread_safe_queue.hpp"

using namespace hailort;
using namespace std::chrono_literals;

std::atomic<bool> g_stop_requested{ false };
std::atomic<bool> g_v4l2_requeue{ true };
thread_safe_queue<cv::Mat> g_capture_queue{};
thread_safe_queue<cv::Mat> g_imshow_queue{};

extern Expected<std::shared_ptr<ConfiguredNetworkGroup> > configure_network_group(VDevice &vdevice, std::string hef_path);
extern auto hailo_vdevice_init(const std::string_view hef_path, std::size_t max_layer_edges = 16) -> std::tuple<Expected<std::vector<InputVStream> >, Expected<std::vector<OutputVStream> > >;
extern void infer_thread(Expected<std::vector<InputVStream> > input_vstreams, Expected<std::vector<OutputVStream> > output_vstreams);
extern void capture_thread();

int main()
{
    std::signal(SIGINT, [](int signal) {
        if (signal == SIGINT) {
            g_stop_requested = true;
        }
    });

    /*采集线程*/
    auto cap_handle = std::thread(capture_thread);
    // cap_handle.detach();

    /*初始化Vdevice*/
    auto vdevice = VDevice::create();
    if (!vdevice) {
        std::cerr << "Failed create vdevice, status = " << vdevice.status() << std::endl;
        return vdevice.status();
    }

    auto network_group = configure_network_group(*vdevice.value(), HEF_FILE);
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

    const std::size_t MAX_LAYER_EDGES = 16;
    if (input_vstreams->size() > MAX_LAYER_EDGES || output_vstreams->size() > MAX_LAYER_EDGES) {
        std::cerr << "Trying to infer network with too many input/output virtual streams, Maximum amount is " << MAX_LAYER_EDGES << " (either change HEF or change the definition of MAX_LAYER_EDGES)" << std::endl;
        return HAILO_INVALID_OPERATION;
    }
    auto infer_handle = std::thread(infer_thread, std::move(input_vstreams), std::move(output_vstreams));
    // infer_handle.detach();

    /*显示线程*/
    while (!g_stop_requested) {
        cv::Mat img;
        g_imshow_queue.front_pop(img);
        std::cout << "imshow_queue size: " << g_imshow_queue.queue_.size() << std::endl;
        cv::imshow("hailo_cam", img);
        if (cv::waitKey(1) == 'q')
            g_stop_requested = true;
        // g_v4l2_requeue.store(true);
        // g_v4l2_requeue.notify_one();
    }
    cap_handle.join();
    infer_handle.join();

    return 0;
}
