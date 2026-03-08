#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace protocol_v1 {

struct GridPoint {
    int x;
    int y;
};

struct FrameHeader {
    uint16_t frame_seq = 0;
    uint16_t payload_len_bytes = 0;
    uint32_t crc32_payload = 0;
};

struct EncoderOptions {
    int logical_grid_size = 108;
    int module_pixels = 10;
    int fps = 60;
    int repeat = 3;
    int max_payload_bytes = 1024;
};

constexpr int kLogicalGridSize = 108;
constexpr int kQuietZoneModules = 4;
constexpr int kFinderSizeModules = 7;
constexpr int kFinderSeparatorModules = 1;
constexpr int kHeaderSizeModules = 8;
constexpr int kInnerStart = kQuietZoneModules + kFinderSizeModules + kFinderSeparatorModules;
constexpr int kInnerEnd = kLogicalGridSize - kQuietZoneModules - 1;
constexpr int kInnerSize = kInnerEnd - kInnerStart + 1;
constexpr int kHeaderOriginX = kInnerStart;
constexpr int kHeaderOriginY = kInnerStart;
constexpr int kPayloadCapacityBits = kInnerSize * kInnerSize - kHeaderSizeModules * kHeaderSizeModules;
constexpr int kPayloadCapacityBytes = kPayloadCapacityBits / 8;
constexpr int kMaxPayloadBytes = 1024;

bool IsInBounds(int x, int y);
bool IsHeaderCell(int x, int y);
bool IsPayloadCell(int x, int y);
std::pair<int, int> HeaderOrigin();
std::vector<GridPoint> PayloadCells();
std::vector<bool> PackHeaderBits(const FrameHeader& header);
std::vector<bool> BytesToBitsMsbFirst(const std::vector<uint8_t>& bytes, std::size_t max_bytes);
uint32_t ComputeCrc32(const std::vector<uint8_t>& bytes);
std::string HeaderToString(const FrameHeader& header);

}  // namespace protocol_v1
