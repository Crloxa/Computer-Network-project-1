#include "encoder.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

void PrintUsage(const char* program_name) {
    std::cout << "Usage:\n"
              << "  " << program_name << " samples <output_dir>\n"
              << "  " << program_name << " demo <input_file> <output_dir> [--fps n] [--repeat n]\n"
              << "  " << program_name << " encode <input_file> <output_dir> [--fps n] [--repeat n]\n"
              << "  " << program_name << " decode <input_video_or_frame_dir> <output_dir>\n\n"
              << "Commands:\n"
              << "  samples  Write V1.6-108-4F sample images and layout guide.\n"
              << "  demo     Alias of encode; keeps the historical demo entry.\n"
              << "  encode   Encode a file into V1.6-108-4F frames and demo.mp4.\n"
              << "  decode   Decode self-generated V1.6 frames or demo.mp4 back into output.bin.\n";
}

bool ParseOptions(int argc,
                  char* argv[],
                  int option_start,
                  protocol_v1::EncoderOptions* options,
                  std::string* error) {
    for (int index = option_start; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--fps") {
            if (index + 1 >= argc) {
                *error = "--fps requires a value.";
                return false;
            }
            options->fps = std::stoi(argv[++index]);
        } else if (argument == "--repeat") {
            if (index + 1 >= argc) {
                *error = "--repeat requires a value.";
                return false;
            }
            options->repeat = std::stoi(argv[++index]);
        } else {
            *error = "Unknown option: " + argument;
            return false;
        }
    }

    if (options->fps <= 0 || options->repeat <= 0) {
        *error = "--fps and --repeat must be positive.";
        return false;
    }
    return true;
}

int RunSamples(const std::filesystem::path& output_dir, const protocol_v1::EncoderOptions& options) {
    std::string error_message;
    if (!demo_encoder::WriteV1Samples(output_dir, options, &error_message)) {
        std::cerr << error_message << std::endl;
        return 1;
    }
    std::cout << "V1.6 samples written to: " << output_dir << std::endl;
    return 0;
}

int RunEncode(const std::filesystem::path& input_path,
              const std::filesystem::path& output_dir,
              const protocol_v1::EncoderOptions& options) {
    std::string error_message;
    if (!demo_encoder::WriteV1Package(input_path, output_dir, options, &error_message)) {
        std::cerr << error_message << std::endl;
        return 1;
    }
    std::cout << "V1.6 package written to: " << output_dir << std::endl;
    return 0;
}

int RunDecode(const std::filesystem::path& input_path,
              const std::filesystem::path& output_dir,
              const protocol_v1::EncoderOptions& options) {
    std::string error_message;
    if (!demo_encoder::DecodeV1Package(input_path, output_dir, options, &error_message)) {
        std::cerr << error_message << std::endl;
        return 1;
    }
    std::cout << "V1.6 decode output written to: " << output_dir << std::endl;
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc < 3) {
            PrintUsage(argv[0]);
            return 1;
        }

        protocol_v1::EncoderOptions options;
        std::string parse_error;
        const std::string command = argv[1];
        if (command == "samples") {
            if (!ParseOptions(argc, argv, 3, &options, &parse_error)) {
                std::cerr << parse_error << std::endl;
                return 1;
            }
            return RunSamples(argv[2], options);
        }

        if ((command == "encode" || command == "demo") && argc >= 4) {
            if (!ParseOptions(argc, argv, 4, &options, &parse_error)) {
                std::cerr << parse_error << std::endl;
                return 1;
            }
            return RunEncode(argv[2], argv[3], options);
        }

        if (command == "decode" && argc >= 4) {
            if (!ParseOptions(argc, argv, 4, &options, &parse_error)) {
                std::cerr << parse_error << std::endl;
                return 1;
            }
            return RunDecode(argv[2], argv[3], options);
        }

        PrintUsage(argv[0]);
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << std::endl;
        return 1;
    }
}
