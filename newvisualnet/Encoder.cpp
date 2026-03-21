#include "Encoder.h"
#include "QRPosition.h"
#include "FileUtils.h"
#include "Config.h"
#include "FFmpegWrapper.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <string>

using namespace std;
using namespace cv;

bool Encoder::transcodeVideo(const string& inputPath, const string& outputPath, int crf) {
    FFmpegWrapper ffmpeg;
    return ffmpeg.transcodeVideo(inputPath, outputPath, crf);
}

bool Encoder::encodeQRFrames(const string& inputFilePath) {
    vector<uint8_t> fileData;
    if (!FileUtils::readFileToBytes(inputFilePath, fileData)) {
        cerr << "[ERROR] Read file failed: " << inputFilePath << endl;
        return false;
    }
    cout << "[INFO] Read file success: " << inputFilePath << " (" << fileData.size() << " bytes)" << endl;

    // 常量调用100%统一
    const int FRAME_CAPACITY = Config::QRCode::BytesPerFrame;
    int totalFrames = (fileData.size() + FRAME_CAPACITY - 1) / FRAME_CAPACITY;
    cout << "[INFO] Total QR frames: " << totalFrames << endl;

    string tempDir = Config::QRCode::TEMP_DIR;
    if (!FileUtils::dirExists(tempDir)) {
        if (!FileUtils::createDirectory(tempDir)) {
            cerr << "[ERROR] Create dir failed: " << tempDir << endl;
            return false;
        }
    }
    FileUtils::clearDirectory(tempDir);

    QRPosition qrPos;
    for (int frameIdx = 0; frameIdx < totalFrames; frameIdx++) {
        int startIdx = frameIdx * FRAME_CAPACITY;
        int endIdx = min(startIdx + FRAME_CAPACITY, (int)fileData.size());
        vector<uint8_t> frameData(fileData.begin() + startIdx, fileData.begin() + endIdx);

        Mat qrFrame;
        if (!qrPos.encodeFrame(frameIdx, totalFrames, frameData.data(), frameData.size(), qrFrame)) {
            cerr << "[ERROR] Encode frame failed: " << frameIdx << endl;
            return false;
        }

        string frameFileName = "qr_frame_" + to_string(frameIdx) + ".png";
        string framePath = FileUtils::joinPath(tempDir, frameFileName);
        if (!imwrite(framePath, qrFrame)) {
            cerr << "[ERROR] Save frame failed: " << framePath << endl;
            return false;
        }
    }

    cout << "[INFO] Encode success! Save to: " << tempDir << endl;
    return true;
}

bool Encoder::createVideoFromQRFrames(const string& outputVideoPath) {
    string tempDir = Config::QRCode::TEMP_DIR;
    if (!FileUtils::dirExists(tempDir)) {
        cerr << "[ERROR] Temp directory not found: " << tempDir << endl;
        return false;
    }

    // 构建FFmpeg命令来合成视频
    string inputPattern = FileUtils::joinPath(tempDir, "qr_frame_%d.png");
    
    // 使用ffmpeg将图片序列合成为视频
    // 注意：这里需要使用ffmpeg的图片序列输入格式
    
    // 构建命令行
    string command = "ffmpeg.exe -framerate 15 -i " + inputPattern + " -c:v libx264 -pix_fmt yuv420p " + outputVideoPath;
    cout << "[INFO] Executing command: " << command << endl;
    
    // 执行命令
    int result = system(command.c_str());
    if (result != 0) {
        cerr << "[ERROR] Failed to create video from QR frames" << endl;
        return false;
    }
    
    cout << "[INFO] Video created successfully: " << outputVideoPath << endl;
    return true;
}