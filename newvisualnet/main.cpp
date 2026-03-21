#include "Config.h"
#include "Encoder.h"
#include "Decoder.h"
#include "FileUtils.h"
#include "QRPosition.h"
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <array>
#include <opencv2/opencv.hpp>

// 绘制QR码布局
bool drawQRLayout(const std::string& savePath);

// 测试QR码编码和解码
void testQRCode();

// 测试从文件中读取二维码并解码
void testQRCodeFromFile();

// 测试数据单元格比较
void testQRCodeCellComparison();

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

static std::vector<std::string> listPngFilesInDir(const std::string& dirPath) {
    std::vector<std::string> pngFiles;

#ifdef _WIN32
    std::string searchPath = FileUtils::joinPath(dirPath, "*");
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                pngFiles.push_back(FileUtils::joinPath(dirPath, findData.cFileName));
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
#else
    DIR* dir = opendir(dirPath.c_str());
    if (dir != nullptr) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            std::string fileName = entry->d_name;
            if (fileName.size() >= 4 && fileName.substr(fileName.size() - 4) == ".png") {
                pngFiles.push_back(FileUtils::joinPath(dirPath, fileName));
            }
        }
        closedir(dir);
    }
#endif

    return pngFiles;
}

void printUsage() {
    std::cout << "========== 程序使用说明 ==========" << std::endl;
    std::cout << "功能列表：" << std::endl;
    std::cout << "  1. 视频转换:        encoder video <输入视频> <输出视频> [crf=" << Config::Video::DEFAULT_CRF << "]" << std::endl;
    std::cout << "  2. QR编码:          encoder qr-encode <输入文件>" << std::endl;
    std::cout << "  3. QR解码(预解码):    encoder qr-decode <输入目录> <输出文件>" << std::endl;
    std::cout << "  4. 绘制QR布局:      encoder draw-layout <输出图片路径>" << std::endl;
    std::cout << "  5. QR转视频:          encoder qr-to-video <输出视频>" << std::endl;
    std::cout << "  6. 视频抽帧:        encoder video-to-frames <输入视频> <输出目录> [repeat=1]" << std::endl;
    std::cout << "  7. 单图解码:        encoder decode-single-image <图片路径>" << std::endl;
    std::cout << "  8. QR测试:          encoder test-qr" << std::endl;
    std::cout << "  9. QR文件测试:       encoder test-qr-file" << std::endl;
    std::cout << " 10. QR文件读写测试:    encoder test-qr-from-file" << std::endl;
    std::cout << " 11. QR单元格比较测试:   encoder test-qr-cells" << std::endl;
    std::cout << "示例：" << std::endl;
    std::cout << "  encoder video input/test.mp4 output/test_out.mp4 20" << std::endl;
    std::cout << "  encoder qr-encode input/test.txt" << std::endl;
    std::cout << "  encoder draw-layout output/qr_layout.png" << std::endl;
    std::cout << "  encoder qr-to-video output/qr_video.mp4" << std::endl;
    std::cout << "  encoder video-to-frames input/qr_video.mp4 output/temp 3" << std::endl;
    std::cout << "  encoder decode-single-image output/yuan.png" << std::endl;
}

static bool runVideoTranscode(const std::vector<std::string>& args) {
    if (args.size() < 4) {
        std::cerr << "[ERROR] 视频转换参数不足！" << std::endl;
        std::cerr << "Usage: encoder video <input_video> <output_video> [crf=" << Config::Video::DEFAULT_CRF << "]" << std::endl;
        return false;
    }
    std::string inputVideo = args[2];
    std::string outputVideo = args[3];
    int crf = Config::Video::DEFAULT_CRF;
    if (args.size() >= 5) crf = std::stoi(args[4]);

    Encoder encoder;
    return encoder.transcodeVideo(inputVideo, outputVideo, crf);
}

static bool runQREncode(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "[ERROR] QR编码参数不足！" << std::endl;
        std::cerr << "Usage: encoder qr-encode <input_file>" << std::endl;
        return false;
    }
    Encoder encoder;
    return encoder.encodeQRFrames(args[2]);
}

static bool runQRDecode(const std::vector<std::string>& args) {
    if (args.size() < 4) {
        std::cerr << "[ERROR] QR解码参数不足！" << std::endl;
        std::cerr << "Usage: encoder qr-decode <input_dir> <output_file>" << std::endl;
        return false;
    }
    std::vector<std::string> framePaths = listPngFilesInDir(args[2]);
    if (framePaths.empty()) {
        std::cerr << "[ERROR] 未找到QR帧文件: " << args[2] << std::endl;
        return false;
    }
    std::sort(framePaths.begin(), framePaths.end());
    Decoder decoder;
    return decoder.decodeQRFrames(args[2], args[3]);
}

static bool runDrawLayout(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "[ERROR] 绘制布局参数不足！" << std::endl;
        std::cerr << "Usage: encoder draw-layout <output_png>" << std::endl;
        return false;
    }
    return drawQRLayout(args[2]);
}

static bool runQRToVideo(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "[ERROR] QR转视频参数不足！" << std::endl;
        std::cerr << "Usage: encoder qr-to-video <output_video>" << std::endl;
        return false;
    }
    Encoder encoder;
    return encoder.createVideoFromQRFrames(args[2]);
}

static bool runVideoToFrames(const std::vector<std::string>& args) {
    if (args.size() < 4) {
        std::cerr << "[ERROR] 视频抽帧参数不足！" << std::endl;
        std::cerr << "Usage: encoder video-to-frames <input_video> <output_dir> [repeat=1]" << std::endl;
        return false;
    }
    std::string inputVideo = args[2];
    std::string outputDir = args[3];
    int repeatCount = 1;
    if (args.size() >= 5) repeatCount = std::stoi(args[4]);

    Decoder decoder;
    return decoder.extractFramesFromVideo(inputVideo, outputDir, repeatCount);
}

static bool runDecodeSingleImage(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "[ERROR] 单图解码参数不足！" << std::endl;
        std::cerr << "Usage: encoder decode-single-image <image_path>" << std::endl;
        return false;
    }
    
    std::string imagePath = args[2];
    Decoder decoder;
    return decoder.decodeSingleImage(imagePath);
}

static bool runTestQR(const std::vector<std::string>& args) {
    // 初始化随机数种子
    srand(time(nullptr));
    
    // 创建随机测试数据，避免规律性
    std::vector<uint8_t> testData(Config::QRCode::BytesPerFrame);
    // 生成随机数据
    for (int i = 0; i < testData.size(); ++i) {
        testData[i] = rand() % 256; // 0-255的随机值
    }
    
    // 输出原始数据
    std::cout << "[INFO] Original test data: " << std::endl;
    for (int i = 0; i < 16; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)testData[i] << " ";
        if ((i + 1) % 8 == 0) std::cout << std::endl;
    }
    std::cout << std::dec << std::endl;
    
    // 生成二维码
    QRPosition qrPosition;
    cv::Mat qrFrame;
    bool success = qrPosition.encodeFrame(0, 1, testData.data(), testData.size(), qrFrame);
    
    if (success) {
        // 保存二维码图片
        cv::imwrite("test_qr.png", qrFrame);
        std::cout << "[INFO] QR code generated and saved as test_qr.png" << std::endl;
        
        // 解码二维码
        cv::Mat decodedFrame;
        bool extracted = extractQRCode(qrFrame, decodedFrame);
        
        if (extracted) {
            // 读取头部信息
            uint16_t headerValue = 0;
            for (int i = 0; i < 16; ++i) {
                int row = Config::QRCode::HEADER_TOP + 0;
                int col = Config::QRCode::HEADER_LEFT + i;
                cv::Vec3b pixel = decodedFrame.at<cv::Vec3b>(row, col);
                bool bit = (pixel[0] > 128 && pixel[1] > 128 && pixel[2] > 128); // White means 1
                headerValue |= (bit ? 1 : 0) << i; // LSB first
            }
            
            uint16_t frameTypeValue = headerValue & 0x0F;
            uint16_t tailLen = (headerValue >> 4) & 0xFFF;
            
            Config::QRCode::FrameType frameType;
            switch (frameTypeValue) {
            case 0b0011:
                frameType = Config::QRCode::FrameType::Start;
                break;
            case 0b1100:
                frameType = Config::QRCode::FrameType::End;
                break;
            case 0b1111:
                frameType = Config::QRCode::FrameType::StartAndEnd;
                break;
            default:
                frameType = Config::QRCode::FrameType::Normal;
                break;
            }
            
            // 读取帧序号
            uint16_t frameNo = 0;
            for (int i = 0; i < 16; ++i) {
                int row = Config::QRCode::HEADER_TOP + 2;
                int col = Config::QRCode::HEADER_LEFT + i;
                cv::Vec3b pixel = decodedFrame.at<cv::Vec3b>(row, col);
                bool bit = (pixel[0] > 128 && pixel[1] > 128 && pixel[2] > 128); // White means 1
                frameNo |= (bit ? 1 : 0) << i; // LSB first
            }
            
            // 读取校验码
            uint16_t checksum = 0;
            for (int i = 0; i < 16; ++i) {
                int row = Config::QRCode::HEADER_TOP + 1;
                int col = Config::QRCode::HEADER_LEFT + i;
                cv::Vec3b pixel = decodedFrame.at<cv::Vec3b>(row, col);
                bool bit = (pixel[0] > 128 && pixel[1] > 128 && pixel[2] > 128); // White means 1
                checksum |= (bit ? 1 : 0) << i; // LSB first
            }
            
            // 读取数据
            struct DataArea {
                int top;
                int left;
                int height;
                int width;
                int trimRight;
            };
            
            struct CellPos {
                int row;
                int col;
            };
            
            const int DataAreaCount = 5;
            const int PaddingCellCount = 4;
            const int CornerReserveSize = 21;
            const int SmallQrPointbias = 7;
            const int SmallQrPointRadius = 3;
            
            const std::array<DataArea, DataAreaCount> kDataAreas = {
                {{3, 37, 3, 75, 0},
                 {6, 21, 15, 91, 0},
                 {21, 3, 88, 127, 0},
                 {109, 3, 3, 127, 0},
                 {112, 21, 18, 91, 0}}
            };
            
            auto isInsideSmallQrPoint = [&](int row, int col) {
                const int center = Config::QRCode::FrameSize - SmallQrPointbias;
                return abs(row - center) <= SmallQrPointRadius && abs(col - center) <= SmallQrPointRadius;
            };
            
            auto isInsideCornerQuietZone = [&](int row, int col) {
                return row >= 130 || col >= 130;
            };
            
            auto isInsideCornerSafetyZone = [&](int row, int col) {
                const int center = Config::QRCode::FrameSize - SmallQrPointbias;
                return abs(row - center) <= SmallQrPointRadius + 2 && abs(col - center) <= SmallQrPointRadius + 2;
            };
            
            auto buildAreaCells = [&](const DataArea & area) {
                std::vector<CellPos> cells;
                for (int row = area.top; row < area.top + area.height; ++row) {
                    const int rowWidth = area.width - area.trimRight;
                    for (int col = area.left; col < area.left + rowWidth; ++col) {
                        cells.push_back({ row, col });
                    }
                }
                return cells;
            };
            
            auto buildCornerDataCells = [&]() {
                std::vector<CellPos> cells;
                for (int row = Config::QRCode::FrameSize - CornerReserveSize; row < Config::QRCode::FrameSize; ++row) {
                    for (int col = Config::QRCode::FrameSize - CornerReserveSize; col < Config::QRCode::FrameSize; ++col) {
                        if (isInsideCornerQuietZone(row, col)) {
                            continue;
                        }
                        if (isInsideCornerSafetyZone(row, col)) {
                            continue;
                        }
                        cells.push_back({ row, col });
                    }
                }
                return cells;
            };
            
            auto buildFullDataCells = [&]() {
                std::vector<CellPos> cells;
                for (const auto& area : kDataAreas) {
                    const auto areaCells = buildAreaCells(area);
                    cells.insert(cells.end(), areaCells.begin(), areaCells.end());
                }
                const auto cornerCells = buildCornerDataCells();
                cells.insert(cells.end(), cornerCells.begin(), cornerCells.end());
                return cells;
            };
            
            auto buildMergedDataCells = [&]() {
                auto cells = buildFullDataCells();
                if (cells.size() > PaddingCellCount) {
                    cells.resize(cells.size() - PaddingCellCount);
                }
                return cells;
            };
            
            const auto mergedCells = buildMergedDataCells();
            int totalBits = mergedCells.size();
            int totalBytes = (totalBits + 7) / 8;
            std::vector<uint8_t> frameData(totalBytes, 0);
            int bitIndex = 0;
            
            for (const auto& cell : mergedCells) {
                cv::Vec3b pixel = decodedFrame.at<cv::Vec3b>(cell.row, cell.col);
                bool bit = (pixel[0] > 128 && pixel[1] > 128 && pixel[2] > 128); // White means 1
                
                int byteIndex = bitIndex / 8;
                int offset = bitIndex % 8;
                if (bit) frameData[byteIndex] |= (1 << offset);
                
                bitIndex++;
            }
            
            // 输出解码后的数据
            std::cout << "[INFO] Decoded data: " << std::endl;
            for (int i = 0; i < 16; i++) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)frameData[i] << " ";
                if ((i + 1) % 8 == 0) std::cout << std::endl;
            }
            std::cout << std::dec << std::endl;
            
            // 验证数据是否一致
            bool dataMatch = true;
            for (int i = 0; i < Config::QRCode::BytesPerFrame; i++) {
                if (testData[i] != frameData[i]) {
                    dataMatch = false;
                    std::cout << "[ERROR] Data mismatch at index " << i << ": expected " << std::hex << (int)testData[i] << ", got " << (int)frameData[i] << std::dec << std::endl;
                    break;
                }
            }
            
            if (dataMatch) {
                std::cout << "[SUCCESS] Data matched!" << std::endl;
            } else {
                std::cout << "[ERROR] Data mismatch!" << std::endl;
            }
        } else {
            std::cout << "[ERROR] Failed to extract QR code" << std::endl;
        }
    } else {
        std::cout << "[ERROR] Failed to generate QR code" << std::endl;
    }
    
    return true;
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv, argv + argc);
    if (args.size() < 2) {
        std::cerr << "[ERROR] �������㣡" << std::endl;
        printUsage();
        return 1;
    }

    std::string command = args[1];
    if (command == "video") {
        if (!runVideoTranscode(args)) return 1;
    }
    else if (command == "qr-encode") {
        if (!runQREncode(args)) return 1;
    }
    else if (command == "qr-decode") {
        if (!runQRDecode(args)) return 1;
    }
    else if (command == "draw-layout") {
        if (!runDrawLayout(args)) return 1;
    }
    else if (command == "qr-to-video") {
        if (!runQRToVideo(args)) return 1;
    }
    else if (command == "video-to-frames") {
        if (!runVideoToFrames(args)) return 1;
    }
    else if (command == "decode-single-image") {
        if (!runDecodeSingleImage(args)) return 1;
    }
    else if (command == "test-qr") {
        if (!runTestQR(args)) return 1;
    }
    else if (command == "test-qr-file") {
        testQRCodeFromFile();
    }
    else if (command == "test-qr-from-file") {
        testQRCodeFromFile();
    }
    else if (command == "test-qr-cells") {
        testQRCodeCellComparison();
    }
    else {
        std::cerr << "[ERROR] δ֪����: " << command << std::endl;
        printUsage();
        return 1;
    }

    std::cout << "[SUCCESS] ִ����ɣ�" << std::endl;
    return 0;
}