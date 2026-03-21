#pragma once
#include <cstdint>
#include "Config.h"

namespace CRC16 {
    uint16_t calculate(const uint8_t* data, int len);
    uint16_t calculateWithFrameInfo(const uint8_t* data, int len, Config::QRCode::FrameType type, uint32_t frameNum);
}