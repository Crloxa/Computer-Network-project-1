#include "encoder.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

void PrintUsage(const char* program_name) {
    std::cout << "Usage:\n"
              << "  " << program_name << " samples <output_dir> [--profile iso133] [--ecc Q] [--canvas px]\n"
              << "  " << program_name << " encode <input_file> <output_dir> [--profile iso133] [--ecc Q] [--canvas px] [--fps n] [--repeat n] [--markers on] [--protocol-samples on|off]\n"
              << "  " << program_name << " decode <input_video_or_frame_dir> <output_dir> [--profile iso133] [--ecc Q] [--canvas px] [--markers on] [--decode-debug on|off]\n\n"
              << "Commands:\n"
              << "  samples  Write self-hosted ISO QR v2 samples and a constrained capacity matrix.\n"
              << "  encode   Encode a single input file into self-hosted ISO QR v2 frames and demo.mp4.\n"
              << "  decode   Decode a self-hosted ISO QR v2 video or frame directory back into output.bin.\n";
}

bool ParseOptions(int argc, char* argv[], int option_start, protocol_iso::EncoderOptions* options, std::string* error) {
    for (int index = option_start; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--profile") {
            if (index + 1 >= argc) {
                *error = "--profile requires a value.";
                return false;
            }
            const auto profile = protocol_iso::ParseProfileId(argv[++index]);
            if (!profile.has_value()) {
                *error = "Unsupported profile: " + std::string(argv[index]);
                return false;
            }
            options->profile_id = profile.value();
        } else if (argument == "--ecc") {
            if (index + 1 >= argc) {
                *error = "--ecc requires a value.";
                return false;
            }
            const auto ecc = protocol_iso::ParseErrorCorrection(argv[++index]);
            if (!ecc.has_value()) {
                *error = "Unsupported ECC level: " + std::string(argv[index]);
                return false;
            }
            options->error_correction = ecc.value();
        } else if (argument == "--canvas") {
            if (index + 1 >= argc) {
                *error = "--canvas requires a value.";
                return false;
            }
            options->canvas_pixels = std::stoi(argv[++index]);
        } else if (argument == "--fps") {
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
        } else if (argument == "--markers") {
            if (index + 1 >= argc) {
                *error = "--markers requires a value.";
                return false;
            }
            const std::string value = argv[++index];
            if (value == "on") {
                options->enable_carrier_markers = true;
            } else if (value == "off") {
                options->enable_carrier_markers = false;
            } else {
                *error = "--markers must be 'on' or 'off'.";
                return false;
            }
        } else if (argument == "--protocol-samples") {
            if (index + 1 >= argc) {
                *error = "--protocol-samples requires a value.";
                return false;
            }
            const std::string value = argv[++index];
            if (value == "on") {
                options->write_protocol_samples = true;
            } else if (value == "off") {
                options->write_protocol_samples = false;
            } else {
                *error = "--protocol-samples must be 'on' or 'off'.";
                return false;
            }
        } else if (argument == "--decode-debug") {
            if (index + 1 >= argc) {
                *error = "--decode-debug requires a value.";
                return false;
            }
            const std::string value = argv[++index];
            if (value == "on") {
                options->write_decode_debug = true;
            } else if (value == "off") {
                options->write_decode_debug = false;
            } else {
                *error = "--decode-debug must be 'on' or 'off'.";
                return false;
            }
        } else {
            *error = "Unknown option: " + argument;
            return false;
        }
    }

    if (options->canvas_pixels < 720) {
        *error = "--canvas must be at least 720.";
        return false;
    }
    if (options->fps <= 0 || options->repeat <= 0) {
        *error = "--fps and --repeat must be positive.";
        return false;
    }
    return true;
}

int RunSamples(const std::filesystem::path& output_dir, const protocol_iso::EncoderOptions& options) {
    std::string error_message;
    if (!demo_encoder::WriteIsoSamples(output_dir, options, &error_message)) {
        std::cerr << error_message << std::endl;
        return 1;
    }
    std::cout << "ISO samples written to: " << output_dir << std::endl;
    return 0;
}

int RunEncode(const std::filesystem::path& input_path,
              const std::filesystem::path& output_dir,
              const protocol_iso::EncoderOptions& options) {
    std::string error_message;
    if (!demo_encoder::WriteIsoPackage(input_path, output_dir, options, &error_message)) {
        std::cerr << error_message << std::endl;
        return 1;
    }
    std::cout << "ISO package written to: " << output_dir << std::endl;
    return 0;
}

int RunDecode(const std::filesystem::path& input_path,
              const std::filesystem::path& output_dir,
              const protocol_iso::EncoderOptions& options) {
    std::string error_message;
    if (!demo_encoder::DecodeIsoPackage(input_path, output_dir, options, &error_message)) {
        std::cerr << error_message << std::endl;
        return 1;
    }
    std::cout << "ISO decode output written to: " << output_dir << std::endl;
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc < 3) {
            PrintUsage(argv[0]);
            return 1;
        }

        protocol_iso::EncoderOptions options;
        std::string parse_error;
        const std::string command = argv[1];
        if (command == "samples") {
            if (!ParseOptions(argc, argv, 3, &options, &parse_error)) {
                std::cerr << parse_error << std::endl;
                return 1;
            }
            return RunSamples(argv[2], options);
        }

        if (command == "encode" && argc >= 4) {
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
