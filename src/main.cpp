#include "ImgDecode.h"
#include "code.h"
#include "ffmpeg.h"
#include "pic.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    void PrintUsage()
    {
        std::cout
            << "Usage:\n"
            << "  encode <input_file> <output_video> <max_duration_ms>\n"
            << "  decode <input_video> <output_file> <validity_file>\n"
            << "  Project1 encode <input_file> <output_dir> [output_format] [frame_limit]\n"
            << "  Project1 encode-video <input_file> <output_video> [duration_ms] [fps]\n"
            << "  Project1 decode-image <input_image> <output_file>\n"
            << "  Project1 decode-dir <input_dir> <output_file>\n"
            << "  Project1 decode - video <input_video> <output_file>[validity_file]\n";
    }

    int ParsePositiveInt(const char* value)
    {
        try
        {
            return std::stoi(value);
        }
        catch (const std::exception&)
        {
            std::cerr << "error: invalid integer argument '" << value << "'\n";
            return -1;
        }
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

    bool WriteBinaryFile(const fs::path& path, const std::vector<unsigned char>& data)
    {
        std::error_code ec;
        if (path.has_parent_path())
        {
            fs::create_directories(path.parent_path(), ec);
            if (ec)
            {
                return false;
            }
        }

        std::ofstream output(path, std::ios::binary);
        if (!output)
        {
            return false;
        }
        if (!data.empty())
        {
            output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        }
        return static_cast<bool>(output);
    }

    bool IsImageFile(const fs::path& path)
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp";
    }

    bool DecodeFrameImage(const fs::path& imagePath, ImageDecode::ImageInfo& imageInfo)
    {
        cv::Mat srcImg = cv::imread(imagePath.string(), cv::IMREAD_COLOR);
        if (srcImg.empty())
        {
            return false;
        }

        cv::Mat logicalFrame;
        if (ImgParse::Main(srcImg, logicalFrame))
        {
            return false;
        }

        return !ImageDecode::Main(logicalFrame, imageInfo);
    }

    bool DecodeImageSequence(const std::vector<fs::path>& imagePaths, const fs::path& outputFile)
    {
        int previousFrame = -1;
        bool hasStarted = false;
        bool hasEnded = false;
        std::vector<unsigned char> outputBytes;

        for (const fs::path& imagePath : imagePaths)
        {
            ImageDecode::ImageInfo imageInfo;
            if (!DecodeFrameImage(imagePath, imageInfo))
            {
                continue;
            }

            if (!hasStarted)
            {
                if (!imageInfo.IsStart)
                {
                    continue;
                }
                hasStarted = true;
            }

            if (previousFrame == imageInfo.FrameBase)
            {
                continue;
            }

            if (previousFrame != -1)
            {
                const int expectedFrame = (previousFrame + 1) & UINT16_MAX;
                if (expectedFrame != imageInfo.FrameBase)
                {
                    std::cerr << "error: skipped frame detected near " << imagePath.string() << '\n';
                    return false;
                }
            }

            previousFrame = imageInfo.FrameBase;
            outputBytes.insert(outputBytes.end(), imageInfo.Info.begin(), imageInfo.Info.end());
            std::cout << "Decoded frame " << imageInfo.FrameBase << " from " << imagePath.filename().string() << '\n';

            if (imageInfo.IsEnd)
            {
                hasEnded = true;
                break;
            }
        }

        if (!hasStarted)
        {
            std::cerr << "error: no start frame found\n";
            return false;
        }
        if (!hasEnded)
        {
            std::cerr << "error: no end frame found\n";
            return false;
        }
        if (!WriteBinaryFile(outputFile, outputBytes))
        {
            std::cerr << "error: failed to write output file " << outputFile.string() << '\n';
            return false;
        }
        return true;
    }

    int EncodeToDirectory(const char* inputFile, const char* outputDir, const char* outputFormat, int frameLimit)
    {
        std::vector<char> inputBytes;
        if (!ReadBinaryFile(inputFile, inputBytes))
        {
            std::cerr << "error: failed to open input file " << inputFile << '\n';
            return 1;
        }

        std::error_code ec;
        fs::create_directories(outputDir, ec);
        if (ec)
        {
            std::cerr << "error: failed to create output directory " << outputDir << '\n';
            return 1;
        }

        Code::Main(inputBytes.data(), static_cast<int>(inputBytes.size()), outputDir, outputFormat, frameLimit);
        std::cout << "Encoded frames written to " << outputDir << '\n';
        return 0;
    }

    int EncodeToVideo(const char* inputFile, const char* videoPath, int durationMs, int fps)
    {
        std::vector<char> inputBytes;
        if (!ReadBinaryFile(inputFile, inputBytes))
        {
            std::cerr << "error: failed to open input file " << inputFile << '\n';
            return 1;
        }

        const fs::path frameDir = "outputImg";
        std::error_code ec;
        fs::remove_all(frameDir, ec);
        fs::create_directories(frameDir, ec);
        if (ec)
        {
            std::cerr << "error: failed to prepare frame directory\n";
            return 1;
        }

        Code::Main(inputBytes.data(),
            static_cast<int>(inputBytes.size()),
            frameDir.string().c_str(),
            "png",
            static_cast<int>(1LL * fps * durationMs / 1000));

        if (FFMPEG::ImagetoVideo(frameDir.string().c_str(), "png", videoPath, fps, 60, 100000) != 0)
        {
            std::cerr << "error: ffmpeg failed while building video\n";
            return 1;
        }

        std::cout << "Encoded video written to " << videoPath << '\n';
        return 0;
    }

    int DecodeOneImage(const char* inputImage, const char* outputFile)
    {
        ImageDecode::ImageInfo imageInfo;
        if (!DecodeFrameImage(inputImage, imageInfo))
        {
            std::cerr << "error: failed to decode image " << inputImage << '\n';
            return 1;
        }

        if (!WriteBinaryFile(outputFile, imageInfo.Info))
        {
            std::cerr << "error: failed to write output file " << outputFile << '\n';
            return 1;
        }

        std::cout << "Decoded frame " << imageInfo.FrameBase << " into " << outputFile << '\n';
        return 0;
    }

    int DecodeDirectory(const char* inputDir, const char* outputFile)
    {
        std::vector<fs::path> imagePaths;
        for (const auto& entry : fs::directory_iterator(inputDir))
        {
            if (entry.is_regular_file() && IsImageFile(entry.path()))
            {
                imagePaths.push_back(entry.path());
            }
        }
        std::sort(imagePaths.begin(), imagePaths.end());

        if (imagePaths.empty())
        {
            std::cerr << "error: no image frames found in " << inputDir << '\n';
            return 1;
        }

        return DecodeImageSequence(imagePaths, outputFile) ? 0 : 1;
    }

    // 修复版：先完成抽帧，再统一扫描目录，避免并发下漏帧/乱序
    bool ExtractVideoFrames(const char* inputVideo, const fs::path& frameDir,
        std::vector<fs::path>& imagePaths)
    {
        std::error_code ec;
        fs::remove_all(frameDir, ec);
        fs::create_directories(frameDir, ec);
        if (ec)
        {
            std::cerr << "error: failed to prepare temporary frame directory\n";
            return false;
        }

        std::atomic<bool> extractionDone{ false };
        std::atomic<int> extractionResult{ 0 };

        std::thread extractor([&]()
            {
                const int ret = FFMPEG::VideotoImage(inputVideo, frameDir.string().c_str(), "png");
                extractionResult.store(ret, std::memory_order_release);
                extractionDone.store(true, std::memory_order_release);
            });

        extractor.join();

        if (!extractionDone.load(std::memory_order_acquire))
        {
            std::cerr << "error: extractor thread finished unexpectedly\n";
            return false;
        }
        if (extractionResult.load(std::memory_order_acquire) != 0)
        {
            std::cerr << "error: ffmpeg failed while extracting video frames\n";
            return false;
        }

        imagePaths.clear();
        for (const auto& entry : fs::directory_iterator(frameDir))
        {
            if (entry.is_regular_file() && IsImageFile(entry.path()))
            {
                imagePaths.push_back(entry.path());
            }
        }
        std::sort(imagePaths.begin(), imagePaths.end());

        return true;
    }

    int DecodeVideo(const char* inputVideo, const char* outputFile)
    {
        const fs::path frameDir = "inputImg";
        std::vector<fs::path> imagePaths;
        if (!ExtractVideoFrames(inputVideo, frameDir, imagePaths))
        {
            return 1;
        }
        if (imagePaths.empty())
        {
            std::cerr << "error: no frames extracted from video\n";
            return 1;
        }

        return DecodeImageSequence(imagePaths, outputFile) ? 0 : 1;
    }

    int DecodeVideoWithValidity(const char* inputVideo,
        const char* outputFile,
        const char* validityFile)
    {
        const fs::path frameDir = "inputImg_decode";
        std::vector<fs::path> imagePaths;
        if (!ExtractVideoFrames(inputVideo, frameDir, imagePaths))
        {
            return 1;
        }
        if (imagePaths.empty())
        {
            std::cerr << "error: no frames extracted from video\n";
            return 1;
        }

        std::map<uint16_t, ImageDecode::ImageInfo> frameMap;
        uint16_t startFrameNo = 0;
        uint16_t endFrameNo = 0;
        bool foundStart = false;
        bool foundEnd = false;

        for (const fs::path& imagePath : imagePaths)
        {
            ImageDecode::ImageInfo imageInfo;
            if (!DecodeFrameImage(imagePath, imageInfo))
            {
                continue;
            }

            const uint16_t fn = imageInfo.FrameBase;
            if (frameMap.count(fn) != 0)
            {
                continue;
            }

            if (imageInfo.IsStart)
            {
                startFrameNo = fn;
                foundStart = true;
            }
            if (imageInfo.IsEnd)
            {
                endFrameNo = fn;
                foundEnd = true;
            }

            frameMap[fn] = std::move(imageInfo);
            std::cout << "Decoded frame " << fn
                << " from " << imagePath.filename().string() << '\n';
        }

        if (!foundStart)
        {
            std::cerr << "warning: no start frame found; writing empty output\n";
            const std::vector<unsigned char> empty;
            WriteBinaryFile(outputFile, empty);
            WriteBinaryFile(validityFile, empty);
            return 0;
        }

        if (!foundEnd)
        {
            std::cerr << "warning: no end frame found; output may be incomplete\n";
            endFrameNo = startFrameNo;
            for (const auto& kv : frameMap)
            {
                const uint16_t fn = kv.first;
                const int dist = static_cast<int>(
                    (static_cast<uint32_t>(fn) - startFrameNo + 65536u) % 65536u);
                const int endDist = static_cast<int>(
                    (static_cast<uint32_t>(endFrameNo) - startFrameNo + 65536u) % 65536u);
                if (dist > endDist)
                {
                    endFrameNo = fn;
                }
            }
        }

        const uint32_t totalFrames =
            (static_cast<uint32_t>(endFrameNo) - startFrameNo + 65536u) % 65536u + 1u;

        std::vector<unsigned char> outputBytes;
        std::vector<unsigned char> validityBytes;
        outputBytes.reserve(totalFrames * static_cast<uint32_t>(ImageDecode::BytesPerFrame));
        validityBytes.reserve(outputBytes.capacity());

        for (uint32_t i = 0; i < totalFrames; ++i)
        {
            const uint16_t fn = static_cast<uint16_t>((startFrameNo + i) % 65536u);
            const auto it = frameMap.find(fn);
            if (it != frameMap.end())
            {
                const auto& info = it->second.Info;
                outputBytes.insert(outputBytes.end(), info.begin(), info.end());
                validityBytes.insert(validityBytes.end(), info.size(), 0xFF);
            }
            else
            {
                const std::size_t fillLen =
                    static_cast<std::size_t>(ImageDecode::BytesPerFrame);
                outputBytes.insert(outputBytes.end(), fillLen, 0x00);
                validityBytes.insert(validityBytes.end(), fillLen, 0x00);
                std::cerr << "warning: frame " << fn << " missing; filled with zeros\n";
            }
        }

        if (!WriteBinaryFile(outputFile, outputBytes))
        {
            std::cerr << "error: failed to write output file " << outputFile << '\n';
            return 1;
        }
        if (!WriteBinaryFile(validityFile, validityBytes))
        {
            std::cerr << "error: failed to write validity file " << validityFile << '\n';
            return 1;
        }

        std::cout << "Decoded " << outputBytes.size() << " bytes into " << outputFile << '\n';
        std::cout << "Validity markers written to " << validityFile << '\n';
        return 0;
    }

    int RunEncoder(const char* inputFile, const char* outputVideo, int durationMs)
    {
        constexpr int kDefaultFps = 15;
        return EncodeToVideo(inputFile, outputVideo, durationMs, kDefaultFps);
    }

    int RunDecoder(const char* inputVideo, const char* outputFile, const char* validityFile)
    {
        return DecodeVideoWithValidity(inputVideo, outputFile, validityFile);
    }
}

int main(int argc, char* argv[])
{
    const std::string exeName = std::filesystem::path(argv[0]).stem().string();
    if (exeName == "encode")
    {
        if (argc != 4)
        {
            std::cerr << "Usage: encode <input_file> <output_video> <max_duration_ms>\n";
            return 1;
        }
        const int durationMs = ParsePositiveInt(argv[3]);
        if (durationMs <= 0)
        {
            std::cerr << "error: max_duration_ms must be a positive integer\n";
            return 1;
        }
        return RunEncoder(argv[1], argv[2], durationMs);
    }
    if (exeName == "decode")
    {
        if (argc != 4)
        {
            std::cerr << "Usage: decode <input_video> <output_file> <validity_file>\n";
            return 1;
        }
        return RunDecoder(argv[1], argv[2], argv[3]);
    }

    if (argc < 2)
    {
        PrintUsage();
        return 1;
    }

    const std::string command = argv[1];
    if (command == "encode")
    {
        if (argc < 4 || argc > 6)
        {
            PrintUsage();
            return 1;
        }
        const char* format = (argc >= 5) ? argv[4] : "png";
        const int frameLimit = (argc == 6) ? ParsePositiveInt(argv[5]) : INT_MAX;
        return EncodeToDirectory(argv[2], argv[3], format, frameLimit);
    }

    if (command == "encode-video")
    {
        if (argc < 4 || argc > 6)
        {
            PrintUsage();
            return 1;
        }
        const int durationMs = (argc >= 5) ? ParsePositiveInt(argv[4]) : 10000;
        const int fps = (argc == 6) ? ParsePositiveInt(argv[5]) : 15;
        return EncodeToVideo(argv[2], argv[3], durationMs, fps);
    }

    if (command == "decode-image")
    {
        if (argc != 4)
        {
            PrintUsage();
            return 1;
        }
        return DecodeOneImage(argv[2], argv[3]);
    }

    if (command == "decode-dir")
    {
        if (argc != 4)
        {
            PrintUsage();
            return 1;
        }
        return DecodeDirectory(argv[2], argv[3]);
    }

    if (command == "decode-video")
    {
        if (argc == 4)
        {
            return DecodeVideo(argv[2], argv[3]);
        }
        if (argc == 5)
        {
            return DecodeVideoWithValidity(argv[2], argv[3], argv[4]);
        }
        PrintUsage();
        return 1;
    }

    if (command == "decode-video-v")
    {
        if (argc != 5)
        {
            PrintUsage();
            return 1;
        }
        return DecodeVideoWithValidity(argv[2], argv[3], argv[4]);
    }

    PrintUsage();
    return 1;
}
