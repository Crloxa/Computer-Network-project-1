#include "qr_iso.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace qr_iso {
namespace {

constexpr int kDataCodewords = 911;
constexpr int kRawCodewords = 2051;
constexpr int kEcCodewordsPerBlock = 30;
constexpr int kGroup1Blocks = 1;
constexpr int kGroup1DataCodewords = 23;
constexpr int kGroup2Blocks = 37;
constexpr int kGroup2DataCodewords = 24;
constexpr int kTotalBlocks = kGroup1Blocks + kGroup2Blocks;
constexpr int kFormatMask = 0x5412;
constexpr int kVersionBits = 0b011101001100111111;

struct QrState {
    image_io::BitMatrix modules{kSymbolSize, kSymbolSize, false};
    image_io::BitMatrix is_function{kSymbolSize, kSymbolSize, false};
};

bool GetBit(uint32_t value, int index) {
    return ((value >> index) & 1U) != 0U;
}

void SetFunctionModule(QrState* state, int x, int y, bool value) {
    if (x < 0 || x >= kSymbolSize || y < 0 || y >= kSymbolSize) {
        return;
    }
    state->modules.set(x, y, value);
    state->is_function.set(x, y, true);
}

void DrawFinderPattern(QrState* state, int center_x, int center_y) {
    for (int dy = -4; dy <= 4; ++dy) {
        for (int dx = -4; dx <= 4; ++dx) {
            const int distance = std::max(std::abs(dx), std::abs(dy));
            const bool dark = distance != 2 && distance != 4;
            SetFunctionModule(state, center_x + dx, center_y + dy, dark);
        }
    }
}

void DrawAlignmentPattern(QrState* state, int center_x, int center_y) {
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            SetFunctionModule(state, center_x + dx, center_y + dy,
                              std::max(std::abs(dx), std::abs(dy)) != 1);
        }
    }
}

bool IsMasked(int mask, int x, int y) {
    switch (mask) {
        case 0: return ((x + y) & 1) == 0;
        case 1: return (y & 1) == 0;
        case 2: return x % 3 == 0;
        case 3: return (x + y) % 3 == 0;
        case 4: return (((y / 2) + (x / 3)) & 1) == 0;
        case 5: return ((x * y) % 2 + (x * y) % 3) == 0;
        case 6: return ((((x * y) % 2) + ((x * y) % 3)) & 1) == 0;
        case 7: return ((((x + y) & 1) + ((x * y) % 3)) & 1) == 0;
        default: return false;
    }
}

uint8_t GfMultiply(uint8_t left, uint8_t right) {
    uint16_t result = 0;
    uint16_t a = left;
    uint16_t b = right;
    while (b != 0U) {
        if ((b & 1U) != 0U) {
            result ^= a;
        }
        a <<= 1U;
        if ((a & 0x100U) != 0U) {
            a ^= 0x11DU;
        }
        b >>= 1U;
    }
    return static_cast<uint8_t>(result);
}

std::vector<uint8_t> ReedSolomonGenerator(int degree) {
    std::vector<uint8_t> result(static_cast<std::size_t>(degree), 0U);
    result[static_cast<std::size_t>(degree - 1)] = 1U;
    uint8_t root = 1U;
    for (int i = 0; i < degree; ++i) {
        for (std::size_t j = 0; j < result.size(); ++j) {
            result[j] = GfMultiply(result[j], root);
            if (j + 1U < result.size()) {
                result[j] ^= result[j + 1U];
            }
        }
        root = GfMultiply(root, 0x02U);
    }
    return result;
}

std::vector<uint8_t> ReedSolomonRemainder(const std::vector<uint8_t>& data,
                                          const std::vector<uint8_t>& generator) {
    std::vector<uint8_t> result(generator.size(), 0U);
    for (uint8_t value : data) {
        const uint8_t factor = value ^ result.front();
        std::rotate(result.begin(), result.begin() + 1, result.end());
        result.back() = 0U;
        for (std::size_t index = 0; index < result.size(); ++index) {
            result[index] ^= GfMultiply(generator[index], factor);
        }
    }
    return result;
}

void DrawVersionBits(QrState* state) {
    for (int i = 0; i < 18; ++i) {
        const bool bit = GetBit(kVersionBits, i);
        const int a = kSymbolSize - 11 + (i % 3);
        const int b = i / 3;
        SetFunctionModule(state, a, b, bit);
        SetFunctionModule(state, b, a, bit);
    }
}

void DrawFormatBits(QrState* state, int mask) {
    const int data = (0x03 << 3) | mask;
    int rem = data;
    for (int i = 0; i < 10; ++i) {
        rem = (rem << 1) ^ (((rem >> 9) & 1) * 0x537);
    }
    const int bits = ((data << 10) | rem) ^ kFormatMask;

    for (int i = 0; i <= 5; ++i) {
        SetFunctionModule(state, 8, i, GetBit(bits, i));
    }
    SetFunctionModule(state, 8, 7, GetBit(bits, 6));
    SetFunctionModule(state, 8, 8, GetBit(bits, 7));
    SetFunctionModule(state, 7, 8, GetBit(bits, 8));
    for (int i = 9; i < 15; ++i) {
        SetFunctionModule(state, 14 - i, 8, GetBit(bits, i));
    }

    for (int i = 0; i < 8; ++i) {
        SetFunctionModule(state, kSymbolSize - 1 - i, 8, GetBit(bits, i));
    }
    for (int i = 8; i < 15; ++i) {
        SetFunctionModule(state, 8, kSymbolSize - 15 + i, GetBit(bits, i));
    }
    SetFunctionModule(state, 8, kSymbolSize - 8, true);
}

void DrawFunctionPatterns(QrState* state) {
    for (int i = 0; i < kSymbolSize; ++i) {
        SetFunctionModule(state, 6, i, (i & 1) == 0);
        SetFunctionModule(state, i, 6, (i & 1) == 0);
    }

    DrawFinderPattern(state, 3, 3);
    DrawFinderPattern(state, kSymbolSize - 4, 3);
    DrawFinderPattern(state, 3, kSymbolSize - 4);

    const std::vector<int> positions = AlignmentPatternPositions();
    const int last = static_cast<int>(positions.size()) - 1;
    for (int row = 0; row < static_cast<int>(positions.size()); ++row) {
        for (int col = 0; col < static_cast<int>(positions.size()); ++col) {
            const bool skip = (row == 0 && (col == 0 || col == last)) || (row == last && col == 0);
            if (!skip) {
                DrawAlignmentPattern(state, positions[col], positions[row]);
            }
        }
    }

    DrawFormatBits(state, 0);
    DrawVersionBits(state);
}

std::vector<uint8_t> BuildDataCodewords(const std::vector<uint8_t>& bytes) {
    std::vector<bool> bits;
    bits.reserve(kDataCodewords * 8U);
    const auto appendBits = [&](uint32_t value, int count) {
        for (int bit = count - 1; bit >= 0; --bit) {
            bits.push_back(((value >> bit) & 1U) != 0U);
        }
    };

    appendBits(0x4U, 4);
    appendBits(static_cast<uint32_t>(bytes.size()), 16);
    for (uint8_t value : bytes) {
        appendBits(value, 8);
    }
    while (bits.size() < kDataCodewords * 8U && bits.size() % 8U != 0U) {
        bits.push_back(false);
    }

    static constexpr uint8_t pad_bytes[2] = {0xECU, 0x11U};
    std::vector<uint8_t> codewords;
    codewords.reserve(kDataCodewords);
    for (std::size_t offset = 0; offset < bits.size(); offset += 8U) {
        uint8_t value = 0U;
        for (std::size_t bit = 0; bit < 8U && offset + bit < bits.size(); ++bit) {
            value = static_cast<uint8_t>((value << 1U) | (bits[offset + bit] ? 1U : 0U));
        }
        codewords.push_back(value);
    }
    for (std::size_t index = codewords.size(); index < static_cast<std::size_t>(kDataCodewords); ++index) {
        codewords.push_back(pad_bytes[index & 1U]);
    }
    return codewords;
}

std::vector<uint8_t> AddEccAndInterleave(const std::vector<uint8_t>& data_codewords) {
    const std::vector<uint8_t> generator = ReedSolomonGenerator(kEcCodewordsPerBlock);
    std::vector<std::vector<uint8_t>> blocks;
    blocks.reserve(kTotalBlocks);

    std::size_t offset = 0;
    for (int block = 0; block < kGroup1Blocks; ++block) {
        std::vector<uint8_t> data(data_codewords.begin() + static_cast<std::ptrdiff_t>(offset),
                                  data_codewords.begin() + static_cast<std::ptrdiff_t>(offset + kGroup1DataCodewords));
        offset += kGroup1DataCodewords;
        std::vector<uint8_t> remainder = ReedSolomonRemainder(data, generator);
        data.push_back(0U);
        data.insert(data.end(), remainder.begin(), remainder.end());
        blocks.push_back(std::move(data));
    }
    for (int block = 0; block < kGroup2Blocks; ++block) {
        std::vector<uint8_t> data(data_codewords.begin() + static_cast<std::ptrdiff_t>(offset),
                                  data_codewords.begin() + static_cast<std::ptrdiff_t>(offset + kGroup2DataCodewords));
        offset += kGroup2DataCodewords;
        std::vector<uint8_t> remainder = ReedSolomonRemainder(data, generator);
        data.insert(data.end(), remainder.begin(), remainder.end());
        blocks.push_back(std::move(data));
    }

    std::vector<uint8_t> output;
    output.reserve(kRawCodewords);
    for (int index = 0; index < 54; ++index) {
        for (int block = 0; block < kTotalBlocks; ++block) {
            if (block < kGroup1Blocks && index == kGroup1DataCodewords) {
                continue;
            }
            output.push_back(blocks[block][index]);
        }
    }
    return output;
}

void DrawCodewords(QrState* state, const std::vector<uint8_t>& codewords) {
    int bit_index = 0;
    for (int right = kSymbolSize - 1; right >= 1; right -= 2) {
        if (right == 6) {
            --right;
        }
        for (int vertical = 0; vertical < kSymbolSize; ++vertical) {
            for (int column = 0; column < 2; ++column) {
                const int x = right - column;
                const bool upward = ((right + 1) & 2) == 0;
                const int y = upward ? (kSymbolSize - 1 - vertical) : vertical;
                if (!state->is_function.get(x, y)) {
                    bool value = false;
                    if (bit_index < static_cast<int>(codewords.size() * 8U)) {
                        value = GetBit(codewords[static_cast<std::size_t>(bit_index / 8)], 7 - (bit_index & 7));
                        ++bit_index;
                    }
                    state->modules.set(x, y, value);
                }
            }
        }
    }
}

void ApplyMask(QrState* state, int mask) {
    for (int y = 0; y < kSymbolSize; ++y) {
        for (int x = 0; x < kSymbolSize; ++x) {
            if (!state->is_function.get(x, y) && IsMasked(mask, x, y)) {
                state->modules.set(x, y, !state->modules.get(x, y));
            }
        }
    }
}

int HammingDistance(uint32_t left, uint32_t right) {
    uint32_t value = left ^ right;
    int count = 0;
    while (value != 0U) {
        value &= value - 1U;
        ++count;
    }
    return count;
}

std::optional<int> DecodeMaskFromFormat(const image_io::BitMatrix& modules) {
    uint32_t bits = 0U;
    for (int i = 0; i <= 5; ++i) {
        bits |= static_cast<uint32_t>(modules.get(8, i)) << i;
    }
    bits |= static_cast<uint32_t>(modules.get(8, 7)) << 6U;
    bits |= static_cast<uint32_t>(modules.get(8, 8)) << 7U;
    bits |= static_cast<uint32_t>(modules.get(7, 8)) << 8U;
    for (int i = 9; i < 15; ++i) {
        bits |= static_cast<uint32_t>(modules.get(14 - i, 8)) << i;
    }

    int best_distance = 16;
    int best_mask = -1;
    for (int mask = 0; mask < 8; ++mask) {
        const int data = (0x03 << 3) | mask;
        int rem = data;
        for (int i = 0; i < 10; ++i) {
            rem = (rem << 1) ^ (((rem >> 9) & 1) * 0x537);
        }
        const uint32_t candidate = static_cast<uint32_t>(((data << 10) | rem) ^ kFormatMask);
        const int distance = HammingDistance(bits, candidate);
        if (distance < best_distance) {
            best_distance = distance;
            best_mask = mask;
        }
    }
    if (best_distance > 3) {
        return std::nullopt;
    }
    return best_mask;
}

std::vector<uint8_t> ReadRawCodewords(image_io::BitMatrix modules, int mask) {
    QrState function_state;
    DrawFunctionPatterns(&function_state);

    for (int y = 0; y < kSymbolSize; ++y) {
        for (int x = 0; x < kSymbolSize; ++x) {
            if (!function_state.is_function.get(x, y) && IsMasked(mask, x, y)) {
                modules.set(x, y, !modules.get(x, y));
            }
        }
    }

    std::vector<bool> bits;
    bits.reserve(kRawCodewords * 8U);
    for (int right = kSymbolSize - 1; right >= 1; right -= 2) {
        if (right == 6) {
            --right;
        }
        for (int vertical = 0; vertical < kSymbolSize; ++vertical) {
            for (int column = 0; column < 2; ++column) {
                const int x = right - column;
                const bool upward = ((right + 1) & 2) == 0;
                const int y = upward ? (kSymbolSize - 1 - vertical) : vertical;
                if (!function_state.is_function.get(x, y)) {
                    bits.push_back(modules.get(x, y));
                }
            }
        }
    }

    std::vector<uint8_t> raw(static_cast<std::size_t>(kRawCodewords), 0U);
    for (std::size_t index = 0; index < raw.size() * 8U; ++index) {
        if (bits[index]) {
            raw[index / 8U] |= static_cast<uint8_t>(1U << (7U - (index & 7U)));
        }
    }
    return raw;
}

std::vector<uint8_t> ExtractDataCodewords(const std::vector<uint8_t>& raw) {
    std::vector<std::vector<uint8_t>> blocks(kTotalBlocks, std::vector<uint8_t>(54, 0U));
    std::size_t offset = 0;
    for (int index = 0; index < 54; ++index) {
        for (int block = 0; block < kTotalBlocks; ++block) {
            if (block < kGroup1Blocks && index == kGroup1DataCodewords) {
                continue;
            }
            blocks[block][index] = raw[offset++];
        }
    }

    std::vector<uint8_t> data;
    data.reserve(kDataCodewords);
    for (int block = 0; block < kGroup1Blocks; ++block) {
        data.insert(data.end(), blocks[block].begin(), blocks[block].begin() + kGroup1DataCodewords);
    }
    for (int block = kGroup1Blocks; block < kTotalBlocks; ++block) {
        data.insert(data.end(), blocks[block].begin(), blocks[block].begin() + kGroup2DataCodewords);
    }
    return data;
}

std::vector<uint8_t> ParseByteModePayload(const std::vector<uint8_t>& data_codewords, std::string* error_message) {
    std::vector<bool> bits;
    bits.reserve(data_codewords.size() * 8U);
    for (uint8_t value : data_codewords) {
        for (int bit = 7; bit >= 0; --bit) {
            bits.push_back(((value >> bit) & 1U) != 0U);
        }
    }

    auto readBits = [&](std::size_t offset, int count) -> std::optional<uint32_t> {
        if (offset + static_cast<std::size_t>(count) > bits.size()) {
            return std::nullopt;
        }
        uint32_t value = 0U;
        for (int bit = 0; bit < count; ++bit) {
            value = (value << 1U) | (bits[offset + static_cast<std::size_t>(bit)] ? 1U : 0U);
        }
        return value;
    };

    const std::optional<uint32_t> mode = readBits(0U, 4);
    if (!mode.has_value() || mode.value() != 0x4U) {
        if (error_message != nullptr) {
            *error_message = "Unsupported QR mode; expected byte mode.";
        }
        return {};
    }
    const std::optional<uint32_t> length = readBits(4U, 16);
    if (!length.has_value()) {
        if (error_message != nullptr) {
            *error_message = "Failed to read QR byte-mode length.";
        }
        return {};
    }

    const std::size_t byte_count = static_cast<std::size_t>(length.value());
    if (byte_count > static_cast<std::size_t>(kMaxFrameBytes)) {
        if (error_message != nullptr) {
            *error_message = "Decoded QR payload exceeds supported frame bytes.";
        }
        return {};
    }

    std::vector<uint8_t> output;
    output.reserve(byte_count);
    std::size_t offset = 20U;
    for (std::size_t index = 0; index < byte_count; ++index) {
        const std::optional<uint32_t> value = readBits(offset, 8);
        if (!value.has_value()) {
            if (error_message != nullptr) {
                *error_message = "QR data stream ended before declared byte count.";
            }
            return {};
        }
        output.push_back(static_cast<uint8_t>(value.value()));
        offset += 8U;
    }
    return output;
}

}  // namespace

std::vector<int> AlignmentPatternPositions() {
    return {6, 30, 54, 78, 102, 126};
}

bool EncodeBytes(const std::vector<uint8_t>& frame_bytes,
                 image_io::BitMatrix* modules,
                 int* selected_mask,
                 std::string* error_message) {
    if (frame_bytes.size() > static_cast<std::size_t>(kMaxFrameBytes)) {
        if (error_message != nullptr) {
            *error_message = "Frame bytes exceed Version 29 + ECC Q byte-mode capacity.";
        }
        return false;
    }

    QrState state;
    DrawFunctionPatterns(&state);
    const std::vector<uint8_t> data_codewords = BuildDataCodewords(frame_bytes);
    const std::vector<uint8_t> raw_codewords = AddEccAndInterleave(data_codewords);
    DrawCodewords(&state, raw_codewords);

    const int mask = 0;
    ApplyMask(&state, mask);
    DrawFormatBits(&state, mask);

    if (modules != nullptr) {
        *modules = state.modules;
    }
    if (selected_mask != nullptr) {
        *selected_mask = mask;
    }
    return true;
}

DecodeResult DecodeModules(const image_io::BitMatrix& modules) {
    DecodeResult result;
    if (modules.width() != kSymbolSize || modules.height() != kSymbolSize) {
        result.message = "QR module matrix size mismatch.";
        return result;
    }

    const std::optional<int> mask = DecodeMaskFromFormat(modules);
    if (!mask.has_value()) {
        result.message = "Failed to decode QR format information.";
        return result;
    }

    const std::vector<uint8_t> raw = ReadRawCodewords(modules, mask.value());
    const std::vector<uint8_t> data = ExtractDataCodewords(raw);
    std::string parse_error;
    std::vector<uint8_t> payload = ParseByteModePayload(data, &parse_error);
    if (payload.empty() && !parse_error.empty()) {
        result.message = parse_error;
        return result;
    }

    result.success = true;
    result.bytes = std::move(payload);
    result.mask = mask.value();
    result.message = "Decoded Version 29 + ECC Q QR modules.";
    return result;
}

}  // namespace qr_iso
