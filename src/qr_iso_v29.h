#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace qr_iso_v29 {

constexpr int kVersion = 29;
constexpr int kModules = 133;
constexpr int kQuietZoneModules = 4;
constexpr int kFullModules = kModules + 2 * kQuietZoneModules;
constexpr int kDataCodewords = 911;
constexpr int kTotalCodewords = 2051;
constexpr int kMaxInputBytes = 908;

using ModuleMatrix = std::vector<uint8_t>;

bool EncodeFrameBytes(const std::vector<uint8_t>& frame_bytes,
                      ModuleMatrix* full_modules,
                      int* selected_mask = nullptr,
                      std::string* error_message = nullptr);
bool DecodeFrameBytes(const ModuleMatrix& full_modules,
                      std::vector<uint8_t>* frame_bytes,
                      int* detected_mask = nullptr,
                      std::string* error_message = nullptr);

}  // namespace qr_iso_v29
