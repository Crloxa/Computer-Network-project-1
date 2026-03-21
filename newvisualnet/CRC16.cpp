#include "CRC16.h"

namespace CRC16 {
    uint16_t calculate(const uint8_t* data, int len) {
        uint16_t crc = 0xFFFF;
        for (int i = 0; i < len; i++) {
            crc ^= (uint16_t)data[i] << 8;
            for (int j = 0; j < 8; j++) {
                crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
            }
        }
        return crc & 0xFFFF;
    }

    uint16_t calculateWithFrameInfo(const uint8_t* data, int len, Config::QRCode::FrameType type, uint32_t frameNum) {
        bool isStart = (type == Config::QRCode::FrameType::Start || type == Config::QRCode::FrameType::StartAndEnd);
        bool isEnd = (type == Config::QRCode::FrameType::End || type == Config::QRCode::FrameType::StartAndEnd);
        
        uint16_t ans = 0;
        const int cutlen = (len / 2) * 2;
        for (int i = 0; i < cutlen; i += 2) {
            ans ^= (static_cast<uint16_t>(data[i]) << 8) | data[i + 1];
        }
        if (len & 1) {
            ans ^= static_cast<uint16_t>(data[cutlen]) << 8;
        }
        ans ^= len;
        ans ^= (uint16_t)(frameNum & 0xFFFF);
        ans ^= static_cast<uint16_t>((isStart << 1) + isEnd);
        
        return ans;
    }
}