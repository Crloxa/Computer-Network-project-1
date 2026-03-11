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

struct EncoderOptions {
    int fps = 60;
    int repeat = 3;
    int max_payload_bytes = 1024;
};

constexpr char kProtocolId[] = "V1.6-108-4F";
constexpr int kLogicalGridSize = 108;
constexpr int kPhysicalOutputPixels = 1080;
constexpr int kModulePixels = 9;
constexpr int kLogicalRenderPixels = kLogicalGridSize * kModulePixels;
constexpr int kQuietZoneModules = 6;
constexpr int kQuietZonePixels = kQuietZoneModules * kModulePixels;
constexpr int kFinderSizeModules = 7;
constexpr int kFinderReserveSizeModules = 8;
constexpr int kHeaderWidthModules = 16;
constexpr int kHeaderHeightModules = 10;
constexpr int kHeaderBits = kHeaderWidthModules * kHeaderHeightModules;
constexpr int kHeaderUsedRows = 3;
constexpr int kHeaderUsedBits = kHeaderWidthModules * kHeaderUsedRows;
constexpr int kHeaderOriginX = 8;
constexpr int kHeaderOriginY = 8;
constexpr int kTimingStart = 8;
constexpr int kTimingEnd = 99;
constexpr int kTimingHorizontalY = 24;
constexpr int kTimingVerticalX = 24;
constexpr int kAlignmentOriginX = 88;
constexpr int kAlignmentOriginY = 88;
constexpr int kAlignmentSizeModules = 5;
constexpr int kFinderTopLeftMin = 0;
constexpr int kFinderTopLeftMax = 7;
constexpr int kFinderTopRightMin = 100;
constexpr int kFinderTopRightMax = 107;
constexpr int kFinderBottomLeftMin = 100;
constexpr int kFinderBottomLeftMax = 107;
constexpr int kPayloadCapacityBits = 11040;
constexpr int kPayloadCapacityBytes = kPayloadCapacityBits / 8;
constexpr int kMaxPayloadBytes = 1024;

bool IsInBounds(int x, int y);
bool IsHeaderCell(int x, int y);
bool IsFinderReserveCell(int x, int y);
bool IsTimingCell(int x, int y);
bool IsAlignmentCell(int x, int y);
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
