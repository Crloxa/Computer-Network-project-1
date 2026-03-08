#include "protocol_v1.h"

#include <iomanip>
#include <sstream>

namespace protocol_v1 {
namespace {

void AppendBitsBigEndian(uint64_t value, int bit_count, std::vector<bool>& bits) {
    for (int bit = bit_count - 1; bit >= 0; --bit) {
        bits.push_back(((value >> bit) & 1ULL) != 0ULL);
    }
}

bool IsInsideRect(int x, int y, int origin_x, int origin_y, int width, int height) {
    return x >= origin_x && x < (origin_x + width) && y >= origin_y && y < (origin_y + height);
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

std::pair<int, int> HeaderOrigin() {
    return {kHeaderOriginX, kHeaderOriginY};
}

std::pair<int, int> HeaderDimensions() {
    return {kHeaderWidthModules, kHeaderHeightModules};
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
    std::vector<bool> primary_bits;
    primary_bits.reserve(64);
    AppendBitsBigEndian(header.frame_seq, 16, primary_bits);
    AppendBitsBigEndian(header.payload_len_bytes, 16, primary_bits);
    AppendBitsBigEndian(header.crc32_payload, 32, primary_bits);

    std::vector<bool> header_bits;
    header_bits.reserve(kHeaderBits);
    header_bits.insert(header_bits.end(), primary_bits.begin(), primary_bits.end());
    header_bits.insert(header_bits.end(), primary_bits.begin(), primary_bits.end());

    const uint32_t header_crc = ComputeCrc32(BitsToBytesMsbFirst(header_bits));
    AppendBitsBigEndian(header_crc, 32, header_bits);
    return header_bits;
}

std::vector<bool> BytesToBitsMsbFirst(const std::vector<uint8_t>& bytes, std::size_t max_bytes) {
    if (max_bytes > bytes.size()) {
        max_bytes = bytes.size();
    }

    std::vector<bool> bits;
    bits.reserve(max_bytes * 8);
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
    stream << "seq=" << header.frame_seq
           << " len=" << header.payload_len_bytes
           << " payload_crc=0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
           << header.crc32_payload
           << " header_crc=0x" << std::setw(8) << header.crc32_header;
    return stream.str();
}

}  // namespace protocol_v1
