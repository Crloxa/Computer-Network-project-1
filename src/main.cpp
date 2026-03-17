#include <climits>
#include <filesystem>
#include <iostream>
#include <string>

#include "visual_net_pipeline.h"

namespace
{
	void PrintUsage()
	{
		std::cout
			<< "Usage:\n"
			<< "  Project1 encode <input_file> <output_dir> [output_format] [frame_limit]\n"
			<< "  Project1 decode-image <input_image> <output_file>\n"
			<< "  Project1 decode-dir <input_dir> <output_file>\n"
			<< "  Project1 roundtrip <input_file> <work_dir> [output_format] [frame_limit]\n";
	}

	int ParseFrameLimit(const char* value)
	{
		return std::stoi(value);
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
	std::string error;

	if (command == "encode")
	{
		if (argc < 4 || argc > 6)
		{
			PrintUsage();
			return 1;
		}

		const std::string format = (argc >= 5) ? argv[4] : "png";
		const int frameLimit = (argc == 6) ? ParseFrameLimit(argv[5]) : INT_MAX;
		if (!VisualNet::EncodeFileToDirectory(argv[2], argv[3], format, frameLimit, error))
		{
			std::cerr << error << '\n';
			return 1;
		}

		std::cout << "Encoded frames written to " << argv[3] << '\n';
		return 0;
	}

	if (command == "decode-image")
	{
		if (argc != 4)
		{
			PrintUsage();
			return 1;
		}

		VisualNet::DecodeStats stats;
		if (!VisualNet::DecodeImageToFile(argv[2], argv[3], stats, error))
		{
			std::cerr << error << '\n';
			return 1;
		}

		std::cout << "Decoded " << stats.decoded_frames << " frame(s) into " << argv[3] << '\n';
		return 0;
	}

	if (command == "decode-dir")
	{
		if (argc != 4)
		{
			PrintUsage();
			return 1;
		}

		VisualNet::DecodeStats stats;
		if (!VisualNet::DecodeDirectoryToFile(argv[2], argv[3], stats, error))
		{
			std::cerr << error << '\n';
			return 1;
		}

		std::cout << "Decoded " << stats.decoded_frames << " frame(s) into " << argv[3] << '\n';
		return 0;
	}

	if (command == "roundtrip")
	{
		if (argc < 4 || argc > 6)
		{
			PrintUsage();
			return 1;
		}

		const std::string format = (argc >= 5) ? argv[4] : "png";
		const int frameLimit = (argc == 6) ? ParseFrameLimit(argv[5]) : INT_MAX;
		VisualNet::DecodeStats stats;
		if (!VisualNet::RunRoundTrip(argv[2], argv[3], format, frameLimit, stats, error))
		{
			std::cerr << error << '\n';
			return 1;
		}

		std::cout
			<< "Roundtrip succeeded. Decoded "
			<< stats.decoded_frames
			<< " frame(s). Output written under "
			<< std::filesystem::path(argv[3]).string()
			<< '\n';
		return 0;
	}

	PrintUsage();
	return 1;
}
