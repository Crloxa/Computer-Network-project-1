// Config.h
#pragma once
#include <string>

namespace Config {
    // 视频配置
    namespace Video {
        constexpr int DEFAULT_CRF = 23;
        constexpr const char* INPUT_DIR = "input";
        constexpr const char* OUTPUT_DIR = "output";
        constexpr const char* LOG_DIR = "log";
        constexpr const char* FFMPEG_EXE = "ffmpeg.exe";
        constexpr const char* DEFAULT_VIDEO_CODEC = "libx264";
        constexpr const char* DEFAULT_AUDIO_CODEC = "aac";
        constexpr bool ENABLE_LOG = true;
        constexpr const char* LOG_FILE = "log/transcode_log.txt";
        constexpr int VideoFps = 15;
    }

    // QR配置（与参考实现一致）
    namespace QRCode {
        constexpr int FrameSize = 133;
        constexpr int FrameOutputRate = 8;
        constexpr int OutputFrame_W = FrameSize * FrameOutputRate;
        constexpr int OutputFrame_H = FrameSize * FrameOutputRate;
        constexpr int BytesPerFrame = 1878; // 标准载荷容量
        constexpr const char* TEMP_DIR = "./input/temp";

        constexpr int SAFE_AREA_WIDTH = 2;
        constexpr int POS_MARKER_SIZE = 21;
        constexpr int POS_SMALL_MARKER_SIZE = 7;
        constexpr int HEADER_TOP = 3;
        constexpr int HEADER_LEFT = 21;
        constexpr int HEADER_WIDTH = 16; // 与参考实现一致
        constexpr int HEADER_HEIGHT = 3;

        enum class FrameType {
            Start = 0,
            End = 1,
            StartAndEnd = 2,
            Normal = 3
        };
    }
}