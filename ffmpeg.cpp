#include "ffmpeg.h"

namespace FFMPEG
{
    constexpr int MAXBUFLEN = 1024;
    const char ffmpegPath[] = "ffmpeg\\bin\\";
    const char tmpPath[] = "tmpdir";

    int ImagetoVideo(const char* imagePath,
        const char* imageFormat,
        const char* videoPath,
        unsigned rawFrameRates,
        unsigned outputFrameRates,
        unsigned kbps)
    {
        char buf[MAXBUFLEN];
        if (kbps)
        {
            std::snprintf(buf, MAXBUFLEN,
                "%sffmpeg.exe -y -framerate %u -f image2 -i \"%s\\%%05d.%s\" "
                "-b:v %uK -vcodec libx264 -r %u \"%s\"",
                ffmpegPath, rawFrameRates, imagePath, imageFormat, kbps, outputFrameRates, videoPath);
        }
        else
        {
            std::snprintf(buf, MAXBUFLEN,
                "%sffmpeg.exe -y -framerate %u -f image2 -i \"%s\\%%05d.%s\" "
                "-vcodec libx264 -r %u \"%s\"",
                ffmpegPath, rawFrameRates, imagePath, imageFormat, outputFrameRates, videoPath);
        }
        return std::system(buf);
    }

    int VideotoImage(const char* videoPath,
        const char* imagePath,
        const char* imageFormat)
    {
        char buf[MAXBUFLEN];
        std::snprintf(buf, MAXBUFLEN, "md \"%s\"", imagePath);
        std::system(buf);
        std::snprintf(buf, MAXBUFLEN,
            "%sffmpeg.exe -y -i \"%s\" -q:v 2 -f image2 \"%s\\%%05d.%s\"",
            ffmpegPath, videoPath, imagePath, imageFormat);
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
