#include "FFmpegWrapper.h"
#include "FileUtils.h"
#include <iostream>
#include <sstream>
#include <cstdlib>

FFmpegWrapper::FFmpegWrapper() : m_lastError("") {}

std::vector<std::string> FFmpegWrapper::buildTranscodeCommand(
    const std::string& inputPath,
    const std::string& outputPath,
    int crf,
    const std::string& videoCodec,
    const std::string& audioCodec,
    int fps
) {
    std::vector<std::string> cmd;
    cmd.push_back(m_ffmpegPath);
    cmd.push_back("-i");
    cmd.push_back(inputPath);
    cmd.push_back("-c:v");
    cmd.push_back(videoCodec);
    cmd.push_back("-crf");
    cmd.push_back(std::to_string(crf));
    cmd.push_back("-r");
    cmd.push_back(std::to_string(fps));
    cmd.push_back("-c:a");
    cmd.push_back(audioCodec);
    cmd.push_back("-y");
    cmd.push_back(outputPath);
    return cmd;
}

bool FFmpegWrapper::executeCommand(const std::vector<std::string>& cmd) {
    std::ostringstream oss;
    for (size_t i = 0; i < cmd.size(); ++i) {
        if (i > 0) oss << " ";
        oss << cmd[i];
    }
    std::string cmdStr = oss.str();
    int ret = std::system(cmdStr.c_str());
    if (ret != 0) {
        m_lastError = "Command failed: " + cmdStr + " (return code: " + std::to_string(ret) + ")";
        return false;
    }
    return true;
}

bool FFmpegWrapper::transcodeVideo(
    const std::string& inputPath,
    const std::string& outputPath,
    int crf,
    const std::string& videoCodec,
    const std::string& audioCodec,
    int fps
) {
    if (!FileUtils::fileExists(inputPath)) {
        m_lastError = "Input file not exists: " + inputPath;
        return false;
    }
    std::string outputDir;
    size_t sepPos = outputPath.find_last_of("/\\");
    if (sepPos != std::string::npos) {
        outputDir = outputPath.substr(0, sepPos);
        if (!FileUtils::dirExists(outputDir)) FileUtils::createDirectory(outputDir);
    }
    std::vector<std::string> cmd = buildTranscodeCommand(inputPath, outputPath, crf, videoCodec, audioCodec, fps);
    return executeCommand(cmd);
}