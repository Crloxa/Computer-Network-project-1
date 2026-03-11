#pragma once

#include "protocol_v1.h"

#include <filesystem>
#include <string>
#include <vector>

namespace demo_encoder {

struct DecodedFrameReport {
    int source_index = -1;
    bool success = false;
    uint16_t frame_seq = 0;
    uint16_t tail_len_bytes = 0;
    uint16_t payload_len_bytes = 0;
    std::string frame_type;
    std::string message;
};

std::vector<uint8_t> ReadFileBytes(const std::filesystem::path& input_path);
bool WriteV1Samples(const std::filesystem::path& output_dir,
                    const protocol_v1::EncoderOptions& options,
                    std::string* error_message = nullptr);
bool WriteV1Package(const std::filesystem::path& input_path,
                    const std::filesystem::path& output_dir,
                    const protocol_v1::EncoderOptions& options,
                    std::string* error_message = nullptr);
bool DecodeV1Package(const std::filesystem::path& input_path,
                     const std::filesystem::path& output_dir,
                     const protocol_v1::EncoderOptions& options,
                     std::string* error_message = nullptr);

}  // namespace demo_encoder
