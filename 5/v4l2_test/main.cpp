#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
// #include <chrono>
// #include <iostream>
#define DEVICE "/dev/video0"
#define WIDTH  1920
#define HEIGHT 1080

struct buffer {
    void *start;
    size_t length;
};

int main()
{
    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    // -------------------------------
    // 查询设备能力
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        perror("VIDIOC_QUERYCAP");
        return 1;
    }
    printf("Driver: %s\n", cap.driver);

    // -------------------------------
    // 设置视频格式
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG; // 常用格式
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("VIDIOC_S_FMT");
        return 1;
    }

    // -------------------------------
    // 请求缓冲区（mmap）
    struct v4l2_requestbuffers req {};

    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("VIDIOC_REQBUFS");
        return 1;
    }

    struct buffer *buffers = static_cast<struct buffer *>(calloc(req.count, sizeof(*buffers)));

    // mmap 每个 buffer
    for (size_t i = 0; i < req.count; i++) {
        struct v4l2_buffer buf {};

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("VIDIOC_QUERYBUF");
            return 1;
        }

        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL, buf.length,
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED,
                                fd, buf.m.offset);

        if (buffers[i].start == MAP_FAILED) {
            perror("mmap");
            return 1;
        }

        // 将 buffer 放入队列
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            perror("VIDIOC_QBUF");
            return 1;
        }
    }

    // -------------------------------
    // 开始流
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        perror("VIDIOC_STREAMON");
        return 1;
    }

    printf("=== Start capturing ===\n");

    // -------------------------------
    // 主循环：采集 100 帧作为示例
    for (int i = 0; i < 100; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        // 取出一个 buffer
        // auto now=std::chrono::system_clock::now();
        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
            perror("VIDIOC_DQBUF");
            return 1;
        }
        // using namespace std::chrono_literals;
        // std::cout<<"获取一帧耗时"<<(std::chrono::system_clock::now()-now)/1ms<<"ms"<<std::endl;
        // 此时 buffers[buf.index].start 里就是一帧图像！
        printf("Frame %d captured, %u bytes\n", i, buf.bytesused);
        fflush(stdout);
        // TODO：你可以在这里处理图像，例如保存成文件或转成 RGB

        // 放回队列
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            perror("VIDIOC_QBUF");
            return 1;
        }
    }

    // -------------------------------
    // 停止流
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
        perror("VIDIOC_STREAMOFF");
        return 1;
    }

    // 释放缓冲区
    for (size_t i = 0; i < req.count; i++) {
        munmap(buffers[i].start, buffers[i].length);
    }
    free(buffers);
    close(fd);

    return 0;
}
