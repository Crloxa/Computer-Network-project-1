#include "pic.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <array>

namespace ImgParse {

    using namespace std;
    using namespace cv;

    // 静态全局缓存：保存上一次成功解析的透视变换矩阵
    // 用于应对单帧极度模糊或闪光时的时空追踪兜底
    //
    static Mat lastValidTransform;

    struct Marker {
        Point2f center;
        double area;
    };

    void normalizeBinaryPolarity(Mat& binWarped) {
        if (binWarped.rows != 266 || binWarped.cols != 266 || binWarped.type() != CV_8UC1) return;

        struct ProbePoint {
            int row;
            int col;
            bool expectBlack;
        };

        const std::array<ProbePoint, 16> probes = { {
            {21, 21, true}, {21, 30, false}, {21, 34, true}, {21, 39, false},
            {21, 245, true}, {21, 236, false}, {21, 232, true}, {21, 227, false},
            {245, 21, true}, {236, 21, false}, {232, 21, true}, {227, 21, false},
            {252, 252, true}, {249, 249, false}, {247, 247, true}, {258, 258, true}
        } };

        int directMatch = 0;
        int invertedMatch = 0;
        for (const auto& p : probes) {
            const bool black = binWarped.at<uint8_t>(p.row, p.col) < 128;
            if (black == p.expectBlack) directMatch++;
            if ((!black) == p.expectBlack) invertedMatch++;
        }

        if (invertedMatch > directMatch) {
            bitwise_not(binWarped, binWarped);
        }
    }

    // 统计局部区域黑色像素的面积，用于 V5 兜底判断方向
    // 【核心修复】：传入的 corner 已经是全局二值化图像
    // 绝不能再次使用 OTSU，否则会导致局部色彩判定反转。直接统计零像素即可！
    //
    int getBlackArea(const Mat& corner) {
        return (corner.rows * corner.cols) - countNonZero(corner);
    }

    // V15 最稳健的三层嵌套轮廓寻找器
    // 极度严苛的层级校验，杜绝几乎所有背景干扰
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
            child = hierarchy[child][0];
        }
        return max_idx;
    }

    // ==========================================
    // V5 强化版：专治前 3 帧的录屏撕裂与极度模糊
    // ==========================================
    //
    bool processV5(const Mat& srcImg, Mat& disImg) {
        Mat gray, small_img;

        // 色相分离掩膜操作，剔除背景干扰
        //
        if (srcImg.channels() == 3) {
            Mat hsv, satMask;
            cvtColor(srcImg, hsv, COLOR_BGR2HSV);
            vector<Mat> hsv_ch;
            split(hsv, hsv_ch);

            threshold(hsv_ch[1], satMask, 100, 255, THRESH_BINARY);
            cvtColor(srcImg, gray, COLOR_BGR2GRAY);
            gray.setTo(0, satMask);
        }
        else {
            gray = srcImg.clone();
        }

        // V5 专用宏观轮廓缩放：缩小到 800，极大加快闭运算与轮廓查找速度
        //
        float scale = 800.0f / std::max(srcImg.cols, srcImg.rows);
        if (scale > 1.0f) scale = 1.0f;
        resize(gray, small_img, Size(), scale, scale, INTER_AREA);

        Mat binaryForOuter;
        adaptiveThreshold(small_img, binaryForOuter, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, 101, 0);

        Mat kernelOuter = getStructuringElement(MORPH_CROSS, Size(5, 5));
        Mat closedForOuter;
        morphologyEx(binaryForOuter, closedForOuter, MORPH_CLOSE, kernelOuter);

        vector<vector<Point>> outerContours;
        findContours(closedForOuter, outerContours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
        if (outerContours.empty()) return false;

        int max_idx = -1;
        double max_area = 0;
        for (size_t i = 0; i < outerContours.size(); i++) {
            double area = contourArea(outerContours[i]);
            if (area > max_area) { max_area = area; max_idx = i; }
        }
        if (max_area < 2000) return false;

        // 先取凸包抹平内凹噪点，确保百分百拟合出四边形
        //
        vector<Point> hull;
        convexHull(outerContours[max_idx], hull);

        vector<Point> approx;
        double epsilon = 0.05 * arcLength(hull, true);
        approxPolyDP(hull, approx, epsilon, true);

        if (approx.size() != 4) {
            RotatedRect minRect = minAreaRect(hull);
            Point2f rect_points[4];
            minRect.points(rect_points);
            approx.clear();
            for (int j = 0; j < 4; j++) approx.push_back(rect_points[j]);
        }

        // 坐标逆映射：除以 scale，将坐标还原到高清原图上！
        //
        vector<Point2f> srcPointsOuter(4);
        for (int i = 0; i < 4; i++) {
            srcPointsOuter[i] = Point2f(approx[i].x / scale, approx[i].y / scale);
        }

        Point2f centerOuter(0, 0);
        for (int i = 0; i < 4; i++) centerOuter += srcPointsOuter[i];
        centerOuter.x /= 4.0f;
        centerOuter.y /= 4.0f;

        std::sort(srcPointsOuter.begin(), srcPointsOuter.end(), [&centerOuter](const Point2f& a, const Point2f& b) {
            return atan2(a.y - centerOuter.y, a.x - centerOuter.x) < atan2(b.y - centerOuter.y, b.x - centerOuter.x);
            });

        // 方向探测器：拉平到 532x532 (266的2倍)，彻底放大定位块差异
        //
        vector<Point2f> dstPointsOuter = {
            Point2f(0.0f, 0.0f), Point2f(532.0f, 0.0f),
            Point2f(532.0f, 532.0f), Point2f(0.0f, 532.0f)
        };

        Mat M_Outer = getPerspectiveTransform(srcPointsOuter, dstPointsOuter);
        Mat warped532;
        warpPerspective(gray, warped532, M_Outer, Size(532, 532), INTER_LINEAR);

        // 探测图二值化
        //
        Mat binWarped532;
        threshold(warped532, binWarped532, 0, 255, THRESH_BINARY | THRESH_OTSU);

        int cornerSize = 88;
        Rect tl(0, 0, cornerSize, cornerSize);
        Rect tr(532 - cornerSize, 0, cornerSize, cornerSize);
        Rect br(532 - cornerSize, 532 - cornerSize, cornerSize, cornerSize);
        Rect bl(0, 532 - cornerSize, cornerSize, cornerSize);

        int areas[4] = {
            getBlackArea(binWarped532(tl)), getBlackArea(binWarped532(tr)),
            getBlackArea(binWarped532(br)), getBlackArea(binWarped532(bl))
        };

        int minArea = areas[0];
        int smallQrIdx = 0;
        for (int i = 1; i < 4; ++i) {
            if (areas[i] < minArea) {
                minArea = areas[i];
                smallQrIdx = i;
            }
        }

        // 根据检测出的方向，生成映射回标准 266x266 的终极透视矩阵
        //
        vector<Point2f> finalDst266;
        if (smallQrIdx == 0)      finalDst266 = { Point2f(266.0f,266.0f), Point2f(0.0f,266.0f), Point2f(0.0f,0.0f), Point2f(266.0f,0.0f) };
        else if (smallQrIdx == 1) finalDst266 = { Point2f(266.0f,0.0f), Point2f(266.0f,266.0f), Point2f(0.0f,266.0f), Point2f(0.0f,0.0f) };
        else if (smallQrIdx == 3) finalDst266 = { Point2f(0.0f,266.0f), Point2f(0.0f,0.0f), Point2f(266.0f,0.0f), Point2f(266.0f,266.0f) };
        else                      finalDst266 = { Point2f(0.0f,0.0f), Point2f(266.0f,0.0f), Point2f(266.0f,266.0f), Point2f(0.0f,266.0f) };

        lastValidTransform = getPerspectiveTransform(srcPointsOuter, finalDst266);

        // 完美契合原代码：抛弃抽样，从原灰度图直接裁剪 266 享受平滑抗锯齿
        //
        Mat grayWarped;
        warpPerspective(gray, grayWarped, lastValidTransform, Size(266, 266), INTER_LINEAR);

        Mat binWarped;
        threshold(grayWarped, binWarped, 0, 255, THRESH_BINARY | THRESH_OTSU);
        normalizeBinaryPolarity(binWarped);

        cvtColor(binWarped, disImg, COLOR_GRAY2BGR);
        return true;
    }

    // ==========================================
    // V15 护城河版：回归 V29 稳健性 + 1080p 安全提速
    // ==========================================
    //
    bool processV15(const Mat& srcImg, Mat& gray, Mat& disImg, bool useHSV) {
        Mat small_img, blurred, binaryForContours;

        if (useHSV && srcImg.channels() == 3) {
            Mat hsv, binaryMask;
            cvtColor(srcImg, hsv, COLOR_BGR2HSV);
            vector<Mat> hsv_ch;
            split(hsv, hsv_ch);

            threshold(hsv_ch[1], binaryMask, 180, 255, THRESH_BINARY);
            gray.setTo(255, binaryMask);
        }

        // 【1080p 安全护城河】：限制最大尺寸为 1920
        // 如果是 4K 会被极速压到 1080p，如果本身是 1080p 则 scale=1 完全无损！
        // 杜绝了暴力压成更小导致的拓扑缝隙糊死问题
        //
        float scale = 1920.0f / std::max(srcImg.cols, srcImg.rows);
        if (scale > 1.0f) scale = 1.0f;
        resize(gray, small_img, Size(), scale, scale, INTER_AREA);

        // 既然限制在 1080p，我们就可以安全沿用原作者精调的物理参数
        //
        GaussianBlur(small_img, blurred, Size(5, 5), 0);
        adaptiveThreshold(blurred, binaryForContours, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 31, 10);

        Mat kernel = getStructuringElement(MORPH_CROSS, Size(2, 2));
        Mat closedBinary;
        morphologyEx(binaryForContours, closedBinary, MORPH_CLOSE, kernel);

        vector<vector<Point>> contours;
        vector<Vec4i> hierarchy;
        findContours(closedBinary, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);

        vector<Marker> markers;

        for (size_t i = 0; i < contours.size(); ++i) {
            int c1 = findLargestChild(i, contours, hierarchy);
            if (c1 < 0) continue;
            int c2 = findLargestChild(c1, contours, hierarchy);
            if (c2 < 0) continue;

            double area0 = contourArea(contours[i]);
            double area1 = contourArea(contours[c1]);
            double area2 = contourArea(contours[c2]);

            // 面积门槛等比折算，放过小图情况
            //
            if (area0 < 15.0 * scale * scale) continue;

            double r01 = area0 / max(area1, 1.0);
            double r12 = area1 / max(area2, 1.0);

            if (r01 > 1.2 && r01 < 8.0 && r12 > 1.2 && r12 < 8.0) {
                Moments M = moments(contours[i]);
                if (M.m00 != 0) {
                    // 除以 scale 还原回原生分辨率物理坐标
                    //
                    markers.push_back({
                        Point2f((M.m10 / M.m00) / scale, (M.m01 / M.m00) / scale),
                        area0 / (scale * scale)
                        });
                }
            }
        }

        // 空间去重合并：既然坐标已还原，直接沿用原生代码的 15.0 距离阈值
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
            if (!duplicate) uniqueMarkers.push_back(m);
        }
        markers = uniqueMarkers;

        if (markers.size() < 3) return false;

        // 按面积降序锁定三大主定位块
        //
        std::sort(markers.begin(), markers.end(), [](const Marker& a, const Marker& b) {
            return a.area > b.area;
            });

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

        Point2f v1 = pt1 - TL;
        Point2f v2 = pt2 - TL;
        double len1 = norm(v1);
        double len2 = norm(v2);

        // 【核心修复】：回归 V29 的大容错范围
        // 透视畸变时边长比例和角度变化剧烈，过于苛刻会导致零碎丢帧！
        //
        double legRatio = len1 / max(len2, 1.0);
        if (legRatio < 0.4 || legRatio > 2.5) return false;

        // 放宽至 0.75，允许 41° 到 139° 的严重透视倾斜
        //
        double cosTheta = (v1.x * v2.x + v1.y * v2.y) / max(len1 * len2, 1.0);
        if (std::abs(cosTheta) > 0.75) return false;

        double cross = v1.x * v2.y - v1.y * v2.x;
        Point2f TR, BL;
        if (cross > 0) { TR = pt1; BL = pt2; }
        else { TR = pt2; BL = pt1; }

        Point2f BR;
        bool foundBR = false;
        Point2f expectedBR = TR + BL - TL;

        if (markers.size() > 3) {
            double minDist = 1e9;
            int bestIdx = -1;
            for (size_t i = 3; i < markers.size(); ++i) {
                double d = norm(markers[i].center - expectedBR);
                if (d < minDist) {
                    minDist = d;
                    bestIdx = i;
                }
            }
            // 放大寻找右下角的宽容度
            //
            if (minDist < max(len1, len2) * 0.5) {
                BR = markers[bestIdx].center;
                foundBR = true;
            }
        }
        if (!foundBR) BR = expectedBR;

        vector<Point2f> srcPoints = { TL, TR, BR, BL };
        vector<Point2f> dstPoints = {
            Point2f(21.0f, 21.0f),
            Point2f(245.0f, 21.0f),
            foundBR ? Point2f(252.0f, 252.0f) : Point2f(245.0f, 245.0f),
            Point2f(21.0f, 245.0f)
        };

        Mat transformMatrix = getPerspectiveTransform(srcPoints, dstPoints);

        // 更新全局缓存矩阵，供无特征遮挡时续命
        //
        lastValidTransform = transformMatrix.clone();

        // 回归高保真！直接使用原生最高分辨率灰度图裁切，抗锯齿满分
        //
        Mat grayWarped;
        warpPerspective(gray, grayWarped, transformMatrix, Size(266, 266), INTER_LINEAR);

        Mat binWarped;
        threshold(grayWarped, binWarped, 0, 255, THRESH_BINARY | THRESH_OTSU);
        normalizeBinaryPolarity(binWarped);

        cvtColor(binWarped, disImg, COLOR_GRAY2BGR);
        return true;
    }

    bool Main(const cv::Mat& srcImg, cv::Mat& disImg) {
        if (srcImg.empty()) return false;

        static int last_cols = 0;
        static int last_rows = 0;
        static int v5_frame_count = 0;

        if (srcImg.cols != last_cols || srcImg.rows != last_rows) {
            last_cols = srcImg.cols;
            last_rows = srcImg.rows;
            v5_frame_count = 0;
            lastValidTransform = Mat();
        }

        // 拦截无形变的原始纯净视频导出帧
        //
        double aspect = (double)srcImg.cols / srcImg.rows;
        if (aspect > 0.95 && aspect < 1.05 && srcImg.cols > 532) {
            Mat grayForDigital;
            if (srcImg.channels() == 3) cvtColor(srcImg, grayForDigital, COLOR_BGR2GRAY);
            else grayForDigital = srcImg.clone();

            disImg.create(266, 266, CV_8UC3);
            Mat binRaw;
            threshold(grayForDigital, binRaw, 0, 255, THRESH_BINARY | THRESH_OTSU);

            float stepX = (float)srcImg.cols / 266.0f;
            float stepY = (float)srcImg.rows / 266.0f;

            for (int r = 0; r < 266; ++r) {
                for (int c = 0; c < 266; ++c) {
                    int px = std::min(static_cast<int>((c + 0.5f) * stepX), srcImg.cols - 1);
                    int py = std::min(static_cast<int>((r + 0.5f) * stepY), srcImg.rows - 1);
                    uint8_t val = binRaw.at<uint8_t>(py, px);
                    disImg.at<Vec3b>(r, c) = val ? Vec3b(255, 255, 255) : Vec3b(0, 0, 0);
                }
            }
            return true;
        }

        // 处理前 3 帧：最容易因为曝光撕裂产生激光，用 V5 暴力外框兜底
        //
        if (v5_frame_count < 3) {
            v5_frame_count++;
            if (processV5(srcImg, disImg)) {
                return true;
            }
        }

        Mat grayNormal;
        if (srcImg.channels() == 3) cvtColor(srcImg, grayNormal, COLOR_BGR2GRAY);
        else grayNormal = srcImg.clone();

        // 常规帧：使用 V15 稳健逻辑，拥有 1080p 护城河加持
        //
        if (processV15(srcImg, grayNormal, disImg, false)) {
            return true;
        }

        // 极端环境兜底：启动 HSV 色彩降维打击，过滤彩色背景和光斑后再重试 V15
        //
        if (srcImg.channels() == 3) {
            Mat grayHSV = grayNormal.clone();
            if (processV15(srcImg, grayHSV, disImg, true)) {
                return true;
            }
        }

        // ==========================================
        // 最终形态：时空一致性追踪兜底 (Temporal Tracking Fallback)
        // ==========================================
        //
        if (!lastValidTransform.empty()) {
            Mat grayWarped;

            warpPerspective(grayNormal, grayWarped, lastValidTransform, Size(266, 266), INTER_LINEAR);

            Mat binWarped;
            threshold(grayWarped, binWarped, 0, 255, THRESH_BINARY | THRESH_OTSU);
            normalizeBinaryPolarity(binWarped);

            cvtColor(binWarped, disImg, COLOR_GRAY2BGR);
            return true;
        }

        return false;
    }

} // namespace ImgParse
