#pragma once

#include <filesystem>
#include <string>

namespace VisualNet
{
	struct DecodeStats
	{
		int decoded_frames = 0;
		bool saw_start = false;
		bool saw_end = false;
	};

	bool EncodeFileToDirectory(const std::filesystem::path& inputPath,
		const std::filesystem::path& outputDir,
		const std::string& outputFormat,
		int frameLimit,
		std::string& error);

	bool DecodeDirectoryToFile(const std::filesystem::path& inputDir,
		const std::filesystem::path& outputFile,
		DecodeStats& stats,
		std::string& error);

	bool DecodeImageToFile(const std::filesystem::path& inputImage,
		const std::filesystem::path& outputFile,
		DecodeStats& stats,
		std::string& error);

	bool RunRoundTrip(const std::filesystem::path& inputFile,
		const std::filesystem::path& workDir,
		const std::string& outputFormat,
		int frameLimit,
		DecodeStats& stats,
		std::string& error);
}
