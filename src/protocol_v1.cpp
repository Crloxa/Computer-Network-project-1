#include "protocol_v1.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace protocol_v1 {
namespace {

void AppendBitsBigEndian(uint64_t value, int bit_count, std::vector<bool>& bits) {
    for (int bit = bit_count - 1; bit >= 0; --bit) {
        bits.push_back(((value >> bit) & 1ULL) != 0ULL);
    }
}

}  // namespace

bool IsInBounds(int x, int y) {
    return x >= 0 && x < kLogicalGridSize && y >= 0 && y < kLogicalGridSize;
}

bool IsHeaderCell(int x, int y) {
    return x >= kHeaderOriginX && x < (kHeaderOriginX + kHeaderSizeModules) &&
           y >= kHeaderOriginY && y < (kHeaderOriginY + kHeaderSizeModules);
}

bool IsPayloadCell(int x, int y) {
    if (!IsInBounds(x, y)) {
        return false;
    }
    const bool in_inner_area = x >= kInnerStart && x <= kInnerEnd && y >= kInnerStart && y <= kInnerEnd;
    return in_inner_area && !IsHeaderCell(x, y);
}

std::pair<int, int> HeaderOrigin() {
    return {kHeaderOriginX, kHeaderOriginY};
}

std::vector<GridPoint> PayloadCells() {
    std::vector<GridPoint> cells;
    cells.reserve(kPayloadCapacityBits);
    for (int y = kInnerStart; y <= kInnerEnd; ++y) {
        for (int x = kInnerStart; x <= kInnerEnd; ++x) {
            if (IsPayloadCell(x, y)) {
                cells.push_back({x, y});
            }
        }
    }
    return cells;
}

std::vector<bool> PackHeaderBits(const FrameHeader& header) {
    std::vector<bool> bits;
    bits.reserve(64);
    AppendBitsBigEndian(header.frame_seq, 16, bits);
    AppendBitsBigEndian(header.payload_len_bytes, 16, bits);
    AppendBitsBigEndian(header.crc32_payload, 32, bits);
    return bits;
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
           << " crc=0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
           << header.crc32_payload;
    return stream.str();
}

}  // namespace protocol_v1
