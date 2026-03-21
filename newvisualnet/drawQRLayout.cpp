#include "Config.h"
#include "QRPosition.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <algorithm>

static std::string joinPath(const std::string& parent, const std::string& child) {
    if (parent.empty()) return child;
    if (child.empty()) return parent;
    char lastChar = parent.back();
    if (lastChar == '/' || lastChar == '\\') return parent + child;
#ifdef _WIN32
    return parent + "\\" + child;
#else
    return parent + "/" + child;
#endif
}

static void drawRegionOutline(cv::Mat& mat, int top, int left, int height, int width, const cv::Vec3b& color, const std::string& name) {
    const int bottom = top + height - 1;
    const int right = left + width - 1;

    for (int col = left; col <= right; ++col) {
        mat.at<cv::Vec3b>(top, col) = color;
        mat.at<cv::Vec3b>(bottom, col) = color;
    }
    for (int row = top; row <= bottom; ++row) {
        mat.at<cv::Vec3b>(row, left) = color;
        mat.at<cv::Vec3b>(row, right) = color;
    }

    cv::putText(mat, name, cv::Point(left + 2, top + 15),
        cv::FONT_HERSHEY_SIMPLEX, 0.3, color, 1);
}

static cv::Mat generateLayout() {
    // 常量调用100%统一
    cv::Mat layout(Config::QRCode::FrameSize, Config::QRCode::FrameSize, CV_8UC3, cv::Vec3b(255, 255, 255));

    QRPosition qrPos;
    qrPos.drawSafeArea(layout);
    qrPos.drawPositionMarkers(layout);

    drawRegionOutline(layout, 3, 21, 3, 16, cv::Vec3b(0, 0, 255), "header");
    drawRegionOutline(layout, 3, 37, 3, 75, cv::Vec3b(255, 0, 0), "data0");
    drawRegionOutline(layout, 6, 21, 15, 91, cv::Vec3b(255, 0, 0), "data1");
    drawRegionOutline(layout, 21, 3, 88, 127, cv::Vec3b(0, 255, 0), "data2");
    drawRegionOutline(layout, 109, 3, 3, 127, cv::Vec3b(0, 255, 255), "data3");
    drawRegionOutline(layout, 112, 21, 18, 91, cv::Vec3b(255, 255, 0), "data4");
    drawRegionOutline(layout, 112, 112, 18, 18, cv::Vec3b(255, 0, 255), "corner");

    return layout;
}

static cv::Mat scaleLayout(const cv::Mat& layout) {
    cv::Mat scaled;
    // 常量调用100%统一
    cv::resize(layout, scaled, cv::Size(Config::QRCode::OutputFrame_W, Config::QRCode::OutputFrame_H),
        0, 0, cv::INTER_NEAREST);
    return scaled;
}

static bool saveLayout(const cv::Mat& layout, const std::string& savePath) {
    cv::Mat scaled = scaleLayout(layout);
    return cv::imwrite(savePath, scaled);
}

int drawQRLayoutMain(const std::string& savePath) {
    try {
        cv::Mat layout = generateLayout();
        if (!saveLayout(layout, savePath)) {
            std::cerr << "[ERROR] Save layout failed: " << savePath << std::endl;
            return 1;
        }
        std::cout << "[INFO] Layout saved to: " << savePath << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "[ERROR] Draw layout failed: " << e.what() << std::endl;
        return 1;
    }
}

bool drawQRLayout(const std::string& savePath) {
    return drawQRLayoutMain(savePath) == 0;
}