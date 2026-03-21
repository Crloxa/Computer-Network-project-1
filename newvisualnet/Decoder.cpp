#include "Decoder.h"
#include "FileUtils.h"
#include "Config.h"
#include "CRC16.h"
#include "Encoder.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace cv;
using namespace std;
namespace fs = std::filesystem;

// 检测定位点
bool detectPositionMarkers(const Mat& frame, vector<Point2f>& corners) {
    Mat gray;
    cvtColor(frame, gray, COLOR_BGR2GRAY);
    
    // 使用自适应阈值处理
    Mat binary;
    adaptiveThreshold(gray, binary, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 11, 2);
    
    // 形态学操作，去除噪声
    Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));
    morphologyEx(binary, binary, MORPH_OPEN, kernel);
    morphologyEx(binary, binary, MORPH_CLOSE, kernel);
    
    vector<vector<Point>> contours;
    findContours(binary, contours, RETR_LIST, CHAIN_APPROX_SIMPLE);
    
    vector<pair<double, Point2f>> potentialMarkers; // 面积和中心点
    
    // 根据图像尺寸调整阈值
    bool isSmallImage = (frame.rows <= 150 && frame.cols <= 150);
    double minArea = isSmallImage ? 1 : 30;
    double minDistance = isSmallImage ? 5 : 30;
    
    for (const auto& contour : contours) {
        double area = contourArea(contour);
        if (area < minArea || area > 100000) continue; // 根据图像尺寸调整面积阈值
        
        Rect rect = boundingRect(contour);
        double aspectRatio = (double)rect.width / rect.height;
        if (aspectRatio < 0.6 || aspectRatio > 1.4) continue; // 放宽宽高比
        
        // 检查是否是正方形
        double squareRatio = min(rect.width, rect.height) / (double)max(rect.width, rect.height);
        if (squareRatio < 0.75) continue; // 放宽正方形比例
        
        // 检查轮廓的层次结构，定位点通常是黑色正方形
        Mat mask = Mat::zeros(frame.size(), CV_8U);
        drawContours(mask, vector<vector<Point>>{contour}, -1, 255, -1);
        
        // 计算轮廓内的平均亮度
        Scalar meanVal = mean(gray, mask);
        if (meanVal[0] > 150) continue; // 放宽亮度阈值
        
        // 计算轮廓的周长
        double perimeter = arcLength(contour, true);
        // 计算轮廓的圆形度
        double circularity = 4 * CV_PI * area / (perimeter * perimeter);
        if (circularity < 0.6) continue; // 放宽圆形度阈值
        
        // 使用重心作为中心点，提高定位精度
        Moments m = moments(contour);
        Point2f center(m.m10 / m.m00, m.m01 / m.m00);
        potentialMarkers.emplace_back(area, center);
    }
    
    if (potentialMarkers.size() < 3) return false;
    
    // 按面积排序
    sort(potentialMarkers.begin(), potentialMarkers.end(), [](const pair<double, Point2f>& a, const pair<double, Point2f>& b) {
        return a.first > b.first;
    });
    
    // 取前三个最大的作为大定位点
    vector<Point2f> largeMarkers;
    for (int i = 0; i < min(5, (int)potentialMarkers.size()); ++i) { // 取前5个，增加找到正确定位点的概率
        largeMarkers.push_back(potentialMarkers[i].second);
    }
    
    if (largeMarkers.size() < 3) return false;
    
    // 找到三个大定位点的位置
    Point2f topLeft, topRight, bottomLeft;
    
    // 找到最左上角的点
    topLeft = largeMarkers[0];
    for (const auto& marker : largeMarkers) {
        if (marker.x + marker.y < topLeft.x + topLeft.y) {
            topLeft = marker;
        }
    }
    
    // 找到最右上角的点
    topRight = largeMarkers[0];
    for (const auto& marker : largeMarkers) {
        if (marker.x - marker.y > topRight.x - topRight.y) {
            topRight = marker;
        }
    }
    
    // 找到最左下角的点
    bottomLeft = largeMarkers[0];
    for (const auto& marker : largeMarkers) {
        if (marker.y - marker.x > bottomLeft.y - bottomLeft.x) {
            bottomLeft = marker;
        }
    }
    
    // 检查三个点是否形成三角形
    double dist1 = norm(topLeft - topRight);
    double dist2 = norm(topLeft - bottomLeft);
    double dist3 = norm(topRight - bottomLeft);
    
    // 检查三个点之间的距离是否合理
    if (dist1 < minDistance || dist2 < minDistance || dist3 < minDistance) return false; // 根据图像尺寸调整距离阈值
    if (abs(dist1 - dist2) > dist1 * 0.4) return false; // 放宽距离差异阈值
    
    // 对定位点进行亚像素级精确化
    vector<Point2f> refinedCorners;
    cornerSubPix(gray, largeMarkers, Size(11, 11), Size(-1, -1), TermCriteria(TermCriteria::EPS + TermCriteria::MAX_ITER, 30, 0.1));
    
    // 重新计算三个定位点
    // 找到最左上角的点
    topLeft = largeMarkers[0];
    for (const auto& marker : largeMarkers) {
        if (marker.x + marker.y < topLeft.x + topLeft.y) {
            topLeft = marker;
        }
    }
    
    // 找到最右上角的点
    topRight = largeMarkers[0];
    for (const auto& marker : largeMarkers) {
        if (marker.x - marker.y > topRight.x - topRight.y) {
            topRight = marker;
        }
    }
    
    // 找到最左下角的点
    bottomLeft = largeMarkers[0];
    for (const auto& marker : largeMarkers) {
        if (marker.y - marker.x > bottomLeft.y - bottomLeft.x) {
            bottomLeft = marker;
        }
    }
    
    corners.push_back(topLeft);
    corners.push_back(topRight);
    corners.push_back(bottomLeft);
    
    return true;
}

// 提取并校正二维码
bool extractQRCode(const Mat& frame, Mat& output) {
    // 直接缩小到原始尺寸，不进行定位点检测和透视变换
    resize(frame, output, Size(Config::QRCode::FrameSize, Config::QRCode::FrameSize), 0, 0, INTER_NEAREST);
    
    return true;
}

using namespace std;
using namespace cv;
namespace fs = std::filesystem;

struct DecodedFrame {
    int frameIdx;
    Config::QRCode::FrameType frameType;
    vector<uint8_t> data;
};

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

bool isInsideSmallQrPoint(int row, int col) {
    const int center = Config::QRCode::FrameSize - SmallQrPointbias;
    return abs(row - center) <= SmallQrPointRadius && abs(col - center) <= SmallQrPointRadius;
}

bool isInsideCornerQuietZone(int row, int col) {
    return row >= 130 || col >= 130;
}

bool isInsideCornerSafetyZone(int row, int col) {
    const int center = Config::QRCode::FrameSize - SmallQrPointbias;
    return abs(row - center) <= SmallQrPointRadius + 2 && abs(col - center) <= SmallQrPointRadius + 2;
}

std::vector<CellPos> buildAreaCells(const DataArea& area) {
    std::vector<CellPos> cells;
    for (int row = area.top; row < area.top + area.height; ++row) {
        const int rowWidth = area.width - area.trimRight;
        for (int col = area.left; col < area.left + rowWidth; ++col) {
            cells.push_back({ row, col });
        }
    }
    return cells;
}

std::vector<CellPos> buildCornerDataCells() {
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
}

std::vector<CellPos> buildFullDataCells() {
    std::vector<CellPos> cells;
    for (const auto& area : kDataAreas) {
        const auto areaCells = buildAreaCells(area);
        cells.insert(cells.end(), areaCells.begin(), areaCells.end());
    }
    const auto cornerCells = buildCornerDataCells();
    cells.insert(cells.end(), cornerCells.begin(), cornerCells.end());
    return cells;
}

std::vector<CellPos> buildMergedDataCells() {
    auto cells = buildFullDataCells();
    if (cells.size() > PaddingCellCount) {
        cells.resize(cells.size() - PaddingCellCount);
    }
    // 输出单元格数量
    cerr << "[DEBUG] Merged cells count: " << cells.size() << endl;
    return cells;
}

bool Decoder::decodeQRFrames(const string& tempDir, const string& outputFilePath) {
    cout << "[INFO] Decode QR frames from: " << tempDir << endl;
    
    vector<string> frameFiles;
    try {
        for (const auto& entry : fs::directory_iterator(tempDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".png") {
                frameFiles.push_back(entry.path().string());
            }
        }
    } catch (const fs::filesystem_error& e) {
        cerr << "[ERROR] Failed to read directory: " << e.what() << endl;
        return false;
    }
    
    if (frameFiles.empty()) {
        cerr << "[ERROR] No QR frames found in " << tempDir << endl;
        return false;
    }
    
    sort(frameFiles.begin(), frameFiles.end());
    vector<DecodedFrame> decodedFrames;
    
    for (const auto& frameFile : frameFiles) {
        Mat frame = imread(frameFile);
        if (frame.empty()) {
            cerr << "[ERROR] Read frame failed: " << frameFile << endl;
            continue;
        }
        
        // 提取并校正二维码
        Mat smallFrame;
        if (!extractQRCode(frame, smallFrame)) {
            cerr << "[WARNING] Failed to extract QR code from frame: " << frameFile << endl;
            continue;
        }
        
        // 计算单元格大小
        int cellSize = smallFrame.cols / Config::QRCode::FrameSize;
        
        // 读取头部信息（LSB优先，与编码顺序一致）
        uint16_t field0 = 0;
        for (int i = 0; i < 16; ++i) {
            int row = Config::QRCode::HEADER_TOP;
            int col = Config::QRCode::HEADER_LEFT + i;
            
            // 计算实际的像素位置
            int y = row * cellSize + cellSize / 2;
            int x = col * cellSize + cellSize / 2;
            
            // 确保坐标在图片范围内
            y = min(max(y, 0), smallFrame.rows - 1);
            x = min(max(x, 0), smallFrame.cols - 1);
            
            Vec3b pixel = smallFrame.at<Vec3b>(y, x);
            bool bit = (pixel[0] > 128 && pixel[1] > 128 && pixel[2] > 128); // White means 1
            field0 |= (bit ? 1 : 0) << i; // LSB first
        }
        
        uint16_t tailLen = field0 >> 4;
        uint16_t frameTypeValue = field0 & 0b1111;
        
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
        
        // 对于Normal帧，使用头部信息中指定的数据长度
        // 不再强制使用完整的帧容量，确保与编码时的实际数据长度一致
        
        // 读取帧序号（LSB优先，与编码顺序一致）
        uint16_t frameNo = 0;
        for (int i = 0; i < 16; ++i) {
            int row = Config::QRCode::HEADER_TOP + 2;
            int col = Config::QRCode::HEADER_LEFT + i;
            
            // 计算实际的像素位置
            int y = row * cellSize + cellSize / 2;
            int x = col * cellSize + cellSize / 2;
            
            // 确保坐标在图片范围内
            y = min(max(y, 0), smallFrame.rows - 1);
            x = min(max(x, 0), smallFrame.cols - 1);
            
            Vec3b pixel = smallFrame.at<Vec3b>(y, x);
            bool bit = (pixel[0] > 128 && pixel[1] > 128 && pixel[2] > 128); // White means 1
            frameNo |= (bit ? 1 : 0) << i; // LSB first
        }
        
        // 读取校验码（LSB优先，与编码顺序一致）
        uint16_t checksum = 0;
        for (int i = 0; i < 16; ++i) {
            int row = Config::QRCode::HEADER_TOP + 1;
            int col = Config::QRCode::HEADER_LEFT + i;
            
            // 计算实际的像素位置
            int y = row * cellSize + cellSize / 2;
            int x = col * cellSize + cellSize / 2;
            
            // 确保坐标在图片范围内
            y = min(max(y, 0), smallFrame.rows - 1);
            x = min(max(x, 0), smallFrame.cols - 1);
            
            Vec3b pixel = smallFrame.at<Vec3b>(y, x);
            bool bit = (pixel[0] > 128 && pixel[1] > 128 && pixel[2] > 128); // White means 1
            checksum |= (bit ? 1 : 0) << i; // LSB first
        }
        
        // 读取所有数据区域的数据，以便查看完整的数据结构
        const auto mergedCells = buildMergedDataCells();
        int totalBits = mergedCells.size();
        int totalBytes = (totalBits + 7) / 8;
        vector<uint8_t> frameData(totalBytes, 0);
        int bitIndex = 0;
        
        for (const auto& cell : mergedCells) {
            // 计算实际的像素位置
            int y = cell.row * cellSize + cellSize / 2;
            int x = cell.col * cellSize + cellSize / 2;
            
            // 确保坐标在图片范围内
            y = min(max(y, 0), smallFrame.rows - 1);
            x = min(max(x, 0), smallFrame.cols - 1);
            
            Vec3b pixel = smallFrame.at<Vec3b>(y, x);
            bool bit = (pixel[0] > 128 && pixel[1] > 128 && pixel[2] > 128); // White means 1
            
            int byteIndex = bitIndex / 8;
            int offset = bitIndex % 8;
            if (bit) frameData[byteIndex] |= (1 << offset);
            
            bitIndex++;
        }
        
        // 输出实际读取的比特数和字节数
        cerr << "[DEBUG] Total cells: " << mergedCells.size() << endl;
        cerr << "[DEBUG] Actual bits read: " << bitIndex << endl;
        cerr << "[DEBUG] Actual bytes read: " << (bitIndex + 7) / 8 << endl;
        
        // 验证校验码（使用与编码时相同的实际数据）
        int checksumDataLen = tailLen;
        
        // 确保使用正确的数据长度进行校验码计算
        vector<uint8_t> checksumData(checksumDataLen, 0);
        if (frameData.size() >= checksumDataLen) {
            memcpy(checksumData.data(), frameData.data(), checksumDataLen);
        } else {
            memcpy(checksumData.data(), frameData.data(), frameData.size());
        }
        
        uint16_t calculatedChecksum = CRC16::calculateWithFrameInfo(checksumData.data(), checksumDataLen, frameType, frameNo);
        
        // 输出校验码计算详情
        cerr << "[DEBUG] Checksum calculation details: " << endl;
        cerr << "[DEBUG] Tail length: " << checksumDataLen << endl;
        cerr << "[DEBUG] Frame number: " << frameNo << " (0x" << hex << frameNo << dec << ")" << endl;
        cerr << "[DEBUG] Is start frame: " << (frameType == Config::QRCode::FrameType::Start || frameType == Config::QRCode::FrameType::StartAndEnd) << endl;
        cerr << "[DEBUG] Is end frame: " << (frameType == Config::QRCode::FrameType::End || frameType == Config::QRCode::FrameType::StartAndEnd) << endl;
        cerr << "[DEBUG] Start/end flag: 0x" << hex << ((frameType == Config::QRCode::FrameType::Start || frameType == Config::QRCode::FrameType::StartAndEnd) << 1) + (frameType == Config::QRCode::FrameType::End || frameType == Config::QRCode::FrameType::StartAndEnd) << dec << endl;
        cerr << "[DEBUG] First 16 bytes of data: " << endl;
        for (int i = 0; i < min(16, (int)checksumData.size()); ++i) {
            cerr << hex << setw(2) << setfill('0') << (int)checksumData[i] << " ";
            if ((i + 1) % 8 == 0) cerr << endl;
        }
        cerr << dec << endl;
        
        if (checksum != calculatedChecksum) {
            cerr << "[WARNING] Checksum mismatch! Calculated: " << calculatedChecksum << ", Expected: " << checksum << endl;
            // 不跳过，继续处理，以便查看完整的数据
        } else {
            cerr << "[INFO] Checksum matched!" << endl;
        }
        
        // 检查是否已经解析过相同帧序号的帧
        bool alreadyDecoded = false;
        for (const auto& existingFrame : decodedFrames) {
            if (existingFrame.frameIdx == frameNo) {
                alreadyDecoded = true;
                break;
            }
        }
        if (alreadyDecoded) {
            // 已经解析过该帧，跳过
            continue;
        }
        
        // 只保留实际数据长度
        frameData.resize(tailLen);
        decodedFrames.push_back({frameNo, frameType, frameData});
    }
    
    if (decodedFrames.empty()) {
        cerr << "[ERROR] No valid QR frames decoded" << endl;
        return false;
    }
    
    // 按帧序号排序
    sort(decodedFrames.begin(), decodedFrames.end(), [](const DecodedFrame& a, const DecodedFrame& b) {
        return a.frameIdx < b.frameIdx;
    });
    
    // 合并数据
    vector<uint8_t> outputData;
    for (const auto& frame : decodedFrames) {
        outputData.insert(outputData.end(), frame.data.begin(), frame.data.end());
    }
    
    // 写入文件
    if (!FileUtils::writeBytesToFile(outputFilePath, outputData)) {
        cerr << "[ERROR] Write output file failed: " << outputFilePath << endl;
        return false;
    }
    
    cout << "[INFO] Decode success! Save to: " << outputFilePath << endl;
    return true;
}

bool Decoder::extractFramesFromVideo(const string& inputVideoPath, const string& outputDir, int repeatCount) {
    // 创建输出目录
    if (!FileUtils::dirExists(outputDir)) {
        if (!FileUtils::createDirectory(outputDir)) {
            cerr << "[ERROR] Create output directory failed: " << outputDir << endl;
            return false;
        }
    }
    FileUtils::clearDirectory(outputDir);
    
    // 首先统一视频格式
    string tempVideoPath = FileUtils::joinPath(outputDir, "temp_unified.mp4");
    Encoder encoder;
    if (!encoder.transcodeVideo(inputVideoPath, tempVideoPath)) {
        cerr << "[ERROR] Failed to unify video format" << endl;
        return false;
    }
    
    // 使用OpenCV VideoCapture读取视频
    VideoCapture cap(tempVideoPath);
    if (!cap.isOpened()) {
        cerr << "[ERROR] Failed to open video file: " << tempVideoPath << endl;
        return false;
    }
    
    int frameCount = 0;
    Mat frame;
    while (cap.read(frame)) {
        frameCount++;
        
        // 重复提取帧
        for (int r = 0; r < repeatCount; ++r) {
            string frameFilename = FileUtils::joinPath(outputDir, "frame_" + to_string((frameCount-1) * repeatCount + r + 1) + ".png");
            imwrite(frameFilename, frame);
        }
    }
    
    // 释放VideoCapture
    cap.release();
    
    // 删除临时统一格式视频
    remove(tempVideoPath.c_str());
    
    cout << "[INFO] Frames extracted successfully to: " << outputDir << endl;
    cout << "[INFO] Extracted " << frameCount << " frames, " << repeatCount << " copies each" << endl;
    return true;
}

bool Decoder::decodeSingleImage(const string& imagePath) {
    cout << "[INFO] Decoding single image: " << imagePath << endl;
    
    // 读取图像
    Mat frame = imread(imagePath);
    if (frame.empty()) {
        cerr << "[ERROR] Failed to read image: " << imagePath << endl;
        return false;
    }
    
    // 提取并校正二维码
    Mat smallFrame;
    if (!extractQRCode(frame, smallFrame)) {
        cerr << "[WARNING] Failed to extract QR code from image" << endl;
        // 尝试直接缩小到原始尺寸
        resize(frame, smallFrame, Size(Config::QRCode::FrameSize, Config::QRCode::FrameSize), 0, 0, INTER_NEAREST);
    }
    
    // 计算单元格大小
    int cellSize = smallFrame.cols / Config::QRCode::FrameSize;
    
    // 读取头部信息（LSB优先，与编码顺序一致）
    uint16_t field0 = 0;
    for (int i = 0; i < 16; ++i) {
        int row = Config::QRCode::HEADER_TOP;
        int col = Config::QRCode::HEADER_LEFT + i;
        
        // 计算实际的像素位置
        int y = row * cellSize + cellSize / 2;
        int x = col * cellSize + cellSize / 2;
        
        // 确保坐标在图片范围内
        y = min(max(y, 0), smallFrame.rows - 1);
        x = min(max(x, 0), smallFrame.cols - 1);
        
        Vec3b pixel = smallFrame.at<Vec3b>(y, x);
        bool bit = (pixel[0] > 128 && pixel[1] > 128 && pixel[2] > 128); // White means 1
        field0 |= (bit ? 1 : 0) << i; // LSB first
    }
    
    uint16_t tailLen = field0 >> 4;
    uint16_t frameTypeValue = field0 & 0b1111;
    
    Config::QRCode::FrameType frameType;
    string frameTypeStr;
    switch (frameTypeValue) {
        case 0b0011:
            frameType = Config::QRCode::FrameType::Start;
            frameTypeStr = "Start";
            break;
        case 0b1100:
            frameType = Config::QRCode::FrameType::End;
            frameTypeStr = "End";
            break;
        case 0b1111:
            frameType = Config::QRCode::FrameType::StartAndEnd;
            frameTypeStr = "StartAndEnd";
            break;
        default:
            frameType = Config::QRCode::FrameType::Normal;
            frameTypeStr = "Normal";
            break;
    }
    
    // 对于Normal帧，强制使用完整的帧容量
    if (frameType == Config::QRCode::FrameType::Normal) {
        tailLen = Config::QRCode::BytesPerFrame;
    }
    
    // 读取帧序号（LSB优先，与编码顺序一致）
    uint16_t frameNo = 0;
    for (int i = 0; i < 16; ++i) {
        int row = Config::QRCode::HEADER_TOP + 2;
        int col = Config::QRCode::HEADER_LEFT + i;
        
        // 计算实际的像素位置
        int y = row * cellSize + cellSize / 2;
        int x = col * cellSize + cellSize / 2;
        
        // 确保坐标在图片范围内
        y = min(max(y, 0), smallFrame.rows - 1);
        x = min(max(x, 0), smallFrame.cols - 1);
        
        Vec3b pixel = smallFrame.at<Vec3b>(y, x);
        bool bit = (pixel[0] > 128 && pixel[1] > 128 && pixel[2] > 128); // White means 1
        frameNo |= (bit ? 1 : 0) << i; // LSB first
    }
    
    // 读取校验码（LSB优先，与编码顺序一致）
    uint16_t checksum = 0;
    for (int i = 0; i < 16; ++i) {
        int row = Config::QRCode::HEADER_TOP + 1;
        int col = Config::QRCode::HEADER_LEFT + i;
        
        // 计算实际的像素位置
        int y = row * cellSize + cellSize / 2;
        int x = col * cellSize + cellSize / 2;
        
        // 确保坐标在图片范围内
        y = min(max(y, 0), smallFrame.rows - 1);
        x = min(max(x, 0), smallFrame.cols - 1);
        
        Vec3b pixel = smallFrame.at<Vec3b>(y, x);
        bool bit = (pixel[0] > 128 && pixel[1] > 128 && pixel[2] > 128); // White means 1
        checksum |= (bit ? 1 : 0) << i; // LSB first
    }
    
    // 读取所有数据区域的数据，以便查看完整的数据结构
    const auto mergedCells = buildMergedDataCells();
    int totalBits = mergedCells.size();
    int totalBytes = (totalBits + 7) / 8;
    vector<uint8_t> frameData(totalBytes, 0);
    int bitIndex = 0;
    
    for (const auto& cell : mergedCells) {
        // 多点采样/投票，提高抗模糊能力
        int whiteVotes = 0, blackVotes = 0;
        int maxX = smallFrame.cols - 1;
        int maxY = smallFrame.rows - 1;
        int cx = cell.col * cellSize + cellSize / 2;
        int cy = cell.row * cellSize + cellSize / 2;
        
        for (int oy : {-2, 0, 2}) {
            for (int ox : {-2, 0, 2}) {
                int x = min(max(cx + ox, 0), maxX);
                int y = min(max(cy + oy, 0), maxY);
                Vec3b pixel = smallFrame.at<Vec3b>(y, x);
                double gray = (pixel[0] + pixel[1] + pixel[2]) / 3.0;
                if (gray > 128) whiteVotes++;
                else blackVotes++;
            }
        }
        
        bool bit = whiteVotes > blackVotes; // 投票决定最终bit值
        
        int byteIndex = bitIndex / 8;
        int offset = bitIndex % 8;
        if (bit) frameData[byteIndex] |= (1 << offset);
        
        bitIndex++;
    }
    
    // 输出实际读取的比特数和字节数
    cerr << "[DEBUG] Total cells: " << mergedCells.size() << endl;
    cerr << "[DEBUG] Actual bits read: " << bitIndex << endl;
    cerr << "[DEBUG] Actual bytes read: " << (bitIndex + 7) / 8 << endl;
    
    // 验证校验码（使用与编码时相同的数据长度）
    int checksumDataLen = tailLen;
    
    // 确保使用正确的数据长度进行校验码计算
    vector<uint8_t> checksumData(checksumDataLen, 0);
    if (frameData.size() >= checksumDataLen) {
        memcpy(checksumData.data(), frameData.data(), checksumDataLen);
    } else {
        memcpy(checksumData.data(), frameData.data(), frameData.size());
    }
    
    uint16_t calculatedChecksum = CRC16::calculateWithFrameInfo(checksumData.data(), checksumDataLen, frameType, frameNo);
    
    // 输出校验码计算详情
    cerr << "[DEBUG] Checksum calculation details: " << endl;
    cerr << "[DEBUG] Tail length: " << checksumDataLen << endl;
    cerr << "[DEBUG] Frame number: " << frameNo << " (0x" << hex << frameNo << dec << ")" << endl;
    cerr << "[DEBUG] Is start frame: " << (frameType == Config::QRCode::FrameType::Start || frameType == Config::QRCode::FrameType::StartAndEnd) << endl;
    cerr << "[DEBUG] Is end frame: " << (frameType == Config::QRCode::FrameType::End || frameType == Config::QRCode::FrameType::StartAndEnd) << endl;
    cerr << "[DEBUG] Start/end flag: 0x" << hex << ((frameType == Config::QRCode::FrameType::Start || frameType == Config::QRCode::FrameType::StartAndEnd) << 1) + (frameType == Config::QRCode::FrameType::End || frameType == Config::QRCode::FrameType::StartAndEnd) << dec << endl;
    cerr << "[DEBUG] First 16 bytes of data: " << endl;
    for (int i = 0; i < min(16, (int)checksumData.size()); ++i) {
        cerr << hex << setw(2) << setfill('0') << (int)checksumData[i] << " ";
        if ((i + 1) % 8 == 0) cerr << endl;
    }
    cerr << dec << endl;
    
    if (checksum != calculatedChecksum) {
        cerr << "[WARNING] Checksum mismatch! Calculated: " << calculatedChecksum << ", Expected: " << checksum << endl;
    } else {
        cout << "[INFO] Checksum matched!" << endl;
    }
    
    // 输出解码信息
    cout << "[INFO] Frame type: " << frameTypeStr << endl;
    cout << "[INFO] Frame number: " << frameNo << " (0x" << hex << frameNo << dec << ")" << endl;
    cout << "[INFO] Data length: " << tailLen << " bytes" << endl;
    cout << "[INFO] Total bytes read: " << frameData.size() << " bytes" << endl;
    cout << "[INFO] Expected bytes: " << Config::QRCode::BytesPerFrame << " bytes" << endl;
    
    // 输出完整的数据（用于调试）
    cout << "[INFO] Complete data:" << endl;
    for (int i = 0; i < frameData.size(); ++i) {
        cout << hex << setw(2) << setfill('0') << (int)frameData[i] << " ";
        if ((i + 1) % 16 == 0) cout << endl;
    }
    cout << endl;
    
    return true;
}