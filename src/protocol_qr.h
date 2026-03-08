#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace protocol_qr {

struct GridPoint {
    int x;
    int y;
};

struct FrameHeader {
    uint16_t frame_seq = 0;
    uint8_t payload_len_bytes = 0;
    uint8_t header_version = 1;
    uint32_t crc32_payload = 0;
};

struct EncoderOptions {
    int module_pixels = 32;
    int fps = 30;
    int repeat = 2;
    int max_payload_bytes = 40;
};

constexpr char kProtocolId[] = "QRX-25-3F1A";
constexpr int kLogicalGridSize = 25;
constexpr int kModulePixels = 32;
constexpr int kQuietZoneModules = 4;
constexpr int kQuietZonePixels = kQuietZoneModules * kModulePixels;
constexpr int kPhysicalOutputPixels =
    (kLogicalGridSize + 2 * kQuietZoneModules) * kModulePixels;

constexpr int kFinderSizeModules = 7;
constexpr int kFinderReserveSizeModules = 8;
constexpr int kHeaderWidthModules = 8;
constexpr int kHeaderHeightModules = 8;
constexpr int kHeaderBits = kHeaderWidthModules * kHeaderHeightModules;
constexpr int kHeaderOriginX = 17;
constexpr int kHeaderOriginY = 8;
constexpr int kTimingStart = 8;
constexpr int kTimingEnd = 16;
constexpr int kTimingHorizontalY = 6;
constexpr int kTimingVerticalX = 6;
constexpr int kAlignmentOriginX = 18;
constexpr int kAlignmentOriginY = 18;
constexpr int kAlignmentSizeModules = 5;

constexpr int kPayloadCapacityBits = 327;
constexpr int kPayloadCapacityBytes = kPayloadCapacityBits / 8;
constexpr int kMaxPayloadBytes = 40;

bool IsInBounds(int x, int y);
bool IsFinderReserveCell(int x, int y);
bool IsHeaderCell(int x, int y);
bool IsTimingCell(int x, int y);
bool IsAlignmentCell(int x, int y);
bool IsPayloadCell(int x, int y);
std::vector<GridPoint> PayloadCells();
std::vector<bool> PackHeaderBits(const FrameHeader& header);
std::vector<bool> BytesToBitsMsbFirst(const std::vector<uint8_t>& bytes, std::size_t max_bytes);
std::vector<uint8_t> BitsToBytesMsbFirst(const std::vector<bool>& bits);
uint32_t ComputeCrc32(const std::vector<uint8_t>& bytes);
std::string HeaderToString(const FrameHeader& header);

}  // namespace protocol_qr
