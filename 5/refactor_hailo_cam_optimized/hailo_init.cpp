#include <iostream>
#include <string_view>
#include <tuple>
#include "hailo/hailort.hpp"

using namespace hailort;

Expected<std::shared_ptr<ConfiguredNetworkGroup> > configure_network_group(VDevice &vdevice, std::string hef_path)
{
    auto hef = Hef::create(hef_path);
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

auto hailo_vdevice_init(const std::string_view hef_path, std::size_t max_layer_edges = 16) -> std::tuple<std::vector<InputVStream>, std::vector<OutputVStream> >
{
    auto vdevice = VDevice::create();
    if (!vdevice) {
        std::cerr << "Failed create vdevice, status = " << vdevice.status() << std::endl;
        exit(vdevice.status());
    }
    auto network_group = configure_network_group(*vdevice.value(), std::string(hef_path));
    if (!network_group) {
        std::cerr << "Failed to configure network group " << hef_path << std::endl;
        exit(network_group.status());
    }

    // Set input format type to auto - libhailort will not scale the data before writing to the HW
    auto input_vstream_params = network_group.value()->make_input_vstream_params({}, HAILO_FORMAT_TYPE_AUTO, HAILO_DEFAULT_VSTREAM_TIMEOUT_MS,
                                                                                 HAILO_DEFAULT_VSTREAM_QUEUE_SIZE);
    if (!input_vstream_params) {
        std::cerr << "Failed creating input vstreams params " << input_vstream_params.status() << std::endl;
        exit(input_vstream_params.status());
    }

    /* The input format order in the example HEF is NHWC in the user-side (may be seen using 'hailortcli parse-hef <HEF_PATH>).
	   Here we override the user-side format order to be NCHW */
    for (auto &params_pair : *input_vstream_params) {
        params_pair.second.user_buffer_format.order = HAILO_FORMAT_ORDER_NHWC;
    }

    auto input_vstreams = VStreamsBuilder::create_input_vstreams(*network_group.value(), *input_vstream_params);
    if (!input_vstreams) {
        std::cerr << "Failed creating input vstreams " << input_vstreams.status() << std::endl;
        exit(input_vstreams.status());
    }

    // Set output format type to float32 - libhailort will de-quantize the data after reading from the HW
    // Note: this process might affect the overall performance
    auto output_vstream_params = network_group.value()->make_output_vstream_params({}, HAILO_FORMAT_TYPE_FLOAT32, HAILO_DEFAULT_VSTREAM_TIMEOUT_MS,
                                                                                   HAILO_DEFAULT_VSTREAM_QUEUE_SIZE);
    if (!output_vstream_params) {
        std::cerr << "Failed creating output vstreams params " << output_vstream_params.status() << std::endl;
        exit(output_vstream_params.status());
    }
    auto output_vstreams = VStreamsBuilder::create_output_vstreams(*network_group.value(), *output_vstream_params);
    if (!output_vstreams) {
        std::cerr << "Failed creating output vstreams " << output_vstreams.status() << std::endl;
        exit(output_vstreams.status());
    }

    if (input_vstreams->size() > max_layer_edges || output_vstreams->size() > max_layer_edges) {
        std::cerr << "Trying to infer network with too many input/output virtual streams, Maximum amount is " << max_layer_edges << " (either change HEF or change the definition of MAX_LAYER_EDGES)" << std::endl;
        exit(HAILO_INVALID_OPERATION);
    } else {
        std::cout << "Successfully initialized hailo virtual device" << std::endl;
        std::cout << "Input virtual streams: " << input_vstreams.value().size() << std::endl;
        std::cout << "Output virtual streams: " << output_vstreams.value().size() << std::endl;
    }
    return { std::move(input_vstreams.value()), std::move(output_vstreams.value()) };
}