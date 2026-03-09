#include "protocol_iso.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace protocol_iso {
namespace {

void AppendUint16BigEndian(uint16_t value, std::vector<uint8_t>& bytes) {
    bytes.push_back(static_cast<uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<uint8_t>(value & 0xFFU));
}

void AppendUint32BigEndian(uint32_t value, std::vector<uint8_t>& bytes) {
    bytes.push_back(static_cast<uint8_t>((value >> 24U) & 0xFFU));
    bytes.push_back(static_cast<uint8_t>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<uint8_t>(value & 0xFFU));
}

uint16_t ReadUint16BigEndian(const std::vector<uint8_t>& bytes, std::size_t offset) {
    return static_cast<uint16_t>((static_cast<uint16_t>(bytes[offset]) << 8U) |
                                 static_cast<uint16_t>(bytes[offset + 1U]));
}

uint32_t ReadUint32BigEndian(const std::vector<uint8_t>& bytes, std::size_t offset) {
    return (static_cast<uint32_t>(bytes[offset]) << 24U) |
           (static_cast<uint32_t>(bytes[offset + 1U]) << 16U) |
           (static_cast<uint32_t>(bytes[offset + 2U]) << 8U) |
           static_cast<uint32_t>(bytes[offset + 3U]);
}

std::string ToLowerCopy(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return result;
}

}  // namespace

Profile ProfileFromId(ProfileId profile_id) {
    switch (profile_id) {
        case ProfileId::kIso109:
            return {profile_id, "iso109", 23, 109};
        case ProfileId::kIso145:
            return {profile_id, "iso145", 32, 145};
        case ProfileId::kIso177:
            return {profile_id, "iso177", 40, 177};
    }
    return {ProfileId::kIso145, "iso145", 32, 145};
}

std::vector<Profile> SupportedProfiles() {
    return {ProfileFromId(ProfileId::kIso109),
            ProfileFromId(ProfileId::kIso145),
            ProfileFromId(ProfileId::kIso177)};
}

std::optional<ProfileId> ParseProfileId(std::string_view value) {
    const std::string normalized = ToLowerCopy(value);
    if (normalized == "iso109" || normalized == "109" || normalized == "v23") {
        return ProfileId::kIso109;
    }
    if (normalized == "iso145" || normalized == "145" || normalized == "v32") {
        return ProfileId::kIso145;
    }
    if (normalized == "iso177" || normalized == "177" || normalized == "v40") {
        return ProfileId::kIso177;
    }
    return std::nullopt;
}

std::string ProfileName(ProfileId profile_id) {
    return ProfileFromId(profile_id).name;
}

std::optional<ErrorCorrection> ParseErrorCorrection(std::string_view value) {
    const std::string normalized = ToLowerCopy(value);
    if (normalized == "m") {
        return ErrorCorrection::kM;
    }
    if (normalized == "q") {
        return ErrorCorrection::kQ;
    }
    return std::nullopt;
}

std::string ErrorCorrectionName(ErrorCorrection error_correction) {
    switch (error_correction) {
        case ErrorCorrection::kM:
            return "M";
        case ErrorCorrection::kQ:
            return "Q";
    }
    return "Q";
}

std::vector<uint8_t> PackHeaderBytes(const FrameHeader& header) {
    std::vector<uint8_t> bytes;
    bytes.reserve(kHeaderBytes);
    bytes.push_back(header.protocol_id);
    bytes.push_back(header.protocol_version);
    AppendUint16BigEndian(header.frame_seq, bytes);
    AppendUint16BigEndian(header.total_frames, bytes);
    AppendUint16BigEndian(header.payload_len, bytes);
    return bytes;
}

std::vector<uint8_t> PackFrameBytes(const FrameHeader& header, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> bytes = PackHeaderBytes(header);
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    const uint32_t crc32 = ComputeCrc32(bytes);
    AppendUint32BigEndian(crc32, bytes);
    return bytes;
}

bool ParseFrameBytes(const std::vector<uint8_t>& frame_bytes,
                     FrameHeader* header,
                     std::vector<uint8_t>* payload,
                     std::string* error_message) {
    if (frame_bytes.size() < static_cast<std::size_t>(kFrameOverheadBytes)) {
        if (error_message != nullptr) {
            *error_message = "Frame payload is smaller than the ISO application header and CRC32.";
        }
        return false;
    }

    FrameHeader parsed_header;
    parsed_header.protocol_id = frame_bytes[0];
    parsed_header.protocol_version = frame_bytes[1];
    parsed_header.frame_seq = ReadUint16BigEndian(frame_bytes, 2);
    parsed_header.total_frames = ReadUint16BigEndian(frame_bytes, 4);
    parsed_header.payload_len = ReadUint16BigEndian(frame_bytes, 6);

    if (parsed_header.protocol_id != kProtocolId || parsed_header.protocol_version != kProtocolVersion) {
        if (error_message != nullptr) {
            *error_message = "Frame header protocol id/version does not match the ISO transport profile.";
        }
        return false;
    }

    const std::size_t expected_size =
        static_cast<std::size_t>(kHeaderBytes) + parsed_header.payload_len + static_cast<std::size_t>(kCrcBytes);
    if (frame_bytes.size() != expected_size) {
        if (error_message != nullptr) {
            *error_message = "Frame length does not match header payload_len.";
        }
        return false;
    }

    const uint32_t expected_crc32 = ReadUint32BigEndian(frame_bytes, expected_size - static_cast<std::size_t>(kCrcBytes));
    std::vector<uint8_t> body(frame_bytes.begin(), frame_bytes.end() - kCrcBytes);
    const uint32_t actual_crc32 = ComputeCrc32(body);
    if (actual_crc32 != expected_crc32) {
        if (error_message != nullptr) {
            std::ostringstream stream;
            stream << "CRC32 mismatch: expected 0x" << std::uppercase << std::hex << expected_crc32
                   << ", got 0x" << actual_crc32;
            *error_message = stream.str();
        }
        return false;
    }

    if (header != nullptr) {
        *header = parsed_header;
    }
    if (payload != nullptr) {
        payload->assign(frame_bytes.begin() + kHeaderBytes, frame_bytes.end() - kCrcBytes);
    }
    return true;
}

uint32_t ComputeCrc32(const std::vector<uint8_t>& bytes) {
    uint32_t crc = 0xFFFFFFFFU;
    for (uint8_t byte : bytes) {
        crc ^= static_cast<uint32_t>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            const bool lsb = (crc & 1U) != 0U;
            crc >>= 1U;
            if (lsb) {
                crc ^= 0xEDB88320U;
            }
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

std::string HeaderToString(const FrameHeader& header) {
    std::ostringstream stream;
    stream << "proto=0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(header.protocol_id)
           << " ver=0x" << std::setw(2) << static_cast<int>(header.protocol_version)
           << std::dec
           << " seq=" << header.frame_seq
           << " total=" << header.total_frames
           << " len=" << header.payload_len;
    return stream.str();
}

}  // namespace protocol_iso
