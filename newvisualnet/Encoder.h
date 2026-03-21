#pragma once
#include <string>

class Encoder {
public:
    bool encodeQRFrames(const std::string& inputFilePath);
    bool transcodeVideo(const std::string& inputPath, const std::string& outputPath, int crf = 23);
    bool createVideoFromQRFrames(const std::string& outputVideoPath);
};