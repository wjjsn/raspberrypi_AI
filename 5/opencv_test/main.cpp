
#include <opencv2/opencv.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
using namespace std::chrono_literals;

std::atomic<bool> g_stop_requested{ false };
int main()
{
    std::signal(SIGINT, [](int signal) {
        if (signal == SIGINT) {
            g_stop_requested = true;
        }
    });

    cv::VideoCapture cap("/dev/video0", cv::CAP_V4L2);
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1920);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 1080);
    cap.set(cv::CAP_PROP_FPS, 30);
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap.set(cv::CAP_PROP_BUFFERSIZE, 10);

    if (!cap.isOpened()) {
        std::cerr << "无法打开摄像头" << std::endl;
        return -1;
    }

    cv::Mat frame;
    auto count = 0;
    auto before_while = std::chrono::system_clock::now();
    while (!g_stop_requested) {
        std::cout << "第" << count + 1 << "帧" << std::endl;
        auto before_cap = std::chrono::system_clock::now();
        cap >> frame;
        std::cout << "捕获一帧耗时：" << (std::chrono::system_clock::now() - before_cap) / 1ms << "ms" << std::endl;
        if (frame.empty()) {
            std::cerr << "无法读取摄像头图像" << std::endl;
            break;
        }
        std::this_thread::sleep_for(100ms);
        count++;
    }
    std::cout << "\n结束！\n";
    std::cout << "总共捕获了" << count << "帧" << " 平均每帧耗时：" << (std::chrono::system_clock::now() - before_while) / 1ms / count << "ms" << std::endl;

    return 0;
}
