#pragma once
#include <string>
#include <opencv2/opencv.hpp>

class Decoder {
public:
    bool decodeQRFrames(const std::string& tempDir, const std::string& outputFilePath);
    bool extractFramesFromVideo(const std::string& inputVideoPath, const std::string& outputDir, int repeatCount = 1);
    bool decodeSingleImage(const std::string& imagePath);
};

// 提取并校正二维码
bool extractQRCode(const cv::Mat& frame, cv::Mat& output);