#pragma once

#include "protocol_v1.h"
#include "protocol_qr.h"

#include <filesystem>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

namespace demo_encoder {

struct EncodedFrame {
    protocol_v1::FrameHeader header;
    std::vector<uint8_t> payload;
    cv::Mat logical_frame;
    cv::Mat physical_frame;
};

std::vector<uint8_t> ReadFileBytes(const std::filesystem::path& input_path);
std::vector<EncodedFrame> EncodeBytesToFrames(const std::vector<uint8_t>& bytes,
                                              const protocol_v1::EncoderOptions& options);
cv::Mat RenderLogicalFrame(const protocol_v1::FrameHeader& header, const std::vector<uint8_t>& payload);
cv::Mat RenderPhysicalFrame(const cv::Mat& logical_frame, int module_pixels);
cv::Mat RenderLayoutGuide(int module_pixels);
bool WriteProtocolSamples(const std::filesystem::path& output_dir,
                          const protocol_v1::EncoderOptions& options,
                          std::string* error_message = nullptr);
bool WriteDemoPackage(const std::filesystem::path& input_path,
                      const std::filesystem::path& output_dir,
                      const protocol_v1::EncoderOptions& options,
                      std::string* error_message = nullptr);

bool WriteStandardQrSamples(const std::filesystem::path& output_dir,
                            const protocol_qr::EncoderOptions& options,
                            std::string* error_message = nullptr);

bool WriteStandardQrPackage(const std::filesystem::path& input_path,
                            const std::filesystem::path& output_dir,
                            const protocol_qr::EncoderOptions& options,
                            std::string* error_message = nullptr);

}  // namespace demo_encoder
