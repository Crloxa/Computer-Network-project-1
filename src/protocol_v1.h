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

enum class FrameType {
    kSingle,
    kStart,
    kNormal,
    kEnd,
};

struct FrameHeader {
    FrameType frame_type = FrameType::kSingle;
    uint16_t frame_seq = 0;
    uint16_t tail_len_bytes = 0;
    uint16_t payload_len_bytes = 0;
    uint16_t checkcode16 = 0;
};

constexpr char kProtocolId[] = "V1.7-133-4F";
constexpr int kLogicalGridSize = 133;
constexpr int kModulePixels = 8;
constexpr int kLogicalRenderPixels = kLogicalGridSize * kModulePixels;
constexpr int kQuietZoneModules = 4;
constexpr int kQuietZonePixels = kQuietZoneModules * kModulePixels;
constexpr int kPhysicalOutputPixels = kLogicalRenderPixels + kQuietZonePixels * 2;
constexpr int kFinderSizeModules = 9;
constexpr int kFinderReserveSizeModules = 10;
constexpr int kHeaderWidthModules = 20;
constexpr int kHeaderHeightModules = 3;
constexpr int kHeaderBits = kHeaderWidthModules * kHeaderHeightModules;
constexpr int kHeaderUsedRows = 3;
constexpr int kHeaderUsedBits = kHeaderWidthModules * kHeaderUsedRows;
constexpr int kHeaderOriginX = 10;
constexpr int kHeaderOriginY = 10;
constexpr int kTimingStart = 10;
constexpr int kTimingEnd = 122;
constexpr int kTimingHorizontalY = 30;
constexpr int kTimingVerticalX = 30;
constexpr int kAuxLocatorOriginX = 108;
constexpr int kAuxLocatorOriginY = 108;
constexpr int kAuxLocatorSizeModules = 7;
constexpr int kAuxLocatorReserveOriginX = 107;
constexpr int kAuxLocatorReserveOriginY = 107;
constexpr int kAuxLocatorReserveSizeModules = 9;
constexpr int kFinderTopLeftMin = 0;
constexpr int kFinderTopLeftMax = 9;
constexpr int kFinderTopRightMin = 123;
constexpr int kFinderTopRightMax = 132;
constexpr int kFinderBottomLeftMin = 123;
constexpr int kFinderBottomLeftMax = 132;
constexpr int kPayloadCapacityBits = 17023;
constexpr int kPayloadCapacityBytes = kPayloadCapacityBits / 8;
constexpr int kMaxPayloadBytes = kPayloadCapacityBytes;

struct EncoderOptions {
    int fps = 60;
    int repeat = 3;
    int max_payload_bytes = kMaxPayloadBytes;
};

bool IsInBounds(int x, int y);
bool IsHeaderCell(int x, int y);
bool IsFinderReserveCell(int x, int y);
bool IsTimingCell(int x, int y);
bool IsAuxLocatorReserveCell(int x, int y);
bool IsPayloadCell(int x, int y);
bool IsStartFrame(FrameType frame_type);
bool IsEndFrame(FrameType frame_type);
std::pair<int, int> HeaderOrigin();
std::pair<int, int> HeaderDimensions();
std::vector<GridPoint> HeaderCells();
std::vector<GridPoint> PayloadCells();
std::vector<bool> PackHeaderBits(const FrameHeader& header);
bool ParseHeaderBits(const std::vector<bool>& bits,
                     FrameHeader* header,
                     std::string* error_message = nullptr);
std::vector<bool> BytesToBitsMsbFirst(const std::vector<uint8_t>& bytes, std::size_t max_bytes);
std::vector<uint8_t> BitsToBytesMsbFirst(const std::vector<bool>& bits);
uint16_t ComputeCheckcode16(const std::vector<uint8_t>& payload,
                            uint16_t tail_len_bytes,
                            uint16_t frame_seq,
                            FrameType frame_type);
uint32_t ComputeCrc32(const std::vector<uint8_t>& bytes);
std::string FrameTypeName(FrameType frame_type);
std::string HeaderToString(const FrameHeader& header);

}  // namespace protocol_v1
