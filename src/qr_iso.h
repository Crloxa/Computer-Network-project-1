#pragma once

#include "simple_image.h"

#include <string>
#include <vector>

namespace qr_iso {

struct DecodeResult {
    bool success = false;
    std::vector<uint8_t> bytes;
    int mask = -1;
    std::string message;
};

constexpr int kVersion = 29;
constexpr int kSymbolSize = 133;
constexpr int kQuietZoneModules = 4;
constexpr int kRenderSize = kSymbolSize + 2 * kQuietZoneModules;
constexpr int kMaxFrameBytes = 908;

bool EncodeBytes(const std::vector<uint8_t>& frame_bytes,
                 image_io::BitMatrix* modules,
                 int* selected_mask = nullptr,
                 std::string* error_message = nullptr);

DecodeResult DecodeModules(const image_io::BitMatrix& modules);
std::vector<int> AlignmentPatternPositions();

}  // namespace qr_iso
