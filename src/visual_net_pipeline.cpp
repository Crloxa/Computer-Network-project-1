#include "visual_net_pipeline.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

#include "ImgDecode.h"
#include "code.h"
#include "pic.h"

namespace VisualNet
{
	namespace
	{
		using ByteVector = std::vector<unsigned char>;

		bool ReadBinaryFile(const std::filesystem::path& path, ByteVector& data, std::string& error)
		{
			std::ifstream input(path, std::ios::binary);
			if (!input)
			{
				error = "failed to open input file: " + path.string();
				return false;
			}
			data.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
			return true;
		}

		bool WriteBinaryFile(const std::filesystem::path& path, const ByteVector& data, std::string& error)
		{
			if (path.has_parent_path())
			{
				std::error_code ec;
				std::filesystem::create_directories(path.parent_path(), ec);
				if (ec)
				{
					error = "failed to create output directory: " + path.parent_path().string();
					return false;
				}
			}

			std::ofstream output(path, std::ios::binary);
			if (!output)
			{
				error = "failed to open output file: " + path.string();
				return false;
			}

			output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
			if (!output)
			{
				error = "failed to write output file: " + path.string();
				return false;
			}
			return true;
		}

		std::string ToLower(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return value;
		}

		bool IsImageFile(const std::filesystem::path& path)
		{
			const std::string ext = ToLower(path.extension().string());
			return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".webp";
		}

		bool CollectImageFiles(const std::filesystem::path& inputDir, std::vector<std::filesystem::path>& files, std::string& error)
		{
			if (!std::filesystem::exists(inputDir))
			{
				error = "input directory does not exist: " + inputDir.string();
				return false;
			}

			for (const auto& entry : std::filesystem::directory_iterator(inputDir))
			{
				if (entry.is_regular_file() && IsImageFile(entry.path()))
				{
					files.push_back(entry.path());
				}
			}

			std::sort(files.begin(), files.end());
			if (files.empty())
			{
				error = "no image files found in directory: " + inputDir.string();
				return false;
			}
			return true;
		}

		bool DecodeImagesToFile(const std::vector<std::filesystem::path>& imagePaths,
			const std::filesystem::path& outputFile,
			DecodeStats& stats,
			std::string& error)
		{
			ByteVector outputBytes;
			int previousFrameBase = -1;
			bool hasStarted = false;
			bool hasEnded = false;

			for (const auto& imagePath : imagePaths)
			{
				cv::Mat sourceImage = cv::imread(imagePath.string(), cv::IMREAD_COLOR);
				if (sourceImage.empty())
				{
					continue;
				}

				cv::Mat logicalFrame;
				if (ImgParse::Main(sourceImage, logicalFrame))
				{
					continue;
				}

				ImageDecode::ImageInfo imageInfo;
				if (ImageDecode::Main(logicalFrame, imageInfo))
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
					stats.saw_start = true;
				}

				if (previousFrameBase == imageInfo.FrameBase)
				{
					continue;
				}

				if (previousFrameBase != -1)
				{
					const int expected = (previousFrameBase + 1) & std::numeric_limits<uint16_t>::max();
					if (expected != imageInfo.FrameBase)
					{
						error = "skipped frame detected before " + imagePath.string();
						return false;
					}
				}

				previousFrameBase = imageInfo.FrameBase;
				outputBytes.insert(outputBytes.end(), imageInfo.Info.begin(), imageInfo.Info.end());
				++stats.decoded_frames;

				if (imageInfo.IsEnd)
				{
					hasEnded = true;
					stats.saw_end = true;
					break;
				}
			}

			if (!hasStarted)
			{
				error = "did not find a start frame in the provided images";
				return false;
			}
			if (!hasEnded)
			{
				error = "did not find an end frame in the provided images";
				return false;
			}

			return WriteBinaryFile(outputFile, outputBytes, error);
		}

		bool FilesEqual(const std::filesystem::path& lhs, const std::filesystem::path& rhs, std::string& error)
		{
			ByteVector left;
			ByteVector right;
			if (!ReadBinaryFile(lhs, left, error))
			{
				return false;
			}
			if (!ReadBinaryFile(rhs, right, error))
			{
				return false;
			}
			if (left != right)
			{
				error = "roundtrip bytes do not match";
				return false;
			}
			return true;
		}
	}

	bool EncodeFileToDirectory(const std::filesystem::path& inputPath,
		const std::filesystem::path& outputDir,
		const std::string& outputFormat,
		int frameLimit,
		std::string& error)
	{
		ByteVector inputBytes;
		if (!ReadBinaryFile(inputPath, inputBytes, error))
		{
			return false;
		}

		std::error_code ec;
		std::filesystem::create_directories(outputDir, ec);
		if (ec)
		{
			error = "failed to create output directory: " + outputDir.string();
			return false;
		}

		Code::Main(reinterpret_cast<const char*>(inputBytes.data()),
			static_cast<int>(inputBytes.size()),
			outputDir.string().c_str(),
			outputFormat.c_str(),
			frameLimit);
		return true;
	}

	bool DecodeDirectoryToFile(const std::filesystem::path& inputDir,
		const std::filesystem::path& outputFile,
		DecodeStats& stats,
		std::string& error)
	{
		stats = {};
		std::vector<std::filesystem::path> files;
		if (!CollectImageFiles(inputDir, files, error))
		{
			return false;
		}
		return DecodeImagesToFile(files, outputFile, stats, error);
	}

	bool DecodeImageToFile(const std::filesystem::path& inputImage,
		const std::filesystem::path& outputFile,
		DecodeStats& stats,
		std::string& error)
	{
		stats = {};
		return DecodeImagesToFile({ inputImage }, outputFile, stats, error);
	}

	bool RunRoundTrip(const std::filesystem::path& inputFile,
		const std::filesystem::path& workDir,
		const std::string& outputFormat,
		int frameLimit,
		DecodeStats& stats,
		std::string& error)
	{
		const auto frameDir = workDir / "encoded_frames";
		const auto decodedFile = workDir / "decoded.bin";
		if (!EncodeFileToDirectory(inputFile, frameDir, outputFormat, frameLimit, error))
		{
			return false;
		}
		if (!DecodeDirectoryToFile(frameDir, decodedFile, stats, error))
		{
			return false;
		}
		return FilesEqual(inputFile, decodedFile, error);
	}
}
