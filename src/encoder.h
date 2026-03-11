#pragma once

#include "protocol_iso.h"

#include <filesystem>
#include <string>
#include <vector>

namespace demo_encoder {

struct DecodedFrameReport {
    int source_index = -1;
    bool success = false;
    std::string profile;
    std::string ecc;
    std::string method;
    std::string message;
    uint16_t frame_seq = 0;
    uint16_t total_frames = 0;
    uint16_t payload_len = 0;
};

std::vector<uint8_t> ReadFileBytes(const std::filesystem::path& input_path);
bool WriteIsoSamples(const std::filesystem::path& output_dir,
                     const protocol_iso::EncoderOptions& options,
                     std::string* error_message = nullptr);
bool WriteIsoPackage(const std::filesystem::path& input_path,
                     const std::filesystem::path& output_dir,
                     const protocol_iso::EncoderOptions& options,
                     std::string* error_message = nullptr);
bool DecodeIsoPackage(const std::filesystem::path& input_path,
                      const std::filesystem::path& output_dir,
                      const protocol_iso::EncoderOptions& options,
                      std::string* error_message = nullptr);

}  // namespace demo_encoder
