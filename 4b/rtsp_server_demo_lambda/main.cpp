#include <cstdlib>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/app/app.h>
#include <opencv2/opencv.hpp>
#include <thread>
#include <iostream>
#include <atomic>

struct CaptureContext {
    GstElement *appsrc;
    std::atomic<bool> running;
    std::thread thread;
};
using namespace std;

// 推流线程
void capture_thread(CaptureContext *ctx)
{
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "无法打开摄像头\n";
        return; // 不要在子线程里 exit(-1)，不然整个进程会直接退出
    }

    cv::Mat frame;
    while (ctx->running) {
        cap >> frame;
        if (frame.empty())
            continue;

        int size = frame.total() * frame.elemSize();

        GstBuffer *buffer = gst_buffer_new_allocate(NULL, size, NULL);
        GstMapInfo map;
        gst_buffer_map(buffer, &map, GST_MAP_WRITE);
        memcpy(map.data, frame.data, size);
        gst_buffer_unmap(buffer, &map);

        GST_BUFFER_PTS(buffer) = gst_util_uint64_scale(g_get_monotonic_time(), 1, G_TIME_SPAN_SECOND);
        GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale(1, 30, G_TIME_SPAN_SECOND);

        gst_app_src_push_buffer(GST_APP_SRC(ctx->appsrc), buffer);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    cap.release();
    gst_app_src_end_of_stream(GST_APP_SRC(ctx->appsrc));
    gst_object_unref(ctx->appsrc); // 用完记得 unref
}


int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    GstRTSPServer *server = gst_rtsp_server_new();
    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);

    GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();

    gst_rtsp_media_factory_set_launch(factory,
                                      "( appsrc name=mysrc is-live=true format=time "
                                      "   caps=video/x-raw,format=BGR,width=640,height=480,framerate=30/1 "
                                      " ! videoconvert "
                                      " ! x264enc tune=zerolatency bitrate=2000 speed-preset=ultrafast "
                                      " ! rtph264pay name=pay0 pt=96 )");

    g_signal_connect(factory, "media-configure",
                     (GCallback)(+[](GstRTSPMedia *factory_media, GstRTSPMedia *media, gpointer user_data) {
                         (void)factory_media;
                         (void)user_data;

                         GstElement *pipeline = gst_rtsp_media_get_element(media);
                         GstElement *appsrc = gst_bin_get_by_name(GST_BIN(pipeline), "mysrc");

                         auto ctx = new CaptureContext;
                         ctx->appsrc = appsrc; // 持有一个引用
                         ctx->running = true;
                         ctx->thread = std::thread(capture_thread, ctx);

                         // 当 media 被 unprepared（客户端断开）时，通知线程停
                         g_signal_connect(media, "unprepared",
                                          (GCallback)(+[](GstRTSPMedia *media, gpointer user_data) {
                                              (void)media;
                                              auto ctx = static_cast<CaptureContext *>(user_data);
                                              ctx->running = false; // 通知采集线程退出
                                              ctx->thread.join(); // 等待线程结束
                                          }),
                                          ctx);

                         // 让 media 销毁时顺便 delete ctx（可选，避免内存泄漏）
                         g_object_set_data_full(G_OBJECT(media), "capture-context", ctx, (GDestroyNotify)[](gpointer data) { 
                            delete static_cast<CaptureContext *>(data); });
                     }),
                     NULL);

    gst_rtsp_mount_points_add_factory(mounts, "/stream", factory);
    g_object_unref(mounts);

    gst_rtsp_server_attach(server, NULL);

    std::cout << "RTSP server 已启动： rtsp://raspberrypi.local:8554/stream\n";

    g_main_loop_run(loop);

    return 0;
}
