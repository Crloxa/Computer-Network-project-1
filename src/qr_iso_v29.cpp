#include "qr_iso_v29.h"
#include "simple_image.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace qr_iso_v29 {
namespace {

constexpr int kRawCodewords = kTotalCodewords;
constexpr int kEcCodewordsPerBlock = 30;
constexpr int kGroup1Blocks = 1;
constexpr int kGroup1DataCodewords = 23;
constexpr int kGroup2Blocks = 37;
constexpr int kGroup2DataCodewords = 24;
constexpr int kTotalBlocks = kGroup1Blocks + kGroup2Blocks;
constexpr int kFormatBitsQ = 0x03;
constexpr int kFormatBchGenerator = 0x537;
constexpr int kFormatMask = 0x5412;
constexpr int kVersionBchGenerator = 0x1F25;
constexpr int kCharacterCountBits = 16;
constexpr int kSymbolSize = kModules;
constexpr int kMaxFrameBytes = kMaxInputBytes;
constexpr std::array<int, 6> kAlignmentCenters = {6, 30, 54, 78, 102, 126};

struct QrState {
    image_io::BitMatrix modules{kSymbolSize, kSymbolSize, false};
    image_io::BitMatrix is_function{kSymbolSize, kSymbolSize, false};
};

void SetError(const std::string& message, std::string* error_message) {
    if (error_message != nullptr) {
        *error_message = message;
    }
}

void AppendBits(uint32_t value, int bit_count, std::vector<uint8_t>* bits) {
    for (int bit = bit_count - 1; bit >= 0; --bit) {
        bits->push_back(((value >> bit) & 1U) != 0U ? 1U : 0U);
    }
}

const std::array<uint8_t, 512>& ExpTable() {
    static const std::array<uint8_t, 512> table = [] {
        std::array<uint8_t, 512> values = {};
        values[0] = 1U;
        for (int index = 1; index < 255; ++index) {
            uint16_t value = static_cast<uint16_t>(values[index - 1]) << 1U;
            if ((value & 0x100U) != 0U) {
                value ^= 0x11DU;
            }
            values[index] = static_cast<uint8_t>(value);
        }
        for (int index = 255; index < 512; ++index) {
            values[index] = values[index - 255];
        }
        return values;
    }();
    return table;
}

const std::array<int16_t, 256>& LogTable() {
    static const std::array<int16_t, 256> table = [] {
        std::array<int16_t, 256> values = {};
        values.fill(-1);
        const auto& exp = ExpTable();
        for (int index = 0; index < 255; ++index) {
            values[exp[index]] = static_cast<int16_t>(index);
        }
        return values;
    }();
    return table;
}

uint8_t Gexp(int value) {
    return ExpTable()[static_cast<std::size_t>(value % 255)];
}

int Glog(uint8_t value) {
    return LogTable()[value];
}

std::vector<uint8_t> PolyMultiply(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right) {
    std::vector<uint8_t> result(left.size() + right.size() - 1U, 0U);
    for (std::size_t left_index = 0; left_index < left.size(); ++left_index) {
        for (std::size_t right_index = 0; right_index < right.size(); ++right_index) {
            if (left[left_index] == 0U || right[right_index] == 0U) {
                continue;
            }
            result[left_index + right_index] ^= Gexp(Glog(left[left_index]) + Glog(right[right_index]));
        }
    }
    return result;
}

const std::vector<uint8_t>& GeneratorPolynomial() {
    static const std::vector<uint8_t> generator = [] {
        std::vector<uint8_t> polynomial = {1U};
        for (int index = 0; index < kEcCodewordsPerBlock; ++index) {
            polynomial = PolyMultiply(polynomial, {1U, Gexp(index)});
        }
        return polynomial;
    }();
    return generator;
}

std::vector<uint8_t> ComputeErrorCodewords(const std::vector<uint8_t>& data_codewords) {
    // 这里按固定的 Version 29 / ECC Q 分组规则计算每个 RS block 的冗余码字。
    std::vector<uint8_t> remainder(static_cast<std::size_t>(kEcCodewordsPerBlock), 0U);
    const std::vector<uint8_t>& generator = GeneratorPolynomial();
    for (uint8_t value : data_codewords) {
        const uint8_t factor = value ^ remainder.front();
        remainder.erase(remainder.begin());
        remainder.push_back(0U);
        if (factor == 0U) {
            continue;
        }
        for (int index = 0; index < kEcCodewordsPerBlock; ++index) {
            remainder[static_cast<std::size_t>(index)] ^=
                Gexp(Glog(generator[static_cast<std::size_t>(index + 1)]) + Glog(factor));
        }
    }
    return remainder;
}

uint32_t ComputeBchRemainder(uint32_t value, int bit_count, uint32_t generator) {
    int generator_degree = 0;
    for (uint32_t temp = generator; temp > 0U; temp >>= 1U) {
        ++generator_degree;
    }
    --generator_degree;

    value <<= generator_degree;
    for (int bit = bit_count + generator_degree - 1; bit >= generator_degree; --bit) {
        if (((value >> bit) & 1U) != 0U) {
            value ^= generator << (bit - generator_degree);
        }
    }
    return value;
}

uint32_t VersionBits() {
    const uint32_t value = static_cast<uint32_t>(kVersion);
    return (value << 12U) | ComputeBchRemainder(value, 6, kVersionBchGenerator);
}

uint16_t FormatBits(int mask) {
    const uint16_t value = static_cast<uint16_t>((kFormatBitsQ << 3) | mask);
    const uint16_t remainder = static_cast<uint16_t>(ComputeBchRemainder(value, 5, kFormatBchGenerator));
    return static_cast<uint16_t>(((value << 10U) | remainder) ^ kFormatMask);
}

std::vector<uint8_t> CreateDataCodewords(const std::vector<uint8_t>& frame_bytes,
                                         std::string* error_message) {
    // 当前首版只支持 Byte mode，因此这里固定写入 mode、长度、payload 和 pad codewords。
    std::vector<uint8_t> bits;
    bits.reserve(static_cast<std::size_t>(kDataCodewords * 8));
    AppendBits(0b0100U, 4, &bits);
    AppendBits(static_cast<uint32_t>(frame_bytes.size()), kCharacterCountBits, &bits);
    for (uint8_t byte : frame_bytes) {
        AppendBits(byte, 8, &bits);
    }

    const int bit_limit = kDataCodewords * 8;
    if (static_cast<int>(bits.size()) > bit_limit) {
        SetError("Frame bytes exceed the fixed ISO QR v29/Q capacity.", error_message);
        return {};
    }
    for (int index = 0; index < std::min(4, bit_limit - static_cast<int>(bits.size())); ++index) {
        bits.push_back(0U);
    }
    while ((bits.size() % 8U) != 0U) {
        bits.push_back(0U);
    }

    std::vector<uint8_t> codewords;
    codewords.reserve(kDataCodewords);
    for (std::size_t offset = 0; offset < bits.size(); offset += 8U) {
        uint8_t value = 0U;
        for (std::size_t bit = 0; bit < 8U; ++bit) {
            value = static_cast<uint8_t>((value << 1U) | bits[offset + bit]);
        }
        codewords.push_back(value);
    }

    static constexpr uint8_t kPadWords[] = {0xECU, 0x11U};
    for (int pad_index = 0; static_cast<int>(codewords.size()) < kDataCodewords; ++pad_index) {
        codewords.push_back(kPadWords[pad_index % 2]);
    }
    return codewords;
}

std::vector<uint8_t> InterleaveCodewords(const std::vector<uint8_t>& data_codewords) {
    std::vector<std::vector<uint8_t>> data_blocks;
    std::vector<std::vector<uint8_t>> ecc_blocks;
    data_blocks.reserve(kTotalBlocks);
    ecc_blocks.reserve(kTotalBlocks);

    std::size_t offset = 0U;
    for (int index = 0; index < kGroup1Blocks; ++index) {
        std::vector<uint8_t> block(data_codewords.begin() + static_cast<std::ptrdiff_t>(offset),
                                   data_codewords.begin() + static_cast<std::ptrdiff_t>(offset + kGroup1DataCodewords));
        offset += static_cast<std::size_t>(kGroup1DataCodewords);
        data_blocks.push_back(block);
        ecc_blocks.push_back(ComputeErrorCodewords(block));
    }
    for (int index = 0; index < kGroup2Blocks; ++index) {
        std::vector<uint8_t> block(data_codewords.begin() + static_cast<std::ptrdiff_t>(offset),
                                   data_codewords.begin() + static_cast<std::ptrdiff_t>(offset + kGroup2DataCodewords));
        offset += static_cast<std::size_t>(kGroup2DataCodewords);
        data_blocks.push_back(block);
        ecc_blocks.push_back(ComputeErrorCodewords(block));
    }

    std::vector<uint8_t> interleaved;
    interleaved.reserve(kRawCodewords);
    for (int byte_index = 0; byte_index < kGroup2DataCodewords; ++byte_index) {
        for (const auto& block : data_blocks) {
            if (byte_index < static_cast<int>(block.size())) {
                interleaved.push_back(block[static_cast<std::size_t>(byte_index)]);
            }
        }
    }
    for (int byte_index = 0; byte_index < kEcCodewordsPerBlock; ++byte_index) {
        for (const auto& block : ecc_blocks) {
            interleaved.push_back(block[static_cast<std::size_t>(byte_index)]);
        }
    }
    return interleaved;
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
            const int x = center_x + dx;
            const int y = center_y + dy;
            const int distance = std::max(std::abs(dx), std::abs(dy));
            const bool dark = distance == 4 || distance == 2 || distance == 1 || distance == 0;
            SetFunctionModule(state, x, y, dark);
        }
    }
}

void DrawAlignmentPattern(QrState* state, int center_x, int center_y) {
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            const int distance = std::max(std::abs(dx), std::abs(dy));
            SetFunctionModule(state, center_x + dx, center_y + dy, distance != 1);
        }
    }
}

void DrawFunctionPatterns(QrState* state) {
    // 功能区全部按 Version 29 的固定布局写入，后续写数据时必须整块跳过。
    DrawFinderPattern(state, 3, 3);
    DrawFinderPattern(state, kSymbolSize - 4, 3);
    DrawFinderPattern(state, 3, kSymbolSize - 4);

    for (int index = 0; index < kSymbolSize; ++index) {
        if (!state->is_function.get(index, 6)) {
            SetFunctionModule(state, index, 6, index % 2 == 0);
        }
        if (!state->is_function.get(6, index)) {
            SetFunctionModule(state, 6, index, index % 2 == 0);
        }
    }

    for (int y : kAlignmentCenters) {
        for (int x : kAlignmentCenters) {
            const bool overlaps_finder =
                (x < 9 && y < 9) ||
                (x > kSymbolSize - 9 && y < 9) ||
                (x < 9 && y > kSymbolSize - 9);
            if (!overlaps_finder) {
                DrawAlignmentPattern(state, x, y);
            }
        }
    }

    SetFunctionModule(state, 8, 4 * kVersion + 9, true);

    for (int index = 0; index < 9; ++index) {
        state->is_function.set(8, index, true);
        state->is_function.set(index, 8, true);
        state->is_function.set(8, kSymbolSize - 1 - index, true);
        state->is_function.set(kSymbolSize - 1 - index, 8, true);
    }
    for (int y = 0; y < 6; ++y) {
        for (int x = kSymbolSize - 11; x < kSymbolSize - 8; ++x) {
            state->is_function.set(x, y, true);
            state->is_function.set(y, x, true);
        }
    }
}

void DrawVersionInfo(QrState* state) {
    const uint32_t bits = VersionBits();
    for (int index = 0; index < 18; ++index) {
        const bool value = ((bits >> index) & 1U) != 0U;
        const int a = index % 3;
        const int b = index / 3;
        SetFunctionModule(state, kSymbolSize - 11 + a, b, value);
        SetFunctionModule(state, b, kSymbolSize - 11 + a, value);
    }
}

void DrawFormatInfo(QrState* state, int mask) {
    const uint16_t bits = FormatBits(mask);
    static constexpr std::array<std::pair<int, int>, 15> kPrimary = {
        std::pair<int, int>{8, 0}, {8, 1}, {8, 2}, {8, 3}, {8, 4},
        {8, 5}, {8, 7}, {8, 8}, {7, 8}, {5, 8},
        {4, 8}, {3, 8}, {2, 8}, {1, 8}, {0, 8},
    };
    static constexpr std::array<std::pair<int, int>, 15> kSecondary = {
        std::pair<int, int>{kSymbolSize - 1, 8}, {kSymbolSize - 2, 8}, {kSymbolSize - 3, 8}, {kSymbolSize - 4, 8},
        {kSymbolSize - 5, 8}, {kSymbolSize - 6, 8}, {kSymbolSize - 7, 8}, {8, kSymbolSize - 8},
        {8, kSymbolSize - 7}, {8, kSymbolSize - 6}, {8, kSymbolSize - 5}, {8, kSymbolSize - 4},
        {8, kSymbolSize - 3}, {8, kSymbolSize - 2}, {8, kSymbolSize - 1},
    };
    for (int index = 0; index < 15; ++index) {
        const bool value = ((bits >> index) & 1U) != 0U;
        SetFunctionModule(state, kPrimary[static_cast<std::size_t>(index)].first, kPrimary[static_cast<std::size_t>(index)].second, value);
        SetFunctionModule(state, kSecondary[static_cast<std::size_t>(index)].first, kSecondary[static_cast<std::size_t>(index)].second, value);
    }
}

bool MaskAt(int mask, int x, int y) {
    switch (mask) {
        case 0: return (x + y) % 2 == 0;
        case 1: return y % 2 == 0;
        case 2: return x % 3 == 0;
        case 3: return (x + y) % 3 == 0;
        case 4: return ((y / 2) + (x / 3)) % 2 == 0;
        case 5: return ((x * y) % 2) + ((x * y) % 3) == 0;
        case 6: return ((((x * y) % 2) + ((x * y) % 3)) % 2) == 0;
        case 7: return ((((x + y) % 2) + ((x * y) % 3)) % 2) == 0;
        default: return false;
    }
}

void DrawCodewords(QrState* state, const std::vector<uint8_t>& codewords) {
    int bit_index = 0;
    int x = kSymbolSize - 1;
    int y = kSymbolSize - 1;
    int direction = -1;
    while (x > 0) {
        if (x == 6) {
            --x;
        }
        while (y >= 0 && y < kSymbolSize) {
            for (int dx = 0; dx < 2; ++dx) {
                const int column = x - dx;
                if (state->is_function.get(column, y)) {
                    continue;
                }
                bool value = false;
                if (bit_index < static_cast<int>(codewords.size() * 8U)) {
                    value = ((codewords[static_cast<std::size_t>(bit_index / 8)] >> (7 - (bit_index % 8))) & 1U) != 0U;
                    ++bit_index;
                }
                state->modules.set(column, y, value);
            }
            y += direction;
        }
        y -= direction;
        direction = -direction;
        x -= 2;
    }
}

void ApplyMask(QrState* state, int mask) {
    for (int y = 0; y < kSymbolSize; ++y) {
        for (int x = 0; x < kSymbolSize; ++x) {
            if (!state->is_function.get(x, y) && MaskAt(mask, x, y)) {
                state->modules.set(x, y, !state->modules.get(x, y));
            }
        }
    }
}

int PenaltyRuns(const image_io::BitMatrix& matrix) {
    int penalty = 0;
    for (int y = 0; y < matrix.height(); ++y) {
        int run_length = 1;
        bool last = matrix.get(0, y);
        for (int x = 1; x < matrix.width(); ++x) {
            const bool value = matrix.get(x, y);
            if (value == last) {
                ++run_length;
            } else {
                if (run_length >= 5) {
                    penalty += 3 + (run_length - 5);
                }
                last = value;
                run_length = 1;
            }
        }
        if (run_length >= 5) {
            penalty += 3 + (run_length - 5);
        }
    }
    for (int x = 0; x < matrix.width(); ++x) {
        int run_length = 1;
        bool last = matrix.get(x, 0);
        for (int y = 1; y < matrix.height(); ++y) {
            const bool value = matrix.get(x, y);
            if (value == last) {
                ++run_length;
            } else {
                if (run_length >= 5) {
                    penalty += 3 + (run_length - 5);
                }
                last = value;
                run_length = 1;
            }
        }
        if (run_length >= 5) {
            penalty += 3 + (run_length - 5);
        }
    }
    return penalty;
}

int PenaltyBlocks(const image_io::BitMatrix& matrix) {
    int penalty = 0;
    for (int y = 0; y < matrix.height() - 1; ++y) {
        for (int x = 0; x < matrix.width() - 1; ++x) {
            const bool value = matrix.get(x, y);
            if (value == matrix.get(x + 1, y) &&
                value == matrix.get(x, y + 1) &&
                value == matrix.get(x + 1, y + 1)) {
                penalty += 3;
            }
        }
    }
    return penalty;
}

int PenaltyFinderLike(const image_io::BitMatrix& matrix) {
    int penalty = 0;
    static constexpr std::array<int, 11> kPattern = {1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0};
    static constexpr std::array<int, 11> kPatternInverse = {0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1};
    for (int y = 0; y < matrix.height(); ++y) {
        for (int x = 0; x <= matrix.width() - 11; ++x) {
            bool match_a = true;
            bool match_b = true;
            for (int i = 0; i < 11; ++i) {
                const int value = matrix.get(x + i, y) ? 1 : 0;
                match_a &= value == kPattern[static_cast<std::size_t>(i)];
                match_b &= value == kPatternInverse[static_cast<std::size_t>(i)];
            }
            if (match_a || match_b) {
                penalty += 40;
            }
        }
    }
    for (int x = 0; x < matrix.width(); ++x) {
        for (int y = 0; y <= matrix.height() - 11; ++y) {
            bool match_a = true;
            bool match_b = true;
            for (int i = 0; i < 11; ++i) {
                const int value = matrix.get(x, y + i) ? 1 : 0;
                match_a &= value == kPattern[static_cast<std::size_t>(i)];
                match_b &= value == kPatternInverse[static_cast<std::size_t>(i)];
            }
            if (match_a || match_b) {
                penalty += 40;
            }
        }
    }
    return penalty;
}

int PenaltyBalance(const image_io::BitMatrix& matrix) {
    int dark = 0;
    for (int y = 0; y < matrix.height(); ++y) {
        for (int x = 0; x < matrix.width(); ++x) {
            dark += matrix.get(x, y) ? 1 : 0;
        }
    }
    const int total = matrix.width() * matrix.height();
    const int percentage = (dark * 100) / total;
    const int five_percent_steps = std::abs(percentage - 50) / 5;
    return five_percent_steps * 10;
}

int PenaltyScore(const image_io::BitMatrix& matrix) {
    return PenaltyRuns(matrix) + PenaltyBlocks(matrix) + PenaltyFinderLike(matrix) + PenaltyBalance(matrix);
}

QrState BuildState(const std::vector<uint8_t>& interleaved, int mask) {
    QrState state;
    DrawFunctionPatterns(&state);
    DrawVersionInfo(&state);
    DrawCodewords(&state, interleaved);
    ApplyMask(&state, mask);
    DrawFormatInfo(&state, mask);
    return state;
}

uint16_t ReadFormatBits(const image_io::BitMatrix& modules) {
    static constexpr std::array<std::pair<int, int>, 15> kPrimary = {
        std::pair<int, int>{8, 0}, {8, 1}, {8, 2}, {8, 3}, {8, 4},
        {8, 5}, {8, 7}, {8, 8}, {7, 8}, {5, 8},
        {4, 8}, {3, 8}, {2, 8}, {1, 8}, {0, 8},
    };
    uint16_t bits = 0U;
    for (int index = 14; index >= 0; --index) {
        bits <<= 1U;
        bits |= modules.get(kPrimary[static_cast<std::size_t>(index)].first,
                            kPrimary[static_cast<std::size_t>(index)].second)
                    ? 1U
                    : 0U;
    }
    return bits;
}

int HammingDistance(uint16_t left, uint16_t right) {
    int distance = 0;
    uint16_t value = static_cast<uint16_t>(left ^ right);
    while (value != 0U) {
        distance += value & 1U;
        value >>= 1U;
    }
    return distance;
}

std::vector<uint8_t> ExtractRawCodewords(const image_io::BitMatrix& modules, int mask) {
    // 解码时先依据 format info 读出 mask，再按标准 zig-zag 顺序把原始码字流扫出来。
    QrState state;
    state.modules = modules;
    DrawFunctionPatterns(&state);
    DrawVersionInfo(&state);
    DrawFormatInfo(&state, mask);
    ApplyMask(&state, mask);

    std::vector<uint8_t> codewords;
    codewords.reserve(kRawCodewords);
    int bit_index = 0;
    int x = kSymbolSize - 1;
    int y = kSymbolSize - 1;
    int direction = -1;
    while (x > 0) {
        if (x == 6) {
            --x;
        }
        while (y >= 0 && y < kSymbolSize) {
            for (int dx = 0; dx < 2; ++dx) {
                const int column = x - dx;
                if (state.is_function.get(column, y)) {
                    continue;
                }
                if (bit_index % 8 == 0) {
                    codewords.push_back(0U);
                }
                if (state.modules.get(column, y)) {
                    codewords.back() |= static_cast<uint8_t>(1U << (7 - (bit_index % 8)));
                }
                ++bit_index;
            }
            y += direction;
        }
        y -= direction;
        direction = -direction;
        x -= 2;
    }
    return codewords;
}

std::vector<uint8_t> DeinterleaveDataCodewords(const std::vector<uint8_t>& codewords) {
    std::vector<std::vector<uint8_t>> data_blocks;
    data_blocks.reserve(kTotalBlocks);
    for (int index = 0; index < kGroup1Blocks; ++index) {
        data_blocks.emplace_back(static_cast<std::size_t>(kGroup1DataCodewords), 0U);
    }
    for (int index = 0; index < kGroup2Blocks; ++index) {
        data_blocks.emplace_back(static_cast<std::size_t>(kGroup2DataCodewords), 0U);
    }

    std::size_t offset = 0U;
    for (int byte_index = 0; byte_index < kGroup2DataCodewords; ++byte_index) {
        for (auto& block : data_blocks) {
            if (byte_index < static_cast<int>(block.size())) {
                block[static_cast<std::size_t>(byte_index)] = codewords[offset++];
            }
        }
    }
    offset += static_cast<std::size_t>(kTotalBlocks * kEcCodewordsPerBlock);

    std::vector<uint8_t> data_codewords;
    data_codewords.reserve(kDataCodewords);
    for (const auto& block : data_blocks) {
        data_codewords.insert(data_codewords.end(), block.begin(), block.end());
    }
    return data_codewords;
}

bool ParsePayload(const std::vector<uint8_t>& data_codewords,
                  std::vector<uint8_t>* frame_bytes,
                  std::string* error_message) {
    std::vector<uint8_t> bits;
    bits.reserve(data_codewords.size() * 8U);
    for (uint8_t byte : data_codewords) {
        for (int bit = 7; bit >= 0; --bit) {
            bits.push_back(((byte >> bit) & 1U) != 0U ? 1U : 0U);
        }
    }

    auto read_bits = [&](std::size_t offset, int count) -> uint32_t {
        uint32_t value = 0U;
        for (int index = 0; index < count; ++index) {
            value = (value << 1U) | bits[offset + static_cast<std::size_t>(index)];
        }
        return value;
    };

    if (read_bits(0U, 4) != 0b0100U) {
        SetError("Decoded QR mode is not byte mode.", error_message);
        return false;
    }
    const uint32_t length = read_bits(4U, 16);
    if (length > static_cast<uint32_t>(kMaxFrameBytes)) {
        SetError("Decoded QR payload length exceeds Version 29 / Q limit.", error_message);
        return false;
    }
    if (20U + static_cast<std::size_t>(length) * 8U > bits.size()) {
        SetError("Decoded QR payload is truncated.", error_message);
        return false;
    }

    std::vector<uint8_t> decoded(static_cast<std::size_t>(length), 0U);
    for (uint32_t index = 0; index < length; ++index) {
        decoded[static_cast<std::size_t>(index)] = static_cast<uint8_t>(read_bits(20U + static_cast<std::size_t>(index) * 8U, 8));
    }
    if (frame_bytes != nullptr) {
        *frame_bytes = std::move(decoded);
    }
    return true;
}

}  // namespace

bool EncodeBytes(const std::vector<uint8_t>& frame_bytes,
                 image_io::BitMatrix* modules,
                 int* selected_mask,
                 std::string* error_message) {
    if (frame_bytes.size() > static_cast<std::size_t>(kMaxFrameBytes)) {
        SetError("Frame bytes exceed the fixed ISO QR v29/Q capacity.", error_message);
        return false;
    }

    const std::vector<uint8_t> data_codewords = CreateDataCodewords(frame_bytes, error_message);
    if (data_codewords.empty() && !frame_bytes.empty()) {
        return false;
    }
    const std::vector<uint8_t> interleaved = InterleaveCodewords(data_codewords);

    int best_mask = 0;
    int best_score = std::numeric_limits<int>::max();
    QrState best_state;
    for (int mask = 0; mask < 8; ++mask) {
        QrState candidate = BuildState(interleaved, mask);
        const int score = PenaltyScore(candidate.modules);
        if (score < best_score) {
            best_score = score;
            best_mask = mask;
            best_state = candidate;
        }
    }

    if (selected_mask != nullptr) {
        *selected_mask = best_mask;
    }
    if (modules != nullptr) {
        *modules = best_state.modules;
    }
    return true;
}

bool DecodeModules(const image_io::BitMatrix& modules,
                   std::vector<uint8_t>* frame_bytes,
                   int* detected_mask,
                   std::string* error_message) {
    if (modules.width() != kSymbolSize || modules.height() != kSymbolSize) {
        SetError("QR module matrix size is invalid.", error_message);
        return false;
    }

    const uint16_t raw_format = ReadFormatBits(modules);
    int best_mask = -1;
    int best_distance = std::numeric_limits<int>::max();
    for (int mask = 0; mask < 8; ++mask) {
        const uint16_t expected = FormatBits(mask);
        const int distance = HammingDistance(raw_format, expected);
        if (distance < best_distance) {
            best_distance = distance;
            best_mask = mask;
        }
    }
    if (best_mask < 0 || best_distance > 3) {
        SetError("Failed to decode QR format info for Version 29 / Q.", error_message);
        return false;
    }
    if (detected_mask != nullptr) {
        *detected_mask = best_mask;
    }

    const std::vector<uint8_t> raw_codewords = ExtractRawCodewords(modules, best_mask);
    if (static_cast<int>(raw_codewords.size()) < kRawCodewords) {
        SetError("Extracted QR codeword count is too small.", error_message);
        return false;
    }
    const std::vector<uint8_t> data_codewords = DeinterleaveDataCodewords(raw_codewords);
    return ParsePayload(data_codewords, frame_bytes, error_message);
}

ModuleMatrix AddQuietZone(const image_io::BitMatrix& symbol) {
    ModuleMatrix full(static_cast<std::size_t>(kFullModules) * static_cast<std::size_t>(kFullModules), 0U);
    for (int y = 0; y < kSymbolSize; ++y) {
        for (int x = 0; x < kSymbolSize; ++x) {
            full[static_cast<std::size_t>(y + kQuietZoneModules) * static_cast<std::size_t>(kFullModules) +
                 static_cast<std::size_t>(x + kQuietZoneModules)] = symbol.get(x, y) ? 1U : 0U;
        }
    }
    return full;
}

bool EncodeFrameBytes(const std::vector<uint8_t>& frame_bytes,
                      ModuleMatrix* full_modules,
                      int* selected_mask,
                      std::string* error_message) {
    // 对外接口返回包含 quiet zone 的完整模块图，供上层统一渲染成 BMP 或 carrier。
    image_io::BitMatrix symbol;
    if (!EncodeBytes(frame_bytes, &symbol, selected_mask, error_message)) {
        return false;
    }
    if (full_modules != nullptr) {
        *full_modules = AddQuietZone(symbol);
    }
    return true;
}

bool DecodeFrameBytes(const ModuleMatrix& full_modules,
                      std::vector<uint8_t>* frame_bytes,
                      int* detected_mask,
                      std::string* error_message) {
    // 上层传入的是包含 quiet zone 的完整模块图，这里先还原成 133x133 的符号区再解码。
    if (full_modules.size() != static_cast<std::size_t>(kFullModules) * static_cast<std::size_t>(kFullModules)) {
        SetError("QR full module matrix size is invalid.", error_message);
        return false;
    }

    image_io::BitMatrix symbol(kSymbolSize, kSymbolSize, false);
    for (int y = 0; y < kSymbolSize; ++y) {
        for (int x = 0; x < kSymbolSize; ++x) {
            symbol.set(x,
                       y,
                       full_modules[static_cast<std::size_t>(y + kQuietZoneModules) * static_cast<std::size_t>(kFullModules) +
                                    static_cast<std::size_t>(x + kQuietZoneModules)] != 0U);
        }
    }
    return DecodeModules(symbol, frame_bytes, detected_mask, error_message);
}

}  // namespace qr_iso_v29
