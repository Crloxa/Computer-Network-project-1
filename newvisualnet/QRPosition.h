#pragma once
#include "Config.h"
#include <opencv2/opencv.hpp>
#include <array>
#include <cstdint>
#include <vector>

class QRPosition {
private:
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

    enum Color {
        Black = 0,
        White = 7
    };

    const std::array<DataArea, 5> kDataAreas = {
        {{3, 37, 3, 75, 0},
         {6, 21, 15, 91, 0},
         {21, 3, 88, 127, 0},
         {109, 3, 3, 127, 0},
         {112, 21, 18, 91, 0}}
    };

    const cv::Vec3b pixel[8] = {
        cv::Vec3b(0, 0, 0), cv::Vec3b(0, 0, 255), cv::Vec3b(0, 255, 0), cv::Vec3b(0, 255, 255),
        cv::Vec3b(255, 0, 0), cv::Vec3b(255, 0, 255), cv::Vec3b(255, 255, 0), cv::Vec3b(255, 255, 255)
    };

    bool isInsideSmallQrPoint(int row, int col);
    bool isInsideCornerQuietZone(int row, int col);
    bool isInsideCornerSafetyZone(int row, int col);
    std::vector<CellPos> buildAreaCells(const DataArea& area);
    std::vector<CellPos> buildCornerDataCells();
    std::vector<CellPos> buildFullDataCells();
    std::vector<CellPos> buildMergedDataCells();

public:
    QRPosition();

    bool encodeFrame(
        int frameIdx,
        int totalFrames,
        const uint8_t* frameData,
        int dataLen,
        cv::Mat& outFrame
    );
    
    // 编码为原始尺寸的二维码图像（用于测试）
    bool encodeFrameOriginalSize(
        int frameIdx,
        int totalFrames,
        const uint8_t* frameData,
        int dataLen,
        cv::Mat& outFrame
    );
    
    // 获取数据单元格数量（用于测试）
    int getMergedDataCellsSize();
    
    // 获取数据单元格（用于测试）
    std::vector<CellPos> getMergedDataCells();

    void drawSafeArea(cv::Mat& mat);
    void drawPositionMarkers(cv::Mat& mat);
    void drawDataAreas(cv::Mat& mat);
    void drawHeaderArea(cv::Mat& mat);

private:
    void writeHeaderField(cv::Mat& mat, int fieldId, uint16_t value, int tailLen, Config::QRCode::FrameType frameType);
    void writeBytesToCells(cv::Mat& mat, const uint8_t* data, int len);
};