#include "ffmpeg.h"
#include <filesystem>
#include <string>

namespace FFMPEG
{
    constexpr int MAXBUFLEN = 1024;
    const char tmpPath[] = "tmpdir";

#ifdef _WIN32
    const char ffmpegCmd[] = "ffmpeg\\bin\\ffmpeg.exe";
#else
    const char ffmpegCmd[] = "ffmpeg";
#endif

    int ImagetoVideo(const char* imagePath,
        const char* imageFormat,
        const char* videoPath,
        unsigned rawFrameRates,
        unsigned outputFrameRates,
        unsigned kbps)
    {
        char buf[MAXBUFLEN];
        const std::filesystem::path inputPattern =
            std::filesystem::path(imagePath) / ("%05d." + std::string(imageFormat));
        if (kbps)
        {
            std::snprintf(buf, MAXBUFLEN,
                "%s -y -framerate %u -f image2 -i \"%s\" "
                "-b:v %uK -vcodec libx264 -r %u \"%s\"",
                ffmpegCmd, rawFrameRates, inputPattern.string().c_str(), kbps, outputFrameRates, videoPath);
        }
        else
        {
            std::snprintf(buf, MAXBUFLEN,
                "%s -y -framerate %u -f image2 -i \"%s\" "
                "-vcodec libx264 -r %u \"%s\"",
                ffmpegCmd, rawFrameRates, inputPattern.string().c_str(), outputFrameRates, videoPath);
        }
        return std::system(buf);
    }

    int VideotoImage(const char* videoPath,
        const char* imagePath,
        const char* imageFormat)
    {
        char buf[MAXBUFLEN];
        std::filesystem::create_directories(imagePath);
        const std::filesystem::path outputPattern =
            std::filesystem::path(imagePath) / ("%05d." + std::string(imageFormat));
        std::snprintf(buf, MAXBUFLEN,
            "%s -y -i \"%s\" -q:v 2 -f image2 \"%s\"",
            ffmpegCmd, videoPath, outputPattern.string().c_str());
        return std::system(buf);
    }

    int test(void)
    {
        bool tag = VideotoImage("test.mp4", tmpPath, "png");
        if (tag)
        {
            return tag;
        }
        tag = ImagetoVideo(tmpPath, "png", "out.mp4", 30, 30);
        return tag;
    }
}
