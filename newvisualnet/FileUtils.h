#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace FileUtils {
    bool readFileToBytes(const std::string& filePath, std::vector<uint8_t>& outData);
    bool writeBytesToFile(const std::string& filePath, const std::vector<uint8_t>& data);
    bool fileExists(const std::string& filePath);
    bool dirExists(const std::string& dirPath);
    bool createDirectory(const std::string& dirPath);
    bool clearDirectory(const std::string& dirPath);
    std::string joinPath(const std::string& parent, const std::string& child);
}