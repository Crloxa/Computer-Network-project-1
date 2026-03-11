#include "protocol_v1.h"

#include <iomanip>
#include <sstream>

namespace protocol_v1 {
namespace {

void AppendBitsBigEndian(uint32_t value, int bit_count, std::vector<bool>& bits) {
    for (int bit = bit_count - 1; bit >= 0; --bit) {
        bits.push_back(((value >> bit) & 1U) != 0U);
    }
}

bool IsInsideRect(int x, int y, int origin_x, int origin_y, int width, int height) {
    return x >= origin_x && x < (origin_x + width) && y >= origin_y && y < (origin_y + height);
}

uint16_t ReadBitsBigEndian(const std::vector<bool>& bits, std::size_t offset, int bit_count) {
    uint16_t value = 0;
    for (int bit = 0; bit < bit_count; ++bit) {
        value <<= 1U;
        if (bits[offset + static_cast<std::size_t>(bit)]) {
            value |= 1U;
        }
    }
    return value;
}

uint16_t FrameTypeCode(FrameType frame_type) {
    switch (frame_type) {
        case FrameType::kSingle:
            return 0x0U;
        case FrameType::kStart:
            return 0x3U;
        case FrameType::kNormal:
            return 0xFU;
        case FrameType::kEnd:
            return 0xCU;
    }
    return 0U;
}

bool FrameTypeFromCode(uint16_t code, FrameType* frame_type) {
    switch (code & 0xFU) {
        case 0x0U:
            *frame_type = FrameType::kSingle;
            return true;
        case 0x3U:
            *frame_type = FrameType::kStart;
            return true;
        case 0xFU:
            *frame_type = FrameType::kNormal;
            return true;
        case 0xCU:
            *frame_type = FrameType::kEnd;
            return true;
    }
    return false;
}

}  // namespace

bool IsInBounds(int x, int y) {
    return x >= 0 && x < kLogicalGridSize && y >= 0 && y < kLogicalGridSize;
}

bool IsHeaderCell(int x, int y) {
    return IsInsideRect(x, y, kHeaderOriginX, kHeaderOriginY, kHeaderWidthModules, kHeaderHeightModules);
}

bool IsFinderReserveCell(int x, int y) {
    if (!IsInBounds(x, y)) {
        return false;
    }
    const bool in_top_left = x <= kFinderTopLeftMax && y <= kFinderTopLeftMax;
    const bool in_top_right = x >= kFinderTopRightMin && y <= kFinderTopLeftMax;
    const bool in_bottom_left = x <= kFinderTopLeftMax && y >= kFinderBottomLeftMin;
    const bool in_bottom_right = x >= kFinderTopRightMin && y >= kFinderBottomLeftMin;
    return in_top_left || in_top_right || in_bottom_left || in_bottom_right;
}

bool IsTimingCell(int x, int y) {
    const bool in_horizontal = y == kTimingHorizontalY && x >= kTimingStart && x <= kTimingEnd;
    const bool in_vertical = x == kTimingVerticalX && y >= kTimingStart && y <= kTimingEnd;
    return in_horizontal || in_vertical;
}

bool IsAlignmentCell(int x, int y) {
    return IsInsideRect(x, y, kAlignmentOriginX, kAlignmentOriginY,
                        kAlignmentSizeModules, kAlignmentSizeModules);
}

bool IsPayloadCell(int x, int y) {
    if (!IsInBounds(x, y)) {
        return false;
    }
    return !IsFinderReserveCell(x, y) && !IsHeaderCell(x, y) && !IsTimingCell(x, y) && !IsAlignmentCell(x, y);
}

bool IsStartFrame(FrameType frame_type) {
    return frame_type == FrameType::kStart || frame_type == FrameType::kSingle;
}

bool IsEndFrame(FrameType frame_type) {
    return frame_type == FrameType::kEnd || frame_type == FrameType::kSingle;
}

std::pair<int, int> HeaderOrigin() {
    return {kHeaderOriginX, kHeaderOriginY};
}

std::pair<int, int> HeaderDimensions() {
    return {kHeaderWidthModules, kHeaderHeightModules};
}

std::vector<GridPoint> HeaderCells() {
    std::vector<GridPoint> cells;
    cells.reserve(kHeaderBits);
    for (int y = kHeaderOriginY; y < kHeaderOriginY + kHeaderHeightModules; ++y) {
        for (int x = kHeaderOriginX; x < kHeaderOriginX + kHeaderWidthModules; ++x) {
            cells.push_back({x, y});
        }
    }
    return cells;
}

std::vector<GridPoint> PayloadCells() {
    std::vector<GridPoint> cells;
    cells.reserve(kPayloadCapacityBits);
    for (int y = 0; y < kLogicalGridSize; ++y) {
        for (int x = 0; x < kLogicalGridSize; ++x) {
            if (IsPayloadCell(x, y)) {
                cells.push_back({x, y});
            }
        }
    }
    return cells;
}

std::vector<bool> PackHeaderBits(const FrameHeader& header) {
    std::vector<bool> bits;
    bits.reserve(kHeaderBits);
    AppendBitsBigEndian(FrameTypeCode(header.frame_type), 4, bits);
    AppendBitsBigEndian(header.tail_len_bytes, 12, bits);
    AppendBitsBigEndian(header.checkcode16, 16, bits);
    AppendBitsBigEndian(header.frame_seq, 16, bits);
    bits.resize(kHeaderBits, false);
    return bits;
}

bool ParseHeaderBits(const std::vector<bool>& bits, FrameHeader* header, std::string* error_message) {
    if (bits.size() != static_cast<std::size_t>(kHeaderBits)) {
        if (error_message != nullptr) {
            *error_message = "header_bit_count_mismatch";
        }
        return false;
    }

    FrameType frame_type = FrameType::kSingle;
    if (!FrameTypeFromCode(ReadBitsBigEndian(bits, 0U, 4), &frame_type)) {
        if (error_message != nullptr) {
            *error_message = "frame_type_invalid";
        }
        return false;
    }

    const uint16_t tail_len_bytes = ReadBitsBigEndian(bits, 4U, 12);
    const uint16_t checkcode16 = ReadBitsBigEndian(bits, 16U, 16);
    const uint16_t frame_seq = ReadBitsBigEndian(bits, 32U, 16);

    for (std::size_t index = static_cast<std::size_t>(kHeaderWidthModules * kHeaderUsedRows); index < bits.size(); ++index) {
        if (bits[index]) {
            if (error_message != nullptr) {
                *error_message = "header_padding_nonzero";
            }
            return false;
        }
    }

    if (tail_len_bytes > static_cast<uint16_t>(kMaxPayloadBytes)) {
        if (error_message != nullptr) {
            *error_message = "tail_len_out_of_range";
        }
        return false;
    }

    if ((frame_type == FrameType::kStart || frame_type == FrameType::kNormal) &&
        tail_len_bytes != static_cast<uint16_t>(kMaxPayloadBytes)) {
        if (error_message != nullptr) {
            *error_message = "tail_len_invalid_for_mid_frame";
        }
        return false;
    }

    if (header != nullptr) {
        header->frame_type = frame_type;
        header->frame_seq = frame_seq;
        header->tail_len_bytes = tail_len_bytes;
        header->payload_len_bytes =
            IsEndFrame(frame_type) ? tail_len_bytes : static_cast<uint16_t>(kMaxPayloadBytes);
        header->checkcode16 = checkcode16;
    }
    return true;
}

std::vector<bool> BytesToBitsMsbFirst(const std::vector<uint8_t>& bytes, std::size_t max_bytes) {
    if (max_bytes > bytes.size()) {
        max_bytes = bytes.size();
    }

    std::vector<bool> bits;
    bits.reserve(max_bytes * 8U);
    for (std::size_t index = 0; index < max_bytes; ++index) {
        for (int bit = 7; bit >= 0; --bit) {
            bits.push_back(((bytes[index] >> bit) & 0x01U) != 0U);
        }
    }
    return bits;
}

std::vector<uint8_t> BitsToBytesMsbFirst(const std::vector<bool>& bits) {
    std::vector<uint8_t> bytes((bits.size() + 7U) / 8U, 0U);
    for (std::size_t index = 0; index < bits.size(); ++index) {
        if (bits[index]) {
            bytes[index / 8U] |= static_cast<uint8_t>(1U << (7U - (index % 8U)));
        }
    }
    return bytes;
}

uint16_t ComputeCheckcode16(const std::vector<uint8_t>& payload,
                            uint16_t tail_len_bytes,
                            uint16_t frame_seq,
                            FrameType frame_type) {
    uint16_t result = 0;
    std::size_t index = 0;
    for (; index + 1U < payload.size(); index += 2U) {
        const uint16_t word =
            static_cast<uint16_t>((static_cast<uint16_t>(payload[index]) << 8U) | payload[index + 1U]);
        result ^= word;
    }
    if (index < payload.size()) {
        result ^= static_cast<uint16_t>(static_cast<uint16_t>(payload[index]) << 8U);
    }

    result ^= tail_len_bytes;
    result ^= frame_seq;
    uint16_t flag_word = 0;
    if (IsStartFrame(frame_type)) {
        flag_word |= 0x0002U;
    }
    if (IsEndFrame(frame_type)) {
        flag_word |= 0x0001U;
    }
    result ^= flag_word;
    return result;
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

std::string FrameTypeName(FrameType frame_type) {
    switch (frame_type) {
        case FrameType::kSingle:
            return "single";
        case FrameType::kStart:
            return "start";
        case FrameType::kNormal:
            return "normal";
        case FrameType::kEnd:
            return "end";
    }
    return "single";
}

std::string HeaderToString(const FrameHeader& header) {
    std::ostringstream stream;
    stream << "type=" << FrameTypeName(header.frame_type)
           << " seq=" << header.frame_seq
           << " payload_len=" << header.payload_len_bytes
           << " tail_len=" << header.tail_len_bytes
           << " checkcode16=0x" << std::uppercase << std::hex << std::setw(4) << std::setfill('0')
           << header.checkcode16;
    return stream.str();
}

}  // namespace protocol_v1
