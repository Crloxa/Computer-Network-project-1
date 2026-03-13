#include "encoder.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr char kCurrentProtocol[] = "V1.7-133-4F";

void PrintUsage(const char* program_name) {
    std::cout << "Usage:\n"
              << "  " << program_name << " --help\n"
              << "  " << program_name << " --version\n"
              << "  " << program_name << " samples <output_dir>\n"
              << "  " << program_name << " demo <input_file> <output_dir> [--fps n] [--repeat n]\n"
              << "  " << program_name << " encode <input_file> <output_dir> [--fps n] [--repeat n]\n"
              << "  " << program_name << " decode <input_video_or_frame_dir> <output_dir>\n\n"
              << "Commands:\n"
              << "  samples  Write V1.7-133-4F sample images and layout guide for effect preview.\n"
              << "  demo     Alias of encode; keeps the historical demo entry for effect generation.\n"
              << "  encode   Encode a file into V1.7-133-4F frames and demo.mp4.\n"
              << "  decode   Self-check helper for repository-generated V1.7 frames or demo.mp4.\n";
}

void PrintVersion() {
    std::cout << "Project1 protocol=" << kCurrentProtocol << std::endl;
}

bool IsLegacyOption(const std::string& argument) {
    return argument == "--profile" ||
           argument == "--ecc" ||
           argument == "--canvas" ||
           argument == "--markers" ||
           argument == "--protocol-samples" ||
           argument == "--decode-debug";
}

void PrintCompatibilityWarning(const std::vector<std::string>& ignored_options) {
    if (ignored_options.empty()) {
        return;
    }

    std::ostringstream stream;
    for (std::size_t index = 0; index < ignored_options.size(); ++index) {
        if (index > 0U) {
            stream << ", ";
        }
        stream << ignored_options[index];
    }

    std::cerr << "[compat] 当前默认主线已切到 " << kCurrentProtocol
              << "，以下历史 ISO 参数已被忽略: " << stream.str() << '\n'
              << "[compat] 当前仅实际使用 --fps 和 --repeat。Windows PowerShell 推荐命令: "
              << ".\\x64\\Debug\\Project1.exe encode input.jpg out\\encode\\input"
              << std::endl;
}

bool ParseOptions(int argc,
                  char* argv[],
                  int option_start,
                  protocol_v1::EncoderOptions* options,
                  std::vector<std::string>* ignored_legacy_options,
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
        } else if (IsLegacyOption(argument)) {
            if (index + 1 >= argc) {
                *error = argument + " requires a value.";
                return false;
            }
            const std::string value = argv[++index];
            if (ignored_legacy_options != nullptr) {
                ignored_legacy_options->push_back(argument + "=" + value);
            }
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
    std::cout << "V1.7 samples written to: " << output_dir << std::endl;
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
    std::cout << "V1.7 package written to: " << output_dir << std::endl;
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
    std::cout << "V1.7 decode output written to: " << output_dir << std::endl;
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc == 2) {
            const std::string single_argument = argv[1];
            if (single_argument == "--help" || single_argument == "-h" || single_argument == "help") {
                PrintUsage(argv[0]);
                return 0;
            }
            if (single_argument == "--version" || single_argument == "-v") {
                PrintVersion();
                return 0;
            }
        }

        if (argc < 3) {
            PrintUsage(argv[0]);
            return 1;
        }

        protocol_v1::EncoderOptions options;
        std::vector<std::string> ignored_legacy_options;
        std::string parse_error;
        const std::string command = argv[1];
        if (command == "samples") {
            if (!ParseOptions(argc, argv, 3, &options, &ignored_legacy_options, &parse_error)) {
                std::cerr << parse_error << std::endl;
                return 1;
            }
            PrintVersion();
            PrintCompatibilityWarning(ignored_legacy_options);
            return RunSamples(argv[2], options);
        }

        if ((command == "encode" || command == "demo") && argc >= 4) {
            if (!ParseOptions(argc, argv, 4, &options, &ignored_legacy_options, &parse_error)) {
                std::cerr << parse_error << std::endl;
                return 1;
            }
            PrintVersion();
            PrintCompatibilityWarning(ignored_legacy_options);
            return RunEncode(argv[2], argv[3], options);
        }

        if (command == "decode" && argc >= 4) {
            if (!ParseOptions(argc, argv, 4, &options, &ignored_legacy_options, &parse_error)) {
                std::cerr << parse_error << std::endl;
                return 1;
            }
            PrintVersion();
            PrintCompatibilityWarning(ignored_legacy_options);
            return RunDecode(argv[2], argv[3], options);
        }

        PrintUsage(argv[0]);
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << std::endl;
        return 1;
    }
}
