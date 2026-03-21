#pragma once
#include <string>
#include <vector>

class FFmpegWrapper {
public:
    FFmpegWrapper();
    bool transcodeVideo(
        const std::string& inputPath,
        const std::string& outputPath,
        int crf = 23,
        const std::string& videoCodec = "libx264",
        const std::string& audioCodec = "aac",
        int fps = 15
    );
    std::string getLastError() const { return m_lastError; }

private:
    std::vector<std::string> buildTranscodeCommand(
        const std::string& inputPath,
        const std::string& outputPath,
        int crf,
        const std::string& videoCodec,
        const std::string& audioCodec,
        int fps
    );
    bool executeCommand(const std::vector<std::string>& cmd);

    std::string m_lastError;
    std::string m_ffmpegPath = "ffmpeg.exe";
};