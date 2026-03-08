#include "encoder.h"
#include "protocol_v1.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

void PrintUsage(const char* program_name) {
    std::cout << "Usage:\n"
              << "  " << program_name << " samples <output_dir>\n"
              << "  " << program_name << " encode <input_image_or_file> <output_dir>\n"
              << "  " << program_name << " demo <input_image_or_file> <output_dir>\n\n"
              << "Commands:\n"
              << "  samples  Write V1.6-108-4F sample PNGs and a layout guide.\n"
              << "  encode   Encode a single input file into logical/physical frame PNGs and demo.mp4.\n"
              << "  demo     Alias of encode.\n";
}

int RunSamples(const std::filesystem::path& output_dir) {
    protocol_v1::EncoderOptions options;
    std::string error_message;
    if (!demo_encoder::WriteProtocolSamples(output_dir, options, &error_message)) {
        std::cerr << error_message << std::endl;
        return 1;
    }
    std::cout << "V1.6-108-4F samples written to: " << output_dir << std::endl;
    return 0;
}

int RunEncode(const std::filesystem::path& input_path, const std::filesystem::path& output_dir) {
    protocol_v1::EncoderOptions options;
    std::string error_message;
    if (!demo_encoder::WriteDemoPackage(input_path, output_dir, options, &error_message)) {
        std::cerr << error_message << std::endl;
        return 1;
    }
    std::cout << "Demo package written to: " << output_dir << std::endl;
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc < 3) {
            PrintUsage(argv[0]);
            return 1;
        }

        const std::string command = argv[1];
        if (command == "samples") {
            return RunSamples(argv[2]);
        }
        if ((command == "encode" || command == "demo") && argc >= 4) {
            return RunEncode(argv[2], argv[3]);
        }

        PrintUsage(argv[0]);
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << std::endl;
        return 1;
    }
}
