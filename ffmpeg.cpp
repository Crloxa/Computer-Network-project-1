#include "ffmpeg.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace FFMPEG
{
    constexpr int MAXBUFLEN = 2048;

    // 可由外部通过 SetFfmpegPath / SetTmpPath 注入，默认为相对目录
    static std::string s_ffmpegPath = "./bin/"; // 请确保末尾带斜杠或由调用方设置正确
    static std::string s_tmpPath = "tmpdir";

    // 解耦入口：setter/getter
    void SetFfmpegPath(const char* path)
    {
        if (path && path[0]) {
            s_ffmpegPath = path;
            // 末尾保证有斜杠
            if (s_ffmpegPath.back() != '/' && s_ffmpegPath.back() != '\\')
                s_ffmpegPath.push_back('/');
        }
    }
    const char* GetFfmpegPath() { return s_ffmpegPath.c_str(); }

    void SetTmpPath(const char* path)
    {
        if (path && path[0]) {
            s_tmpPath = path;
        }
    }
    const char* GetTmpPath() { return s_tmpPath.c_str(); }

    // 执行命令的封装：集中处理命令引号与日志重定向（便于单测/调试）
    static int ExecCommand(const char* cmd)
    {
        // 这里保留原来的 system 调用，但统一入口，便于后续替换（如 popen）
        return system(cmd);
    }

    int VideotoImage(const char* videoPath,
        const char* imagePath,
        const char* imageFormat)
    {
        char BUF[MAXBUFLEN];

        // 创建目录（带引号以支持包含空格的路径）
        snprintf(BUF, MAXBUFLEN, "md \"%s\"", imagePath);
        ExecCommand(BUF);

        // 使用统一的 ffmpeg 路径拼接方式，整个可执行文件用引号包裹
        // 保持原有逻辑：解码时启用多线程，禁用 vsync 自动重复/丢帧，保留 q:v 控制质量
        snprintf(BUF, MAXBUFLEN,
            "\"%sffmpeg.exe\" -y -i \"%s\" -threads 0 -vsync 0 -q:v 2 -f image2 \"%s\\%%05d.%s\"",
            GetFfmpegPath(), videoPath, imagePath, imageFormat);

        return ExecCommand(BUF);
    }

    int ImagetoVideo(const char* imagePath,
        const char* imageFormat,
        const char* videoPath,
        unsigned rawFrameRates,
        unsigned outputFrameRates,
        unsigned kbps)
    {
        char BUF[MAXBUFLEN];

        // 使用 -framerate 指定输入帧率，使用快速 preset 与多线程，加上 yuv420p 保证解码兼容性
        if (kbps)
            snprintf(BUF, MAXBUFLEN,
                "\"%sffmpeg.exe\" -y -framerate %u -f image2 -i \"%s\\%%05d.%s\" -c:v libx264 -preset ultrafast -b:v %uK -threads 0 -pix_fmt yuv420p -r %u \"%s\"",
                GetFfmpegPath(), rawFrameRates, imagePath, imageFormat, kbps, outputFrameRates, videoPath);
        else
            snprintf(BUF, MAXBUFLEN,
                "\"%sffmpeg.exe\" -y -framerate %u -f image2 -i \"%s\\%%05d.%s\" -c:v libx264 -preset ultrafast -threads 0 -pix_fmt yuv420p -r %u \"%s\"",
                GetFfmpegPath(), rawFrameRates, imagePath, imageFormat, outputFrameRates, videoPath);

        return ExecCommand(BUF);
    }

    //缩放滤镜
    int ScaleImage(const char* inputPath,
                   const char* outputPath,
                   int width,
                   int height)
    {
        if (!inputPath || !outputPath) return -1;

        char BUF[MAXBUFLEN];
        snprintf(BUF, MAXBUFLEN,
            "\"%sffmpeg.exe\" -y -i \"%s\" -vf \"scale=%d:%d\" \"%s\"",
            GetFfmpegPath(), inputPath, width, height, outputPath);

        return ExecCommand(BUF);
    }

    int test(void)
    {
        // 保持原有逻辑，使用返回码判断（0 成功）
        const char* tmp = GetTmpPath();
        int rc = VideotoImage("test.mp4", tmp, "png");
        if (rc != 0) {
            return rc;
        }
        rc = ImagetoVideo(tmp, "png", "out.mp4", 30, 30, 0);
        return rc;
    }
}
