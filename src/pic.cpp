#include "pic.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace ImgParse {

    using namespace std;
    using namespace cv;

    // 静态全局缓存：保存上一次成功解析的透视变换矩阵
    // 用于应对单帧极度模糊时的时空追踪兜底
    //
    static Mat lastValidTransform;

    struct Marker {
        Point2f center;
        double area;
    };

    // 统计局部区域黑色像素的面积，用于 V5 兜底判断方向
    // 降维打击：在拉平的正方形中，四角到中心的距方 (dist^2) 恒等
    // 因此比较面积等价于比较 area / dist^2，完美继承 warp_engine 核心思想
    //
    int getBlackArea(const Mat& corner) {
        Mat binCorner;
        threshold(corner, binCorner, 0, 255, THRESH_BINARY | THRESH_OTSU);
        return (corner.rows * corner.cols) - countNonZero(binCorner);
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

        // 借鉴 warp_engine 的色相分离掩膜操作，剔除背景干扰
        //
        if (srcImg.channels() == 3) {
            Mat hsv, satMask;
            cvtColor(srcImg, hsv, COLOR_BGR2HSV);
            vector<Mat> hsv_ch;
            split(hsv, hsv_ch);

            // 高饱和度区域绝对是背景杂物，直接提取并抹黑
            //
            threshold(hsv_ch[1], satMask, 100, 255, THRESH_BINARY);
            cvtColor(srcImg, gray, COLOR_BGR2GRAY);
            gray.setTo(0, satMask);
        }
        else {
            gray = srcImg.clone();
        }

        float scale = 800.0f / std::max(srcImg.cols, srcImg.rows);
        if (scale > 1.0f) scale = 1.0f;
        resize(gray, small_img, Size(), scale, scale, INTER_AREA);

        // 局部自适应，代替脆弱的大津法，抵抗反光
        //
        Mat binaryForOuter;
        adaptiveThreshold(small_img, binaryForOuter, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, 101, 0);

        // 使用 5x5 交叉核，足以缝合断裂的 SafeArea 边框且保留完美的四个锐角
        //
        Mat kernelOuter = getStructuringElement(MORPH_CROSS, Size(5, 5));
        Mat closedForOuter;
        morphologyEx(binaryForOuter, closedForOuter, MORPH_CLOSE, kernelOuter);

        vector<vector<Point>> outerContours;

        // RETR_EXTERNAL 完美无视内部撕裂的花纹，只抓取最外层白框
        //
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

        // 拉平到 798x798 高清平面，放大面积差异
        //
        vector<Point2f> dstPointsOuter = {
            Point2f(0.0f, 0.0f), Point2f(798.0f, 0.0f),
            Point2f(798.0f, 798.0f), Point2f(0.0f, 798.0f)
        };

        Mat M_Outer = getPerspectiveTransform(srcPointsOuter, dstPointsOuter);
        Mat warped798;
        warpPerspective(gray, warped798, M_Outer, Size(798, 798), INTER_LINEAR);

        Mat binWarped;
        threshold(warped798, binWarped, 0, 255, THRESH_BINARY | THRESH_OTSU);

        // 宏观统计四角黑面积，精准制导旋转方向
        // 798 体系下，定位块占据 126x126 的空间
        //
        int cornerSize = 126; 
        Rect tl(0, 0, cornerSize, cornerSize);
        Rect tr(798 - cornerSize, 0, cornerSize, cornerSize);
        Rect br(798 - cornerSize, 798 - cornerSize, cornerSize, cornerSize);
        Rect bl(0, 798 - cornerSize, cornerSize, cornerSize);

        int areas[4] = {
            getBlackArea(binWarped(tl)), getBlackArea(binWarped(tr)),
            getBlackArea(binWarped(br)), getBlackArea(binWarped(bl))
        };

        int minArea = areas[0];
        int smallQrIdx = 0;
        for (int i = 1; i < 4; ++i) {
            if (areas[i] < minArea) {
                minArea = areas[i];
                smallQrIdx = i;
            }
        }

        // 记录并生成最终供全局兜底使用的矩阵 (133 缩放系)
        // 根据旋转方向，将源顶点映射到正确的 133x133 目标角
        //
        vector<Point2f> finalDst133;
        if (smallQrIdx == 0)      finalDst133 = { Point2f(133.0f,133.0f), Point2f(0.0f,133.0f), Point2f(0.0f,0.0f), Point2f(133.0f,0.0f) };
        else if (smallQrIdx == 1) finalDst133 = { Point2f(133.0f,0.0f), Point2f(133.0f,133.0f), Point2f(0.0f,133.0f), Point2f(0.0f,0.0f) };
        else if (smallQrIdx == 3) finalDst133 = { Point2f(0.0f,133.0f), Point2f(0.0f,0.0f), Point2f(133.0f,0.0f), Point2f(133.0f,133.0f) };
        else                      finalDst133 = { Point2f(0.0f,0.0f), Point2f(133.0f,0.0f), Point2f(133.0f,133.0f), Point2f(0.0f,133.0f) };

        // 更新全局矩阵缓存
        //
        lastValidTransform = getPerspectiveTransform(srcPointsOuter, finalDst133);

        Mat final798;
        if (smallQrIdx == 0) rotate(binWarped, final798, ROTATE_180);
        else if (smallQrIdx == 1) rotate(binWarped, final798, ROTATE_90_CLOCKWISE);
        else if (smallQrIdx == 3) rotate(binWarped, final798, ROTATE_90_COUNTERCLOCKWISE);
        else final798 = binWarped.clone();

        // 完美中心降采样：从规整的 798 图提取 133，避开边缘模糊
        //
        disImg.create(133, 133, CV_8UC3);
        for (int r = 0; r < 133; ++r) {
            for (int c = 0; c < 133; ++c) {
                int py = r * 6 + 3;
                int px = c * 6 + 3;
                uint8_t val = final798.at<uint8_t>(py, px);
                disImg.at<Vec3b>(r, c) = val ? Vec3b(255, 255, 255) : Vec3b(0, 0, 0);
            }
        }

        return true;
    }

    // ==========================================
    // V15 原汁原味高精度内部特征提取逻辑 
    // ==========================================
    //
    bool processV15(const Mat& srcImg, Mat& gray, Mat& disImg, bool useHSV) {
        Mat blurred, binaryForContours;

        // HSV 滤镜：提取饱和度，干掉录屏光斑与彩色背景噪点
        //
        if (useHSV && srcImg.channels() == 3) {
            Mat hsv, binaryMask;
            cvtColor(srcImg, hsv, COLOR_BGR2HSV);
            vector<Mat> hsv_ch;
            split(hsv, hsv_ch);

            // 提取饱和度大于 180 的区域，全部抹白
            //
            threshold(hsv_ch[1], binaryMask, 180, 255, THRESH_BINARY);
            gray.setTo(255, binaryMask);
        }

        GaussianBlur(gray, blurred, Size(5, 5), 0);
        adaptiveThreshold(blurred, binaryForContours, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 31, 10);

        // L 型微切片形态学手术刀，专切摩尔纹
        //
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

            if (area0 < 15) continue;

            double r01 = area0 / max(area1, 1.0);
            double r12 = area1 / max(area2, 1.0);

            if (r01 > 1.2 && r01 < 8.0 && r12 > 1.2 && r12 < 8.0) {
                Moments M = moments(contours[i]);
                if (M.m00 != 0) {
                    markers.push_back({ Point2f(M.m10 / M.m00, M.m01 / M.m00), area0 });
                }
            }
        }

        // 空间去重合并
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

        // 核心修复：严格按面积降序排列！
        // 保证提取出来的前 3 个必定是三大主定位块，无视中间乱入的小噪点。
        //
        std::sort(markers.begin(), markers.end(), [](const Marker& a, const Marker& b) {
            return a.area > b.area;
            });

        // 强制使用前三个面积最大的点寻找直角 (即 TL)
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

        // 终极几何锁：针对三个主定位块进行自然法则约束，错位则放弃该帧
        //
        Point2f v1 = pt1 - TL;
        Point2f v2 = pt2 - TL;
        double len1 = norm(v1);
        double len2 = norm(v2);

        // 边长比例锁：允许一定透视，但直角边长比例绝不应超过 1.6 倍
        //
        double legRatio = len1 / max(len2, 1.0);
        if (legRatio < 0.6 || legRatio > 1.6) return false;

        // 夹角锁：利用点乘判定必须大致是直角，余弦绝对值大于 0.5 意味着发生严重共线噪点
        //
        double cosTheta = (v1.x * v2.x + v1.y * v2.y) / max(len1 * len2, 1.0);
        if (std::abs(cosTheta) > 0.5) return false;

        double cross = v1.x * v2.y - v1.y * v2.x;
        Point2f TR, BL;
        if (cross > 0) { TR = pt1; BL = pt2; }
        else { TR = pt2; BL = pt1; }

        Point2f BR;
        bool foundBR = false;
        Point2f expectedBR = TR + BL - TL;

        // 从剩下的其他标记中，寻找最靠近推导出来的右下角的点
        //
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
            // 若该点距离误差在边长的 40% 内，说明是真实的 SmallQrPoint
            //
            if (minDist < max(len1, len2) * 0.4) {
                BR = markers[bestIdx].center;
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

        Mat transformMatrix = getPerspectiveTransform(srcPoints, dstPoints);

        // 更新全局缓存，用于后续帧如果崩溃时兜底
        //
        lastValidTransform = transformMatrix.clone();

        Mat grayWarped;
        warpPerspective(gray, grayWarped, transformMatrix, Size(133, 133), INTER_LINEAR);

        Mat binWarped;
        threshold(grayWarped, binWarped, 0, 255, THRESH_BINARY | THRESH_OTSU);

        cvtColor(binWarped, disImg, COLOR_GRAY2BGR);
        return true;
    }

    bool Main(const cv::Mat& srcImg, cv::Mat& disImg) {
        if (srcImg.empty()) return false;

        // 视频分辨率锁：防止连续处理多个不同视频时缓存污染
        //
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
        if (aspect > 0.95 && aspect < 1.05 && srcImg.cols > 266) {
            Mat grayForDigital;
            if (srcImg.channels() == 3) cvtColor(srcImg, grayForDigital, COLOR_BGR2GRAY);
            else grayForDigital = srcImg.clone();

            disImg.create(133, 133, CV_8UC3);
            Mat binRaw;
            threshold(grayForDigital, binRaw, 0, 255, THRESH_BINARY | THRESH_OTSU);

            float stepX = (float)srcImg.cols / 133.0f;
            float stepY = (float)srcImg.rows / 133.0f;

            for (int r = 0; r < 133; ++r) {
                for (int c = 0; c < 133; ++c) {
                    int px = std::min(static_cast<int>((c + 0.5f) * stepX), srcImg.cols - 1);
                    int py = std::min(static_cast<int>((r + 0.5f) * stepY), srcImg.rows - 1);
                    uint8_t val = binRaw.at<uint8_t>(py, px);
                    disImg.at<Vec3b>(r, c) = val ? Vec3b(255, 255, 255) : Vec3b(0, 0, 0);
                }
            }
            return true;
        }

        // 处理前 3 帧：最容易因为曝光撕裂产生激光，必须用 V5 暴力外框提取兜底
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

        // 常规帧：视频稳定后，使用最精细的 V15 逻辑提取，绝杀定位点
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
        // 如果所有特征提取均宣告失败，直接借用上一帧成功的透视矩阵！
        // ==========================================
        //
        if (!lastValidTransform.empty()) {
            Mat grayWarped;

            // 直接使用上一次成功的矩阵对当前原图进行提取
            //
            warpPerspective(grayNormal, grayWarped, lastValidTransform, Size(133, 133), INTER_LINEAR);

            Mat binWarped;
            threshold(grayWarped, binWarped, 0, 255, THRESH_BINARY | THRESH_OTSU);

            cvtColor(binWarped, disImg, COLOR_GRAY2BGR);
            return true;
        }

        return false;
    }

} // namespace ImgParse