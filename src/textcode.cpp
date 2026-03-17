#include "code.h"
#include "ffmpeg.h"

#include <climits>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    void PrintUsage()
    {
        std::cout
            << "Usage:\n"
            << "  textcode <input_file> <output_video> [duration_ms] [fps]\n";
    }

    bool ReadBinaryFile(const fs::path& path, std::vector<char>& data)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            return false;
        }

        input.seekg(0, std::ios::end);
        const std::streamoff size = input.tellg();
        input.seekg(0, std::ios::beg);
        if (size < 0)
        {
            return false;
        }

        data.resize(static_cast<std::size_t>(size));
        if (!data.empty())
        {
            input.read(data.data(), size);
        }
        return static_cast<bool>(input) || data.empty();
    }
}

int main(int argc, char* argv[])
{
    if (argc < 3 || argc > 5)
    {
        PrintUsage();
        return 1;
    }

    const fs::path inputPath = argv[1];
    const fs::path outputVideo = argv[2];
    const int durationMs = (argc >= 4) ? std::stoi(argv[3]) : 10000;
    const int fps = (argc == 5) ? std::stoi(argv[4]) : 15;
    if (durationMs <= 0 || fps <= 0)
    {
        std::cerr << "error: duration_ms and fps must be positive\n";
        return 1;
    }

    std::vector<char> inputBytes;
    if (!ReadBinaryFile(inputPath, inputBytes))
    {
        std::cerr << "error: failed to open input file " << inputPath.string() << '\n';
        return 1;
    }

    std::error_code ec;
    if (outputVideo.has_parent_path())
    {
        fs::create_directories(outputVideo.parent_path(), ec);
        if (ec)
        {
            std::cerr << "error: failed to create output directory " << outputVideo.parent_path().string() << '\n';
            return 1;
        }
    }

    const fs::path frameDir = outputVideo.parent_path().empty()
        ? fs::path("textcode_frames")
        : (outputVideo.parent_path() / "textcode_frames");
    fs::remove_all(frameDir, ec);
    fs::create_directories(frameDir, ec);
    if (ec)
    {
        std::cerr << "error: failed to prepare temporary frame directory\n";
        return 1;
    }

    const int frameLimit = static_cast<int>(1LL * fps * durationMs / 1000);
    Code::Main(inputBytes.data(),
               static_cast<int>(inputBytes.size()),
               frameDir.string().c_str(),
               "png",
               (frameLimit > 0) ? frameLimit : INT_MAX);

    if (FFMPEG::ImagetoVideo(frameDir.string().c_str(), "png", outputVideo.string().c_str(), fps, fps, 100000) != 0)
    {
        std::cerr << "error: ffmpeg failed while generating video\n";
        return 1;
    }

    std::cout << "Encoded video written to " << outputVideo.string() << '\n';
    std::cout << "Temporary frames written to " << frameDir.string() << '\n';
    return 0;
}
