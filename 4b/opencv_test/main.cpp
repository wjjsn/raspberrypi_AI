#include <format>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <thread>
using namespace std::chrono_literals;

int main()
{
    cv::VideoCapture cap(0); // 打开默认摄像头

    if (!cap.isOpened()) {
        std::cerr << "无法打开摄像头" << std::endl;
        return -1;
    }

    cv::Mat frame;
	auto count=0;
    while (true) {
        cap >> frame;
        if (frame.empty()) {
            std::cerr << "无法读取摄像头图像" << std::endl;
            break;
        }

        cv::imwrite(std::format("test{}.jpg",count), frame);

        std::this_thread::sleep_for(1s); 
        count++;
    }

    return 0;
}
