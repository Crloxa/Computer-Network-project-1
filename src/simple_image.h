#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace image_io {

struct Rgb {
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
};

class BitMatrix {
public:
    BitMatrix() = default;
    BitMatrix(int width, int height, bool value);

    int width() const;
    int height() const;

    bool get(int x, int y) const;
    void set(int x, int y, bool value);

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<uint8_t> bits_;
};

class GrayImage {
public:
    GrayImage() = default;
    GrayImage(int width, int height, uint8_t value);

    int width() const;
    int height() const;

    uint8_t get(int x, int y) const;
    void set(int x, int y, uint8_t value);

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<uint8_t> pixels_;
};

class RgbImage {
public:
    RgbImage() = default;
    RgbImage(int width, int height, Rgb fill);

    int width() const;
    int height() const;

    const Rgb& get(int x, int y) const;
    void set(int x, int y, const Rgb& value);

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<Rgb> pixels_;
};

constexpr Rgb kBlack = {0, 0, 0};
constexpr Rgb kWhite = {255, 255, 255};
constexpr Rgb kBlue = {40, 120, 220};

RgbImage ResizeNearest(const RgbImage& source, int width, int height);
RgbImage Crop(const RgbImage& source, int x, int y, int width, int height);
GrayImage ToGray(const RgbImage& source);
uint8_t ComputeOtsuThreshold(const GrayImage& image);
BitMatrix Threshold(const GrayImage& image, uint8_t threshold);

void FillRect(RgbImage* image, int x, int y, int width, int height, Rgb color);
void Blit(const RgbImage& source, RgbImage* dest, int x, int y);

bool WriteBmp(const std::filesystem::path& path, const RgbImage& image, std::string* error_message);
bool ReadBmp(const std::filesystem::path& path, RgbImage* image, std::string* error_message);

}  // namespace image_io
