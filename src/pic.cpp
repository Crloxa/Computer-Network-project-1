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

    // 在层级树中寻找面积最大的子轮廓
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

    bool Main(const cv::Mat& srcImg, cv::Mat& disImg) {
        if (srcImg.empty()) return false;

        Mat gray, blurred;
        cvtColor(srcImg, gray, COLOR_BGR2GRAY);

        // 高斯模糊降低残影和视频压缩带来的高频噪点
        //
        GaussianBlur(gray, blurred, Size(5, 5), 0);

        // 针对录屏的光照不均问题，采用自适应阈值提取轮廓，抗干扰极强
        //
        Mat binaryForContours;
        adaptiveThreshold(blurred, binaryForContours, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 31, 10);

        vector<vector<Point>> contours;
        vector<Vec4i> hierarchy;
        findContours(binaryForContours, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);

        vector<Marker> markers;

        // 寻找 3 层嵌套的回字型轮廓
        //
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

            // 依据大定位点的比例放宽条件，兼容一定的透视变形
            //
            if (r01 > 1.2 && r01 < 8.0 && r12 > 1.2 && r12 < 8.0) {
                Moments M = moments(contours[i]);
                if (M.m00 != 0) {
                    markers.push_back({ Point2f(M.m10 / M.m00, M.m01 / M.m00), area0 });
                }
            }
        }

        // 合并离得太近的噪点重心
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

        // 找到了足够的定位点，进入几何约束验证阶段
        //
        if (markers.size() >= 3) {
            sort(markers.begin(), markers.end(), [](const Marker& a, const Marker& b) {
                return a.area > b.area;
                });

            // 找直角定点 (即左上角 TL)
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

            // 严格几何约束：两条直角边长度必须接近，否则判定为找错了点，拒绝该帧！
            //
            double len1 = norm(pt1 - TL);
            double len2 = norm(pt2 - TL);
            double legRatio = len1 / max(len2, 1.0);

            if (legRatio > 0.5 && legRatio < 2.0) {

                // 向量叉积识别右上角与左下角
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
        }

        // ==========================================
        // 方案一：执行带透视校正的图像渲染
        // ==========================================
        //
        if (usePerspective) {
            Mat grayWarped;

            // 关键优化：先将纯灰度图拉平，不带有任何二值化噪点
            //
            warpPerspective(gray, grayWarped, transformMatrix, Size(133, 133), INTER_LINEAR);

            Mat binWarped;
            // 在拉平后的 133x133 完美子区域内执行大津法，彻底屏蔽背景亮度干扰，有效对抗残影
            //
            threshold(grayWarped, binWarped, 0, 255, THRESH_BINARY | THRESH_OTSU);

            cvtColor(binWarped, disImg, COLOR_GRAY2BGR);
            return true;
        }
        // ==========================================
        // 方案二：针对没有变形的标准压制视频，执行纯数字采样
        // ==========================================
        //
        else {
            double aspect = (double)srcImg.cols / srcImg.rows;
            if (aspect > 0.95 && aspect < 1.05 && srcImg.cols > 266) {
                disImg.create(133, 133, CV_8UC3);

                Mat binRaw;
                threshold(gray, binRaw, 0, 255, THRESH_BINARY | THRESH_OTSU);

                float stepX = (float)srcImg.cols / 133.0f;
                float stepY = (float)srcImg.rows / 133.0f;

                for (int r = 0; r < 133; ++r) {
                    for (int c = 0; c < 133; ++c) {

                        // 强制取中心点避免边缘模糊
                        //
                        int px = std::min(static_cast<int>((c + 0.5f) * stepX), srcImg.cols - 1);
                        int py = std::min(static_cast<int>((r + 0.5f) * stepY), srcImg.rows - 1);

                        uint8_t val = binRaw.at<uint8_t>(py, px);
                        disImg.at<Vec3b>(r, c) = val ? Vec3b(255, 255, 255) : Vec3b(0, 0, 0);
                    }
                }
                return true;
            }
        }

        // 无法识别也无法直接采样，返回 false 抛弃该失真帧
        //
        return false;
    }

} // namespace ImgParse