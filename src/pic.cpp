#include "pic.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace ImgParse {

    using namespace std;
    using namespace cv;

    struct Marker {
        Point2f center;
        double area;
    };

    // 安全获取最大轮廓子节点
    //
    int findLargestChild(int parentIdx, const vector<vector<Point>>& contours, const vector<Vec4i>& hierarchy) {
        int max_idx = -1;
        double max_area = -1.0;
        int child = hierarchy[parentIdx][2];
        while (child >= 0) {
            double area = contourArea(contours[child]);
            if (area > max_area) {
                max_area = area;
                max_idx = child;
            }
            child = hierarchy[child][0]; // 遍历同层节点
            //
        }
        return max_idx;
    }

    bool Main(const cv::Mat& srcImg, cv::Mat& disImg) {
        if (srcImg.empty()) return true;

        Mat gray;
        cvtColor(srcImg, gray, COLOR_BGR2GRAY);

        // 首先进行二值化，采用 OTSU 大津法自动寻找黑白界限
        //
        Mat binary;
        threshold(gray, binary, 0, 255, THRESH_BINARY_INV | THRESH_OTSU);

        vector<vector<Point>> contours;
        vector<Vec4i> hierarchy;
        findContours(binary, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);

        vector<Marker> markers;

        // 1. 尝试寻找三层回字形嵌套轮廓，对抗物理摄像头畸变拍摄
        //
        for (size_t i = 0; i < contours.size(); ++i) {
            int c1 = findLargestChild(i, contours, hierarchy);
            if (c1 < 0) continue;
            int c2 = findLargestChild(c1, contours, hierarchy);
            if (c2 < 0) continue;

            double area0 = contourArea(contours[i]);
            double area1 = contourArea(contours[c1]);
            double area2 = contourArea(contours[c2]);

            if (area0 < 10) continue;

            double r01 = area0 / max(area1, 1.0);
            double r12 = area1 / max(area2, 1.0);

            // 依据 code.cpp 中 QrPoint 特征设定的物理比例 (容错放宽，防压缩形变)
            //
            if (r01 > 1.1 && r01 < 6.0 && r12 > 1.1 && r12 < 6.0) {
                Moments M = moments(contours[i]);
                if (M.m00 != 0) {
                    markers.push_back({ Point2f(M.m10 / M.m00, M.m01 / M.m00), area0 });
                }
            }
        }

        // 剔除重复的识别噪点
        //
        vector<Marker> uniqueMarkers;
        for (const auto& m : markers) {
            bool duplicate = false;
            for (auto& um : uniqueMarkers) {
                if (norm(m.center - um.center) < 15.0) {
                    if (m.area > um.area) {
                        um.area = m.area;
                        um.center = m.center;
                    }
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                uniqueMarkers.push_back(m);
            }
        }
        markers = uniqueMarkers;

        bool usePerspective = false;
        Mat transformMatrix;

        // 找到了定位点，执行透视校正
        //
        if (markers.size() >= 3) {
            sort(markers.begin(), markers.end(), [](const Marker& a, const Marker& b) {
                return a.area > b.area;
                });

            // 寻找包含直角的左上角点
            //
            double maxDist = 0;
            int rightAngleIdx = -1;
            for (int i = 0; i < 3; ++i) {
                for (int j = i + 1; j < 3; ++j) {
                    double d = norm(markers[i].center - markers[j].center);
                    if (d > maxDist) {
                        maxDist = d;
                        rightAngleIdx = 3 - i - j;
                    }
                }
            }

            Point2f TL = markers[rightAngleIdx].center;
            Point2f pt1 = markers[(rightAngleIdx + 1) % 3].center;
            Point2f pt2 = markers[(rightAngleIdx + 2) % 3].center;

            // 使用向量叉积辨别右上角与左下角
            //
            Point2f v1 = pt1 - TL;
            Point2f v2 = pt2 - TL;
            double cross = v1.x * v2.y - v1.y * v2.x;

            Point2f TR, BL;
            if (cross > 0) { TR = pt1; BL = pt2; }
            else { TR = pt2; BL = pt1; }

            // 推导可能缺少的右下角点
            //
            Point2f BR;
            bool foundBR = false;
            Point2f expectedBR = TR + BL - TL;

            if (markers.size() > 3) {
                double minDist = 1e9;
                for (size_t i = 3; i < markers.size(); ++i) {
                    double d = norm(markers[i].center - expectedBR);
                    if (d < minDist) {
                        minDist = d;
                        BR = markers[i].center;
                    }
                }
                if (minDist < norm(TR - TL) * 0.4) {
                    foundBR = true;
                }
            }
            if (!foundBR) BR = expectedBR;

            vector<Point2f> srcPoints = { TL, TR, BR, BL };
            vector<Point2f> dstPoints = {
                Point2f(10.0f, 10.0f),
                Point2f(122.0f, 10.0f),
                foundBR ? Point2f(126.0f, 126.0f) : Point2f(122.0f, 122.0f),
                Point2f(10.0f, 122.0f)
            };

            transformMatrix = getPerspectiveTransform(srcPoints, dstPoints);
            usePerspective = true;
        }

        // ========================================== //
        // 2. 根据特征点寻找情况，执行二选一图像渲染策略
        // ========================================== //
        if (usePerspective) {
            // [策略A：相机物理拍摄模式]
            //
            Mat warpedImg;
            warpPerspective(srcImg, warpedImg, transformMatrix, Size(133, 133), INTER_LINEAR);

            Mat grayWarped, binWarped;
            cvtColor(warpedImg, grayWarped, COLOR_BGR2GRAY);
            threshold(grayWarped, binWarped, 0, 255, THRESH_BINARY | THRESH_OTSU);
            cvtColor(binWarped, disImg, COLOR_GRAY2BGR);
        }
        else {
            // [策略B：标准视频提取模式 (无物理透视)]
            // 应对你现在测试的 FFMPEG 生成件，绕过糟糕的 resize INTER_NEAREST 边缘抖动！
            // 采用完美核心采样点提取，直接净化视频噪点！
            //
            disImg.create(133, 133, CV_8UC3);

            Mat binRaw;
            // 原图大图二值化去除压缩杂色
            //
            threshold(gray, binRaw, 0, 255, THRESH_BINARY | THRESH_OTSU);

            float stepX = (float)srcImg.cols / 133.0f;
            float stepY = (float)srcImg.rows / 133.0f;

            for (int r = 0; r < 133; ++r) {
                for (int c = 0; c < 133; ++c) {
                    // c + 0.5f：强制游标走到每一个 10x10 色块的核心正中间进行采集
                    //
                    int px = std::min(static_cast<int>((c + 0.5f) * stepX), srcImg.cols - 1);
                    int py = std::min(static_cast<int>((r + 0.5f) * stepY), srcImg.rows - 1);

                    uint8_t val = binRaw.at<uint8_t>(py, px);
                    // 直接向解码器注入 CV_8UC3 的二极化纯净颜色
                    //
                    disImg.at<Vec3b>(r, c) = val ? Vec3b(255, 255, 255) : Vec3b(0, 0, 0);
                }
            }
        }

        // 永远返回 false (0) 成功接管处理流，阻止外层的有害降级。
        // 这也会确保你在 pic_output 文件夹内看到生成得十分完美的纯黑白采样图像。
        //
        return false;
    }

} // namespace ImgParse