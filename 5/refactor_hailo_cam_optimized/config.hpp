#pragma once

inline constexpr auto FROM_FILE = false;

inline constexpr auto HEF_FILE = "/home/wjjsn/code/yolov8n.hef";
inline constexpr auto VIDEO_PATH = "/home/wjjsn/test.mp4";

inline constexpr auto VIDEO_DEVICE = "/dev/video0";
inline constexpr auto USE_V4L2 = true;

inline constexpr auto VIDEO_WIDTH = 1920;
inline constexpr auto VIDEO_HEIGHT = 1080;

static_assert(!(FROM_FILE && USE_V4L2), "V4L2 cannot be used with video file");
