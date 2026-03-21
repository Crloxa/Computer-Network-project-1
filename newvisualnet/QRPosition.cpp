#include "QRPosition.h"
#include "CRC16.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <random>

using namespace cv;
using namespace std;

QRPosition::QRPosition() {
}

bool QRPosition::isInsideSmallQrPoint(int row, int col) {
    const int center = Config::QRCode::FrameSize - 7; // SmallQrPointbias = 7
    return abs(row - center) <= 3 && abs(col - center) <= 3; // SmallQrPointRadius = 3
}

bool QRPosition::isInsideCornerQuietZone(int row, int col) {
    return row >= 130 || col >= 130;
}

bool QRPosition::isInsideCornerSafetyZone(int row, int col) {
    const int center = Config::QRCode::FrameSize - 7; // SmallQrPointbias = 7
    return abs(row - center) <= 5 && abs(col - center) <= 5; // radius + 2
}

vector<QRPosition::CellPos> QRPosition::buildAreaCells(const DataArea& area) {
    vector<CellPos> cells;
    for (int row = area.top; row < area.top + area.height; ++row) {
        const int rowWidth = area.width - area.trimRight;
        for (int col = area.left; col < area.left + rowWidth; ++col) {
            cells.push_back({ row, col });
        }
    }
    return cells;
}

vector<QRPosition::CellPos> QRPosition::buildCornerDataCells() {
    vector<CellPos> cells;
    for (int row = Config::QRCode::FrameSize - 21; row < Config::QRCode::FrameSize; ++row) {
        for (int col = Config::QRCode::FrameSize - 21; col < Config::QRCode::FrameSize; ++col) {
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

vector<QRPosition::CellPos> QRPosition::buildFullDataCells() {
    vector<CellPos> cells;
    for (const auto& area : kDataAreas) {
        const auto areaCells = buildAreaCells(area);
        cells.insert(cells.end(), areaCells.begin(), areaCells.end());
    }
    const auto cornerCells = buildCornerDataCells();
    cells.insert(cells.end(), cornerCells.begin(), cornerCells.end());
    return cells;
}

vector<QRPosition::CellPos> QRPosition::buildMergedDataCells() {
    auto cells = buildFullDataCells();
    if (cells.size() > 4) { // PaddingCellCount = 4
        cells.resize(cells.size() - 4);
    }
    return cells;
}

void QRPosition::drawSafeArea(Mat& mat) {
    rectangle(mat, Rect(0, 0, mat.cols, Config::QRCode::SAFE_AREA_WIDTH), Scalar(255, 255, 255), FILLED);
    rectangle(mat, Rect(0, 0, Config::QRCode::SAFE_AREA_WIDTH, mat.rows), Scalar(255, 255, 255), FILLED);
    rectangle(mat, Rect(0, mat.rows - Config::QRCode::SAFE_AREA_WIDTH, mat.cols, Config::QRCode::SAFE_AREA_WIDTH), Scalar(255, 255, 255), FILLED);
    rectangle(mat, Rect(mat.cols - Config::QRCode::SAFE_AREA_WIDTH, 0, Config::QRCode::SAFE_AREA_WIDTH, mat.rows), Scalar(255, 255, 255), FILLED);
}

void QRPosition::drawPositionMarkers(Mat& mat) {
    auto drawBigMarker = [&](int x, int y) {
        rectangle(mat, Rect(x, y, Config::QRCode::POS_MARKER_SIZE, Config::QRCode::POS_MARKER_SIZE), Scalar(0, 0, 0), FILLED);
        rectangle(mat, Rect(x + 3, y + 3, Config::QRCode::POS_MARKER_SIZE - 6, Config::QRCode::POS_MARKER_SIZE - 6), Scalar(255, 255, 255), FILLED);
        rectangle(mat, Rect(x + 6, y + 6, Config::QRCode::POS_MARKER_SIZE - 12, Config::QRCode::POS_MARKER_SIZE - 12), Scalar(0, 0, 0), FILLED);
        };

    auto drawSmallMarker = [&](int x, int y) {
        rectangle(mat, Rect(x, y, Config::QRCode::POS_SMALL_MARKER_SIZE, Config::QRCode::POS_SMALL_MARKER_SIZE), Scalar(0, 0, 0), FILLED);
        rectangle(mat, Rect(x + 2, y + 2, Config::QRCode::POS_SMALL_MARKER_SIZE - 4, Config::QRCode::POS_SMALL_MARKER_SIZE - 4), Scalar(255, 255, 255), FILLED);
        };

    drawBigMarker(Config::QRCode::SAFE_AREA_WIDTH, Config::QRCode::SAFE_AREA_WIDTH);
    drawBigMarker(mat.cols - Config::QRCode::POS_MARKER_SIZE - Config::QRCode::SAFE_AREA_WIDTH, Config::QRCode::SAFE_AREA_WIDTH);
    drawBigMarker(Config::QRCode::SAFE_AREA_WIDTH, mat.rows - Config::QRCode::POS_MARKER_SIZE - Config::QRCode::SAFE_AREA_WIDTH);
    drawSmallMarker(mat.cols - Config::QRCode::POS_SMALL_MARKER_SIZE - Config::QRCode::SAFE_AREA_WIDTH, mat.rows - Config::QRCode::POS_SMALL_MARKER_SIZE - Config::QRCode::SAFE_AREA_WIDTH);
}

void QRPosition::drawHeaderArea(Mat& mat) {
    rectangle(mat, Rect(Config::QRCode::HEADER_LEFT, Config::QRCode::HEADER_TOP, Config::QRCode::HEADER_WIDTH, Config::QRCode::HEADER_HEIGHT), Scalar(0, 0, 255), FILLED);
}

void QRPosition::drawDataAreas(Mat& mat) {
    // 绘制数据区域（仅调试用）
    for (const auto& area : kDataAreas) {
        rectangle(mat, Rect(area.left, area.top, area.width, area.height), Scalar(0, 255, 0), 1);
    }
}

void QRPosition::writeHeaderField(Mat& mat, int fieldId, uint16_t value, int tailLen, Config::QRCode::FrameType frameType) {
    int row = Config::QRCode::HEADER_TOP + fieldId;
    for (int i = 0; i < Config::QRCode::HEADER_WIDTH; i++) {
        uchar bit = (value >> i) & 1; // LSB first, matching reference code
        mat.at<Vec3b>(row, Config::QRCode::HEADER_LEFT + i) = bit ? pixel[White] : pixel[Black]; // White means 1, matching reference code
    }
}

void QRPosition::writeBytesToCells(Mat& mat, const uint8_t* data, int len) {
    auto cells = buildMergedDataCells();
    int bitIndex = 0;
    
    for (const auto& cell : cells) {
        const int byteIndex = bitIndex / 8;
        const int offset = bitIndex % 8;
        bool bit = false;
        
        if (byteIndex < len) {
            bit = (data[byteIndex] >> offset) & 1; // LSB first
        } else {
            bit = 0; // 填充部分为0
        }
        
        mat.at<Vec3b>(cell.row, cell.col) = bit ? pixel[White] : pixel[Black]; // White means 1, matching reference code
        bitIndex++;
    }
}

bool QRPosition::encodeFrame(
    int frameIdx,
    int totalFrames,
    const uint8_t* frameData,
    int dataLen,
    cv::Mat& outFrame
) {
    if (!frameData || dataLen <= 0) return false;

    // 常量调用100%统一
    outFrame = Mat(Config::QRCode::FrameSize, Config::QRCode::FrameSize, CV_8UC3, Scalar(255, 255, 255));

    drawSafeArea(outFrame);
    drawPositionMarkers(outFrame);

    Config::QRCode::FrameType frameType = (frameIdx == 0 && frameIdx == totalFrames - 1) ? Config::QRCode::FrameType::StartAndEnd
        : (frameIdx == 0) ? Config::QRCode::FrameType::Start
        : (frameIdx == totalFrames - 1) ? Config::QRCode::FrameType::End
        : Config::QRCode::FrameType::Normal;

    // Field 0: 帧类型 + tailLen
    uint16_t tailLen = dataLen;
    // 对于Normal帧，使用完整的帧容量
    if (frameType == Config::QRCode::FrameType::Normal) {
        tailLen = Config::QRCode::BytesPerFrame;
    }
    
    // 准备填充后的数据
    uint8_t paddedData[Config::QRCode::BytesPerFrame] = {0};
    memcpy(paddedData, frameData, dataLen);
    // 对不足的部分进行填充（使用固定值0，便于调试）
    for (int i = dataLen; i < Config::QRCode::BytesPerFrame; ++i) {
        paddedData[i] = 0; // 全部填充为0
    }

    uint16_t frameTypeValue = 0;
    switch (frameType) {
        case Config::QRCode::FrameType::Start:
            frameTypeValue = 0b0011;
            break;
        case Config::QRCode::FrameType::End:
            frameTypeValue = 0b1100;
            break;
        case Config::QRCode::FrameType::StartAndEnd:
            frameTypeValue = 0b1111;
            break;
        case Config::QRCode::FrameType::Normal:
            frameTypeValue = 0b0000;
            break;
    }
    // 确保tailLen不超过12位（因为headerValue只有12位用于tailLen）
    tailLen = tailLen > 0xFFF ? 0xFFF : tailLen; // 12位最大值
    uint16_t headerValue = (tailLen << 4) | frameTypeValue;
    writeHeaderField(outFrame, 0, headerValue, tailLen, frameType);

    // Field 2: 帧序号
    uint16_t frameNo = frameIdx % 65536;
    writeHeaderField(outFrame, 2, frameNo, tailLen, frameType);

    // 写入数据（使用填充后的数据）
    writeBytesToCells(outFrame, paddedData, Config::QRCode::BytesPerFrame);

    // Field 1: 校验码 - 使用与头部相同的16位帧序号和实际数据
    uint16_t checksum = CRC16::calculateWithFrameInfo(frameData, dataLen, frameType, frameNo);
    writeHeaderField(outFrame, 1, checksum, tailLen, frameType);

    // 放大到输出尺寸
    resize(outFrame, outFrame, Size(Config::QRCode::OutputFrame_W, Config::QRCode::OutputFrame_H), 0, 0, INTER_NEAREST);

    return true;
}

bool QRPosition::encodeFrameOriginalSize(
    int frameIdx,
    int totalFrames,
    const uint8_t* frameData,
    int dataLen,
    cv::Mat& outFrame
) {
    if (!frameData || dataLen <= 0) return false;

    // 常量调用100%统一
    outFrame = Mat(Config::QRCode::FrameSize, Config::QRCode::FrameSize, CV_8UC3, Scalar(255, 255, 255));

    drawSafeArea(outFrame);
    drawPositionMarkers(outFrame);

    Config::QRCode::FrameType frameType = (frameIdx == 0 && frameIdx == totalFrames - 1) ? Config::QRCode::FrameType::StartAndEnd
        : (frameIdx == 0) ? Config::QRCode::FrameType::Start
        : (frameIdx == totalFrames - 1) ? Config::QRCode::FrameType::End
        : Config::QRCode::FrameType::Normal;

    // Field 0: 帧类型 + tailLen
    uint16_t tailLen = dataLen;
    // 对于Normal帧，使用完整的帧容量
    if (frameType == Config::QRCode::FrameType::Normal) {
        tailLen = Config::QRCode::BytesPerFrame;
    }
    
    // 准备填充后的数据
    uint8_t paddedData[Config::QRCode::BytesPerFrame] = {0};
    memcpy(paddedData, frameData, dataLen);
    // 对不足的部分进行填充（使用固定值0，便于调试）
    for (int i = dataLen; i < Config::QRCode::BytesPerFrame; ++i) {
        paddedData[i] = 0; // 全部填充为0
    }

    uint16_t frameTypeValue = 0;
    switch (frameType) {
        case Config::QRCode::FrameType::Start:
            frameTypeValue = 0b0011;
            break;
        case Config::QRCode::FrameType::End:
            frameTypeValue = 0b1100;
            break;
        case Config::QRCode::FrameType::StartAndEnd:
            frameTypeValue = 0b1111;
            break;
        case Config::QRCode::FrameType::Normal:
            frameTypeValue = 0b0000;
            break;
    }
    // 确保tailLen不超过12位（因为headerValue只有12位用于tailLen）
    tailLen = tailLen > 0xFFF ? 0xFFF : tailLen; // 12位最大值
    uint16_t headerValue = (tailLen << 4) | frameTypeValue;
    writeHeaderField(outFrame, 0, headerValue, tailLen, frameType);

    // Field 2: 帧序号
    uint16_t frameNo = frameIdx % 65536;
    writeHeaderField(outFrame, 2, frameNo, tailLen, frameType);

    // 写入数据（使用填充后的数据）
    writeBytesToCells(outFrame, paddedData, Config::QRCode::BytesPerFrame);

    // Field 1: 校验码 - 使用与头部相同的16位帧序号和实际数据
    uint16_t checksum = CRC16::calculateWithFrameInfo(frameData, dataLen, frameType, frameNo);
    writeHeaderField(outFrame, 1, checksum, tailLen, frameType);

    // 不放大图像，保持原始尺寸

    return true;
}

int QRPosition::getMergedDataCellsSize() {
    auto cells = buildMergedDataCells();
    return cells.size();
}

std::vector<QRPosition::CellPos> QRPosition::getMergedDataCells() {
    return buildMergedDataCells();
}