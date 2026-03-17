#include "ffmpeg.h"
#include <string>

#define MAXBUFLEN 2048

namespace FFMPEG
{
    // 可由外部通过 SetFfmpegPath / SetTmpPath 注入，默认为相对目录
    static std::string s_ffmpegPath = "ffmpeg\\bin\\";
    static std::string s_tmpPath = "tmpdir";

    // 解耦入口：setter/getter
    void SetFfmpegPath(const char* path)
    {
        if (path && path[0]) {
            s_ffmpegPath = path;
            if (s_ffmpegPath.back() != '/' && s_ffmpegPath.back() != '\\')
                s_ffmpegPath.push_back('\\');
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

    // 执行命令的封装
    static int ExecCommand(const char* cmd)
    {
        printf("[EXEC] %s\n", cmd);
        return system(cmd);
    }

    int VideotoImage(const char* videoPath,
        const char* imagePath,
        const char* imageFormat,
        int width,
        int height)
    {
        char BUF[MAXBUFLEN];

        // 创建目录
        snprintf(BUF, MAXBUFLEN, "mkdir ""%s"" 2>nul", imagePath);
        ExecCommand(BUF);

        // 构建 ffmpeg 命令
        if (width > 0 && height > 0) {
            // 带分辨率调整
            snprintf(BUF, MAXBUFLEN,
                """%sffmpeg.exe"" -y -i ""%s"" -threads 0 -vsync 0 -q:v 2 -vf ""scale=%d:%d"" -f image2 ""%s\\%%05d.%s""",
                GetFfmpegPath(), videoPath, width, height, imagePath, imageFormat);
        } else {
            // 保持原始分辨率
            snprintf(BUF, MAXBUFLEN,
                """%sffmpeg.exe"" -y -i ""%s"" -threads 0 -vsync 0 -q:v 2 -f image2 ""%s\\%%05d.%s""",
                GetFfmpegPath(), videoPath, imagePath, imageFormat);
        }

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

        if (kbps)
            snprintf(BUF, MAXBUFLEN,
                """%sffmpeg.exe"" -y -framerate %u -f image2 -i ""%s\\%%05d.%s"" -c:v libx264 -preset ultrafast -b:v %uK -threads 0 -pix_fmt yuv420p -r %u ""%s""",
                GetFfmpegPath(), rawFrameRates, imagePath, imageFormat, kbps, outputFrameRates, videoPath);
        else
            snprintf(BUF, MAXBUFLEN,
                """%sffmpeg.exe"" -y -framerate %u -f image2 -i ""%s\\%%05d.%s"" -c:v libx264 -preset ultrafast -threads 0 -pix_fmt yuv420p -r %u ""%s""",
                GetFfmpegPath(), rawFrameRates, imagePath, imageFormat, outputFrameRates, videoPath);

        return ExecCommand(BUF);
    }

    int ScaleImage(const char* inputPath,
                   const char* outputPath,
                   int width,
                   int height)
    {
        if (!inputPath || !outputPath) return -1;

        char BUF[MAXBUFLEN];
        snprintf(BUF, MAXBUFLEN,
            """%sffmpeg.exe"" -y -i ""%s"" -vf ""scale=%d:%d:force_original_aspect_ratio=decrease,pad=%d:%d:(ow-iw)/2:(oh-ih)/2"" -frames:v 1 -update 1 ""%s""",
            GetFfmpegPath(), inputPath, width, height, width, height, outputPath);

        return ExecCommand(BUF);
    }

    int test(void)
    {
        const char* tmp = GetTmpPath();
        int rc = VideotoImage("test.mp4", tmp, "png", -1, -1);
        if (rc != 0) {
            return rc;
        }
        rc = ImagetoVideo(tmp, "png", "out.mp4", 30, 30, 0);
        return rc;
    }
}
