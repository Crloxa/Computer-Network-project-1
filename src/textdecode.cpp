#include "ImgDecode.h"
#include "ffmpeg.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdint>
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
            << "  textdecode <input_video_or_frame_dir> <output_file>\n"
            << "  textdecode dir <frame_dir> <output_file>\n"
            << "  textdecode video <input_video> <output_file>\n";
    }

    bool IsImageExtension(const fs::path& path)
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp";
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

    std::vector<fs::path> CollectImagePaths(const fs::path& frameDir)
    {
        std::vector<fs::path> imagePaths;
        std::error_code ec;
        if (!fs::exists(frameDir, ec) || !fs::is_directory(frameDir, ec))
        {
            return imagePaths;
        }

        for (const auto& entry : fs::directory_iterator(frameDir, ec))
        {
            if (ec)
            {
                imagePaths.clear();
                return imagePaths;
            }
            if (!entry.is_regular_file())
            {
                continue;
            }
            if (IsImageExtension(entry.path()))
            {
                imagePaths.push_back(entry.path());
            }
        }

        std::sort(imagePaths.begin(), imagePaths.end());
        return imagePaths;
    }
}

int main(int argc, char* argv[])
{
    if (argc != 3 && argc != 4)
    {
        PrintUsage();
        return 1;
    }

    std::string mode;
    fs::path inputPath;
    fs::path outputFile;
    if (argc == 4)
    {
        mode = argv[1];
        std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });
        inputPath = argv[2];
        outputFile = argv[3];
        if (mode != "dir" && mode != "video")
        {
            PrintUsage();
            return 1;
        }
    }
    else
    {
        inputPath = argv[1];
        outputFile = argv[2];
    }
    std::vector<fs::path> imagePaths;
    fs::path frameDir;

    std::error_code ec;
    const bool forceDir = mode == "dir";
    const bool forceVideo = mode == "video";
    if (forceDir || (!forceVideo && fs::exists(inputPath, ec) && fs::is_directory(inputPath, ec)))
    {
        frameDir = inputPath;
        imagePaths = CollectImagePaths(frameDir);
        if (imagePaths.empty())
        {
            std::cerr << "error: no images found in frame directory " << frameDir.string() << '\n';
            return 1;
        }
    }
    else
    {
        frameDir = outputFile.has_parent_path()
            ? (outputFile.parent_path() / "textdecode_frames")
            : fs::path("textdecode_frames");
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
            extractionResult = FFMPEG::VideotoImage(inputPath.string().c_str(), frameDir.string().c_str(), "png");
            extractionDone = true;
        });

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
            std::cerr << "error: ffmpeg failed while extracting frames\n";
            return 1;
        }
        if (imagePaths.empty())
        {
            std::cerr << "error: no frames extracted from video\n";
            return 1;
        }
    }

    if (!DecodeImageSequence(imagePaths, outputFile))
    {
        return 1;
    }

    std::cout << "Decoded file written to " << outputFile.string() << '\n';
    std::cout << "Frame source used: " << frameDir.string() << '\n';
    return 0;
}
