#include "ImgDecode.h"
#include "code.h"
#include "ffmpeg.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
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
            << "  Project1NoPic encode <input_file> <output_dir> [output_format] [frame_limit]\n"
            << "  Project1NoPic encode-video <input_file> <output_video> [duration_ms] [fps]\n"
            << "  Project1NoPic decode-image <input_image> <output_file>\n"
            << "  Project1NoPic decode-dir <input_dir> <output_file>\n"
            << "  Project1NoPic decode-video <input_video> <output_file>\n";
    }

    int ParsePositiveInt(const char* value)
    {
        return std::stoi(value);
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
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
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
        if (srcImg.rows == ImageDecode::FrameSize && srcImg.cols == ImageDecode::FrameSize)
        {
            logicalFrame = srcImg;
        }
        else
        {
            cv::resize(
                srcImg,
                logicalFrame,
                cv::Size(ImageDecode::FrameSize, ImageDecode::FrameSize),
                0.0,
                0.0,
                cv::INTER_NEAREST
            );
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

        Code::Main(
            inputBytes.data(),
            static_cast<int>(inputBytes.size()),
            frameDir.string().c_str(),
            "png",
            static_cast<int>(1LL * fps * durationMs / 1000)
        );

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

    int DecodeVideo(const char* inputVideo, const char* outputFile)
    {
        const fs::path frameDir = "inputImg";
        std::error_code ec;
        fs::remove_all(frameDir, ec);
        fs::create_directories(frameDir, ec);
        if (ec)
        {
            std::cerr << "error: failed to prepare temporary frame directory\n";
            return 1;
        }

        bool extractionDone = false;
        int extractionResult = 0;
        std::thread extractor([&]()
        {
            extractionResult = FFMPEG::VideotoImage(inputVideo, frameDir.string().c_str(), "png");
            extractionDone = true;
        });

        std::vector<fs::path> imagePaths;
        for (int index = 1;; ++index)
        {
            char fileName[64];
            std::snprintf(fileName, sizeof(fileName), "%05d.png", index);
            const fs::path imagePath = frameDir / fileName;

            while (!fs::exists(imagePath) && !extractionDone)
            {
                std::this_thread::yield();
            }

            if (!fs::exists(imagePath))
            {
                break;
            }
            imagePaths.push_back(imagePath);
        }

        extractor.join();
        if (extractionResult != 0)
        {
            std::cerr << "error: ffmpeg failed while extracting video frames\n";
            return 1;
        }
        if (imagePaths.empty())
        {
            std::cerr << "error: no frames extracted from video\n";
            return 1;
        }

        return DecodeImageSequence(imagePaths, outputFile) ? 0 : 1;
    }
}

int main(int argc, char* argv[])
{
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
        if (argc != 4)
        {
            PrintUsage();
            return 1;
        }
        return DecodeVideo(argv[2], argv[3]);
    }

    PrintUsage();
    return 1;
}
