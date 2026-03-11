#include "simple_image.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>

namespace image_io {

namespace {

std::size_t PixelIndex(int width, int x, int y) {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
}

void WriteUint16LittleEndian(std::ostream& stream, uint16_t value) {
    stream.put(static_cast<char>(value & 0xFFU));
    stream.put(static_cast<char>((value >> 8U) & 0xFFU));
}

void WriteUint32LittleEndian(std::ostream& stream, uint32_t value) {
    stream.put(static_cast<char>(value & 0xFFU));
    stream.put(static_cast<char>((value >> 8U) & 0xFFU));
    stream.put(static_cast<char>((value >> 16U) & 0xFFU));
    stream.put(static_cast<char>((value >> 24U) & 0xFFU));
}

uint16_t ReadUint16LittleEndian(std::istream& stream) {
    const uint8_t b0 = static_cast<uint8_t>(stream.get());
    const uint8_t b1 = static_cast<uint8_t>(stream.get());
    return static_cast<uint16_t>(b0 | (static_cast<uint16_t>(b1) << 8U));
}

uint32_t ReadUint32LittleEndian(std::istream& stream) {
    const uint8_t b0 = static_cast<uint8_t>(stream.get());
    const uint8_t b1 = static_cast<uint8_t>(stream.get());
    const uint8_t b2 = static_cast<uint8_t>(stream.get());
    const uint8_t b3 = static_cast<uint8_t>(stream.get());
    return static_cast<uint32_t>(b0) |
           (static_cast<uint32_t>(b1) << 8U) |
           (static_cast<uint32_t>(b2) << 16U) |
           (static_cast<uint32_t>(b3) << 24U);
}

}  // namespace

BitMatrix::BitMatrix(int width, int height, bool value)
    : width_(width),
      height_(height),
      bits_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), value ? 1U : 0U) {}

int BitMatrix::width() const {
    return width_;
}

int BitMatrix::height() const {
    return height_;
}

bool BitMatrix::get(int x, int y) const {
    return bits_[PixelIndex(width_, x, y)] != 0U;
}

void BitMatrix::set(int x, int y, bool value) {
    bits_[PixelIndex(width_, x, y)] = value ? 1U : 0U;
}

GrayImage::GrayImage(int width, int height, uint8_t value)
    : width_(width),
      height_(height),
      pixels_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), value) {}

int GrayImage::width() const {
    return width_;
}

int GrayImage::height() const {
    return height_;
}

uint8_t GrayImage::get(int x, int y) const {
    return pixels_[PixelIndex(width_, x, y)];
}

void GrayImage::set(int x, int y, uint8_t value) {
    pixels_[PixelIndex(width_, x, y)] = value;
}

RgbImage::RgbImage(int width, int height, Rgb fill)
    : width_(width),
      height_(height),
      pixels_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), fill) {}

int RgbImage::width() const {
    return width_;
}

int RgbImage::height() const {
    return height_;
}

const Rgb& RgbImage::get(int x, int y) const {
    return pixels_[PixelIndex(width_, x, y)];
}

void RgbImage::set(int x, int y, const Rgb& value) {
    pixels_[PixelIndex(width_, x, y)] = value;
}

RgbImage ResizeNearest(const RgbImage& source, int width, int height) {
    RgbImage output(width, height, kWhite);
    for (int y = 0; y < height; ++y) {
        const int source_y = std::clamp((y * source.height()) / std::max(1, height), 0, source.height() - 1);
        for (int x = 0; x < width; ++x) {
            const int source_x = std::clamp((x * source.width()) / std::max(1, width), 0, source.width() - 1);
            output.set(x, y, source.get(source_x, source_y));
        }
    }
    return output;
}

RgbImage Crop(const RgbImage& source, int x, int y, int width, int height) {
    RgbImage output(width, height, kWhite);
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            const int source_x = x + col;
            const int source_y = y + row;
            if (source_x >= 0 && source_x < source.width() &&
                source_y >= 0 && source_y < source.height()) {
                output.set(col, row, source.get(source_x, source_y));
            }
        }
    }
    return output;
}

GrayImage ToGray(const RgbImage& source) {
    GrayImage output(source.width(), source.height(), 255U);
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            const Rgb& pixel = source.get(x, y);
            const int gray = (static_cast<int>(pixel.r) * 77 +
                              static_cast<int>(pixel.g) * 150 +
                              static_cast<int>(pixel.b) * 29) >> 8;
            output.set(x, y, static_cast<uint8_t>(gray));
        }
    }
    return output;
}

uint8_t ComputeOtsuThreshold(const GrayImage& image) {
    std::array<uint32_t, 256> histogram{};
    const int width = image.width();
    const int height = image.height();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            ++histogram[image.get(x, y)];
        }
    }

    const uint32_t total = static_cast<uint32_t>(width * height);
    uint64_t sum_all = 0;
    for (int index = 0; index < 256; ++index) {
        sum_all += static_cast<uint64_t>(index) * histogram[index];
    }

    uint64_t sum_background = 0;
    uint32_t background_weight = 0;
    double best_score = -1.0;
    uint8_t best_threshold = 127;

    for (int threshold = 0; threshold < 256; ++threshold) {
        background_weight += histogram[threshold];
        if (background_weight == 0) {
            continue;
        }
        const uint32_t foreground_weight = total - background_weight;
        if (foreground_weight == 0) {
            break;
        }

        sum_background += static_cast<uint64_t>(threshold) * histogram[threshold];
        const double mean_background = static_cast<double>(sum_background) / static_cast<double>(background_weight);
        const double mean_foreground =
            static_cast<double>(sum_all - sum_background) / static_cast<double>(foreground_weight);
        const double diff = mean_background - mean_foreground;
        const double score =
            static_cast<double>(background_weight) * static_cast<double>(foreground_weight) * diff * diff;
        if (score > best_score) {
            best_score = score;
            best_threshold = static_cast<uint8_t>(threshold);
        }
    }

    return best_threshold;
}

BitMatrix Threshold(const GrayImage& image, uint8_t threshold) {
    BitMatrix output(image.width(), image.height(), false);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            output.set(x, y, image.get(x, y) <= threshold);
        }
    }
    return output;
}

void FillRect(RgbImage* image, int x, int y, int width, int height, Rgb color) {
    for (int row = 0; row < height; ++row) {
        const int py = y + row;
        if (py < 0 || py >= image->height()) {
            continue;
        }
        for (int col = 0; col < width; ++col) {
            const int px = x + col;
            if (px < 0 || px >= image->width()) {
                continue;
            }
            image->set(px, py, color);
        }
    }
}

void Blit(const RgbImage& source, RgbImage* dest, int x, int y) {
    for (int row = 0; row < source.height(); ++row) {
        for (int col = 0; col < source.width(); ++col) {
            const int px = x + col;
            const int py = y + row;
            if (px < 0 || px >= dest->width() || py < 0 || py >= dest->height()) {
                continue;
            }
            dest->set(px, py, source.get(col, row));
        }
    }
}

bool WriteBmp(const std::filesystem::path& path, const RgbImage& image, std::string* error_message) {
    std::ofstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        if (error_message != nullptr) {
            *error_message = "Failed to open BMP for writing: " + path.string();
        }
        return false;
    }

    const uint32_t row_stride = static_cast<uint32_t>((image.width() * 3 + 3) & ~3);
    const uint32_t pixel_array_size = row_stride * static_cast<uint32_t>(image.height());
    const uint32_t file_size = 14U + 40U + pixel_array_size;

    stream.put('B');
    stream.put('M');
    WriteUint32LittleEndian(stream, file_size);
    WriteUint16LittleEndian(stream, 0U);
    WriteUint16LittleEndian(stream, 0U);
    WriteUint32LittleEndian(stream, 54U);

    WriteUint32LittleEndian(stream, 40U);
    WriteUint32LittleEndian(stream, static_cast<uint32_t>(image.width()));
    WriteUint32LittleEndian(stream, static_cast<uint32_t>(image.height()));
    WriteUint16LittleEndian(stream, 1U);
    WriteUint16LittleEndian(stream, 24U);
    WriteUint32LittleEndian(stream, 0U);
    WriteUint32LittleEndian(stream, pixel_array_size);
    WriteUint32LittleEndian(stream, 2835U);
    WriteUint32LittleEndian(stream, 2835U);
    WriteUint32LittleEndian(stream, 0U);
    WriteUint32LittleEndian(stream, 0U);

    const uint32_t padding = row_stride - static_cast<uint32_t>(image.width() * 3);
    for (int y = image.height() - 1; y >= 0; --y) {
        for (int x = 0; x < image.width(); ++x) {
            const Rgb& pixel = image.get(x, y);
            stream.put(static_cast<char>(pixel.b));
            stream.put(static_cast<char>(pixel.g));
            stream.put(static_cast<char>(pixel.r));
        }
        for (uint32_t index = 0; index < padding; ++index) {
            stream.put('\0');
        }
    }
    return stream.good();
}

bool ReadBmp(const std::filesystem::path& path, RgbImage* image, std::string* error_message) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        if (error_message != nullptr) {
            *error_message = "Failed to open BMP for reading: " + path.string();
        }
        return false;
    }

    const char b0 = static_cast<char>(stream.get());
    const char b1 = static_cast<char>(stream.get());
    if (b0 != 'B' || b1 != 'M') {
        if (error_message != nullptr) {
            *error_message = "Unsupported BMP header in: " + path.string();
        }
        return false;
    }

    static_cast<void>(ReadUint32LittleEndian(stream));
    static_cast<void>(ReadUint16LittleEndian(stream));
    static_cast<void>(ReadUint16LittleEndian(stream));
    const uint32_t pixel_offset = ReadUint32LittleEndian(stream);
    const uint32_t dib_size = ReadUint32LittleEndian(stream);
    if (dib_size != 40U) {
        if (error_message != nullptr) {
            *error_message = "Unsupported BMP DIB size in: " + path.string();
        }
        return false;
    }

    const int32_t width = static_cast<int32_t>(ReadUint32LittleEndian(stream));
    const int32_t height = static_cast<int32_t>(ReadUint32LittleEndian(stream));
    const uint16_t planes = ReadUint16LittleEndian(stream);
    const uint16_t bits_per_pixel = ReadUint16LittleEndian(stream);
    const uint32_t compression = ReadUint32LittleEndian(stream);
    static_cast<void>(ReadUint32LittleEndian(stream));
    static_cast<void>(ReadUint32LittleEndian(stream));
    static_cast<void>(ReadUint32LittleEndian(stream));
    static_cast<void>(ReadUint32LittleEndian(stream));
    static_cast<void>(ReadUint32LittleEndian(stream));

    if (planes != 1U || (bits_per_pixel != 24U && bits_per_pixel != 32U) || compression != 0U || width <= 0 || height == 0) {
        if (error_message != nullptr) {
            *error_message = "Only uncompressed 24-bit or 32-bit BMP is supported: " + path.string();
        }
        return false;
    }

    // BMP 允许使用负高度表示“自顶向下”存储，这里统一转成正常的行坐标。
    const bool top_down = height < 0;
    const int image_height = std::abs(height);
    stream.seekg(static_cast<std::streamoff>(pixel_offset), std::ios::beg);
    RgbImage output(width, image_height, kWhite);
    const int bytes_per_pixel = bits_per_pixel / 8;
    const int row_stride = (width * bytes_per_pixel + 3) & ~3;
    std::vector<uint8_t> row(static_cast<std::size_t>(row_stride), 0U);
    for (int row_index = 0; row_index < image_height; ++row_index) {
        stream.read(reinterpret_cast<char*>(row.data()), static_cast<std::streamsize>(row.size()));
        if (!stream) {
            if (error_message != nullptr) {
                *error_message = "Unexpected EOF while reading BMP pixels: " + path.string();
            }
            return false;
        }
        const int y = top_down ? row_index : (image_height - 1 - row_index);
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>(x * bytes_per_pixel);
            output.set(x, y, {row[offset + 2U], row[offset + 1U], row[offset]});
        }
    }

    *image = std::move(output);
    return true;
}

}  // namespace image_io
