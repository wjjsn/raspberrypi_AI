#include "thread_safe_queue.hpp"
#include "opencv2/opencv.hpp"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#include "config.hpp"

extern thread_safe_queue<cv::Mat> g_capture_queue;

extern std::atomic<bool> g_stop_requested;
extern std::atomic<bool> g_v4l2_requeue;

using namespace std::chrono_literals;

struct buffer {
    void *start;
    size_t length;
};

void capture_thread()
{
    if (USE_V4L2) {
        int fd = open(VIDEO_DEVICE, O_RDWR);
        if (fd < 0) {
            perror("open");
            exit(-1);
        }

        // -------------------------------
        // 查询设备能力
        struct v4l2_capability cap;
        if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
            perror("VIDIOC_QUERYCAP");
            exit(-1);
        }
        printf("Driver: %s\n", cap.driver);

        // -------------------------------
        // 设置视频格式
        struct v4l2_format fmt {};

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = 1920;
        fmt.fmt.pix.height = 1080;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG; // 常用格式
        fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

        if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
            perror("VIDIOC_S_FMT");
            exit(-1);
        }

        // -------------------------------
        // 请求缓冲区（mmap）
        struct v4l2_requestbuffers req {};

        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
            perror("VIDIOC_REQBUFS");
            exit(-1);
        }

        auto buffers = new struct buffer[req.count];

        // mmap 每个 buffer
        for (size_t i = 0; i < req.count; i++) {
            struct v4l2_buffer buf {};

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
                perror("VIDIOC_QUERYBUF");
                exit(-1);
            }

            buffers[i].length = buf.length;
            buffers[i].start = mmap(NULL, buf.length,
                                    PROT_READ | PROT_WRITE,
                                    MAP_SHARED,
                                    fd, buf.m.offset);

            if (buffers[i].start == MAP_FAILED) {
                perror("mmap");
                exit(-1);
            }

            // 将 buffer 放入队列
            if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
                perror("VIDIOC_QBUF");
                exit(-1);
            }
        }

        // -------------------------------
        // 开始流
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
            perror("VIDIOC_STREAMON");
            exit(-1);
        }

        printf("=== Start capturing ===\n");

        // -------------------------------
        // 主循环
        while (!g_stop_requested) {
            struct v4l2_buffer buf {};

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            // 取出一个 buffer
            auto cap_start = std::chrono::system_clock::now();
            if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
                perror("VIDIOC_DQBUF");
                exit(-1);
            }
            auto cap_time = std::chrono::system_clock::now();

            cv::Mat frame(1, buffers[buf.index].length, CV_8UC1, buffers[buf.index].start);
            frame = cv::imdecode(frame, cv::IMREAD_COLOR);
            cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
            g_capture_queue.push(frame);
            std::cout << "从V4L2设备取帧耗时" << (cap_time - cap_start) / 1ms << "ms" << "\n";
            g_v4l2_requeue.store(false);
            g_v4l2_requeue.wait(false);
            // 放回队列
            if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
                perror("VIDIOC_QBUF");
                exit(-1);
            }
        }

        // -------------------------------
        // 停止流
        if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
            perror("VIDIOC_STREAMOFF");
            exit(-1);
        }

        // 释放缓冲区
        for (size_t i = 0; i < req.count; i++) {
            munmap(buffers[i].start, buffers[i].length);
        }
        free(buffers);
        close(fd);

    } else {
        cv::VideoCapture cap{};

        if constexpr (FROM_FILE) {
            cap = cv::VideoCapture(VIDEO_PATH);
        } else {
            // cap = cv::VideoCapture(VIDEO_DEVICE);
            cap = cv::VideoCapture("libcamerasrc ! video/x-raw,width=1920,height=1080,framerate=30/1,format=NV12 "
                                   "! appsink max-buffers=1 drop=true sync=false",
                                   cv::CAP_GSTREAMER);
            // cap.set(cv::CAP_PROP_FRAME_WIDTH, 1920);
            // cap.set(cv::CAP_PROP_FRAME_HEIGHT, 1080);
            // cap.set(cv::CAP_PROP_FPS, 30);
            // cap.set(cv::CAP_PROP_BUFFERSIZE, 10);
        }

        if (!cap.isOpened()) {
            std::cerr << "Failed to open camera " << std::endl;
            g_stop_requested = true;
        }
        while (!g_stop_requested) {
            cv::Mat frame;
            cap >> frame;
            cv::cvtColor(frame, frame, cv::COLOR_YUV2BGR_NV12);
            if (frame.empty()) {
                std::cout << "End of video file" << std::endl;
                g_stop_requested = true;
                break;
            }
            // cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
            g_capture_queue.push(frame);
            std::cout << "cap队列" << g_capture_queue.queue_.size() << std::endl;
        }
        cap.release();
    }
}