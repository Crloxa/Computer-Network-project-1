#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace protocol_iso {

enum class ProfileId {
    kIso109,
    kIso145,
    kIso177,
};

enum class ErrorCorrection {
    kM,
    kQ,
};

struct Profile {
    ProfileId id = ProfileId::kIso145;
    const char* name = "iso145";
    int version = 32;
    int logical_size = 145;
};

struct FrameHeader {
    uint8_t protocol_id = 0xA2;
    uint8_t protocol_version = 0x01;
    uint16_t frame_seq = 0;
    uint16_t total_frames = 1;
    uint16_t payload_len = 0;
};

struct EncoderOptions {
    ProfileId profile_id = ProfileId::kIso145;
    ErrorCorrection error_correction = ErrorCorrection::kQ;
    int canvas_pixels = 1440;
    int fps = 60;
    int repeat = 3;
    bool enable_carrier_markers = true;
};

constexpr uint8_t kProtocolId = 0xA2;
constexpr uint8_t kProtocolVersion = 0x01;
constexpr int kHeaderBytes = 8;
constexpr int kCrcBytes = 4;
constexpr int kFrameOverheadBytes = kHeaderBytes + kCrcBytes;

Profile ProfileFromId(ProfileId profile_id);
std::vector<Profile> SupportedProfiles();
std::optional<ProfileId> ParseProfileId(std::string_view value);
std::string ProfileName(ProfileId profile_id);

std::optional<ErrorCorrection> ParseErrorCorrection(std::string_view value);
std::string ErrorCorrectionName(ErrorCorrection error_correction);

std::vector<uint8_t> PackHeaderBytes(const FrameHeader& header);
std::vector<uint8_t> PackFrameBytes(const FrameHeader& header, const std::vector<uint8_t>& payload);
bool ParseFrameBytes(const std::vector<uint8_t>& frame_bytes,
                     FrameHeader* header,
                     std::vector<uint8_t>* payload,
                     std::string* error_message = nullptr);

uint32_t ComputeCrc32(const std::vector<uint8_t>& bytes);
std::string HeaderToString(const FrameHeader& header);

}  // namespace protocol_iso
