#include"picture.h"

//#define FIND_QRPOINT_DEBUG 1
//#define CropParallelRect_DEBUG 1
#define Show_Img(src) do\
{\
	imshow("DEBUG", src);\
	waitKey();\
}while (0);

namespace ImgParse
{
    Mat Rotation_90(const Mat& srcImg)
    {
        // 直接使用OpenCV的rotate函数简化逻辑，减少了 transpose 和 flip 操作的组合逻辑
        Mat tempImg;
        rotate(srcImg, tempImg, ROTATE_90_CLOCKWISE); // 90度顺时针旋转
        return tempImg;
    }

    //以前通过提取轮廓上四个关键点的平均值来近似计算中心点，改进为计算所有点的几何重心
    Point2f CalRectCenter(const vector<Point>& contours)
    {
        // 通过所有点求几何中心，支持不规则形状
        float sumX = 0, sumY = 0;
        int n = contours.size();
        for (const auto& point : contours) {
            sumX += point.x;
            sumY += point.y;
        }
        return Point2f(sumX / n, sumY / n); // 计算所有点的平均值
    }

    //消除变量 ax, ay, bx, by 的赋值操作，减少临时变量
    bool IsClockWise(const Point& basePoint, const Point& point1, const Point& point2)
    {
        return (point1.x - basePoint.x) * (point2.y - basePoint.y) -
            (point2.x - basePoint.x) * (point1.y - basePoint.y) > 0;
    }


    float Cal3PointAngle(const Point& point0, const Point& point1, const Point& point2)
    {
        float dx1 = point1.x - point0.x, dy1 = point1.y - point0.y;
        float dx2 = point2.x - point0.x, dy2 = point2.y - point0.y;
        float numerator = dx1 * dx2 + dy1 * dy2;
        float denominator = std::hypot(dx1, dy1) * std::hypot(dx2, dy2) + 1e-10f;//使用 std::hypot 避免了直接平方和开根号导致的可能溢出或精度误差
        return acos(numerator / denominator) * 180.0f / 3.141592653f;
    }

    //主要是优化了矩形角点计算，避免创建无意义的 cv::Mat 类型变量 srcPoints，直接使用 std::vector
    Mat CropRect(const Mat& srcImg, const RotatedRect& rotatedRect)
    {
        vector<Point2f> srcPoints(4); // 容器直接存储顶点坐标
        rotatedRect.points(srcPoints.data());  // 填充矩形四个顶点

        vector<Point2f> dis_points =
        {
            Point2f(0, rotatedRect.size.height - 1),
            Point2f(0, 0),
            Point2f(rotatedRect.size.width - 1, 0),
            Point2f(rotatedRect.size.width - 1, rotatedRect.size.height - 1)
        };

        Mat M = getPerspectiveTransform(srcPoints, dis_points); // 计算变换矩阵
        Mat disImg;
        warpPerspective(srcImg, disImg, M, rotatedRect.size);    // 透视变换
#ifdef FIND_QRPOINT_DEBUG
        Show_Img(disImg);
#endif
        return disImg;
    }
    
    float distance(const Point2f& a, const Point2f& b)
    {
        return std::hypot(b.x - a.x, b.y - a.y);// std::hypot
    }

    //增强了几何验证，确保计算结果符合平行四边形的对称性质。
    Point CalForthPoint(const Point& poi0, const Point& poi1, const Point& poi2)
    {
        // 确保输入点不重合
        if (poi0 == poi1 || poi1 == poi2 || poi0 == poi2) {
            throw std::invalid_argument("Input points must be distinct.");
        }

        // 确保输入点不共线（向量叉积为 0 表示共线）
        if ((poi1.x - poi0.x) * (poi2.y - poi0.y) == (poi2.x - poi0.x) * (poi1.y - poi0.y)) {
            throw std::invalid_argument("Input points must not be collinear.");
        }
        Point forthPoint(poi2.x + poi1.x - poi0.x, poi2.y + poi1.y - poi0.y);
        // 验证平行四边形性质：对角线的中点相等
        Point midpoint1((poi0.x + poi2.x) / 2, (poi0.y + poi2.y) / 2);
        Point midpoint2((poi1.x + forthPoint.x) / 2, (poi1.y + forthPoint.y) / 2);
        if (midpoint1 != midpoint2) {
            throw std::logic_error("Computed point does not satisfy parallelogram symmetry.");
        }
        return forthPoint;
    }

    //减少了一些中间计算量，直接计算出扩展向量的分量，并使用 std::hypot 计算长度
    pair<float, float> CalExtendVec(const Point2f& poi0, const Point2f& poi1, const Point2f& poi2, float bias)
    {
        float dis0 = distance(poi0, poi1), dis1 = distance(poi0, poi2);
        float rate = dis1 / dis0;
        float totx = poi0.x * (1 + rate) - poi1.x * rate - poi2.x;
        float toty = poi0.y * (1 + rate) - poi1.y * rate - poi2.y;
        float distot = std::hypot(totx, toty);
        return { totx / distot * bias, toty / distot * bias };
    }

    //增加输入验证与异常处理
    Mat CropParallelRect(const Mat& srcImg, const vector<Point2f>& srcPoints, Size size = { 0, 0 })
    {
        // 验证输入点数量
        if (srcPoints.size() != 4) {
            throw std::invalid_argument("srcPoints must contain exactly 4 points.");
        }

        // 验证输入图像合法性
        if (srcImg.empty()) {
            throw std::invalid_argument("Input image is empty.");
        }

        // 验证输入点是否包含重合点
        for (int i = 0; i < 4; ++i) {
            for (int j = i + 1; j < 4; ++j) {
                if (srcPoints[i] == srcPoints[j]) {
                    throw std::invalid_argument("Input points must be distinct.");
                }
            }
        }

        cv::Mat disImg;
        vector<Point2f> poi4 = srcPoints;

#ifdef CropParallelRect_DEBUG 
        for (int i = 0; i < 4; ++i) {
            line(srcImg, srcPoints[i], poi4[i], CV_RGB(255, 0, 0), 2);
            Show_Img(srcImg);
        }
#endif

        if (size == Size(0, 0))
            size = Size(distance(srcPoints[0], srcPoints[1]), distance(srcPoints[1], srcPoints[3]));

        vector<Point2f> dis_points =
        {
            Point2f(0, 0),                              // 左上角像素 (0, 0)
            Point2f(size.width - 1, 0),                 // 右上角像素 (width-1, 0)
            Point2f(0, size.height - 1),                // 左下角像素 (0, height-1)
            Point2f(size.width - 1, size.height - 1)    // 右下角像素 (width-1, height-1），这些-1不可以省略，否则会导致变换后的图像边界出现黑边
        };

        auto M = getPerspectiveTransform(srcPoints, dis_points);
        warpPerspective(srcImg, disImg, M, size);

#ifdef CropParallelRect_DEBUG 
        Show_Img(disImg);
#endif 
        return disImg;
    }

    //增加验证输入合法性，取消了全局常量，可以在调用时直接改容差范围
         /*例如：bool result1 = isRightlAngle(89.5f);        使用默认容差 15.0f
                 bool result2 = isRightlAngle(89.5f, 5.0f);     使用自定义容差 5.0f（更严格）*/
    bool isRightlAngle(float angle, float tolerance = 15.0f)
    {
        // 验证输入角度的合法性
        if (angle < 0.0f || angle > 180.0f) {
            throw std::invalid_argument("Angle must be between 0 and 180 degrees.");
        }

        // 验证容差的合法性
        if (tolerance < 0.0f || tolerance > 90.0f) {
            throw std::invalid_argument("Tolerance must be between 0 and 90 degrees.");
        }

        return angle >= (90.0f - tolerance) && angle <= (90.0f + tolerance);
    }
    
    namespace QrcodeParse
    {
        struct ParseInfo
        {
            Point2f Center;          // 定位点的中心坐标
            int size;                // 轮廓的点数（代表形状复杂度）
            RotatedRect Rect;        // 最小外接旋转矩形

            // 构造函数：从轮廓点集初始化
            ParseInfo(const vector<Point>& pointSet) :
                Center(CalRectCenter(pointSet)),
                size(pointSet.size()),
                Rect(minAreaRect(pointSet)) {
            }

            // 默认构造
            ParseInfo() = default;
        };
    }

    //支持自定义比例范围
    bool IsQrTypeRateLegal(float rate, float minRate = 1.8f, float maxRate = 2.2f)
    {
        return rate >= minRate && rate <= maxRate;
    }

    //支持自适应容差
    bool isLegalDistanceRate(float rate, float expectedDistance,
        float minRelativeTolerance = 0.1f, float maxRelativeTolerance = 0.15f)
    {
        // 根据距离大小自适应调整容差
        float adaptiveTolerance = std::max(minRelativeTolerance,
            std::min(maxRelativeTolerance, 1.0f / expectedDistance));

        float minRate = 1.0f - adaptiveTolerance;
        float maxRate = 1.0f + adaptiveTolerance;

        return rate >= minRate && rate <= maxRate;
    }

    //之前实现采用"第一个匹配就停止"的策略，但实际上可能存在多个候选点都满足条件，所以评估所有候选点并选择最优
    bool FindForthPoint(vector<QrcodeParse::ParseInfo>& PointsInfo,
        float typeRatioMin = 1.8f, float typeRatioMax = 2.2f,
        float tolerance = 15.0f,
        float distExpectedDist = 100.0f)
    {
        if (PointsInfo.size() < 4) return 1;  // 不足4个点，无法搜索

        float avgSize = (PointsInfo[0].size + PointsInfo[1].size + PointsInfo[2].size) / 3.0f;
        Point2f forthPointPos = CalForthPoint(PointsInfo[0].Center, PointsInfo[1].Center, PointsInfo[2].Center);
        float expectDistance = distance(PointsInfo[0].Center, forthPointPos);

        QrcodeParse::ParseInfo bestPoint;
        float bestScore = -1.0f;
        bool found = false;

        // 评估所有候选点，选择评分最高的
        for (int i = 3; i < PointsInfo.size(); ++i) {
            // 检查三个条件
            float typeRatio = avgSize / PointsInfo[i].size;
            if (!IsQrTypeRateLegal(typeRatio, typeRatioMin, typeRatioMax)) continue;

            float angle = Cal3PointAngle(PointsInfo[i].Center, PointsInfo[1].Center, PointsInfo[2].Center);
            if (!isRightlAngle(angle, tolerance)) continue;

            float distRatio = distance(PointsInfo[i].Center, PointsInfo[0].Center) / expectDistance;
            if (!isLegalDistanceRate(distRatio, expectDistance, 0.05f, 0.15f)) continue;

            // 计算综合评分
            float typeScore = 1.0f - abs(typeRatio - 2.0f) / 0.2f;  // 理想值为 2.0
            float angleScore = 1.0f - abs(angle - 90.0f) / 15.0f;    // 理想值为 90
            float distScore = 1.0f - abs(distRatio - 1.0f) / 0.1f;   // 理想值为 1.0
            float totalScore = (typeScore * 0.3f + angleScore * 0.4f + distScore * 0.3f);   //343的比例

            if (totalScore > bestScore) {
                bestScore = totalScore;
                bestPoint = PointsInfo[i];
                found = true;
            }
        }

        if (!found) return 1;

        // 保留最优的候选点
        PointsInfo.erase(PointsInfo.begin() + 3, PointsInfo.end());
        PointsInfo.push_back(std::move(bestPoint));
        return 0;
    }

	//之前实现的权重系数和计算公式都是硬编码的,增加了调整配置结构体，允许自定义调整参数
    struct AdjustmentConfig {
        float lengthWeightFactor;    // 长度权重因子
        float secondOrderFactor;     // 二阶调整因子
        float forthCornerRatio;      // 第四角的调整比例
        bool useAdaptiveLength;      // 是否使用自适应长度
    };
    vector<Point2f> AdjustForthPoint(const vector<QrcodeParse::ParseInfo> PointsInfo,
        bool tag,
        const AdjustmentConfig& config = AdjustmentConfig{
            0.125f, 9.0f / 14.0f * std::sqrt(2.0f),
            11.0f / 18.0f, false
        })
    {
        float avglen = 0.0;
        vector<Point2f> ret;
        int id[4][3] = { { 0,1,2 },{1,0,3},{2,0,3},{3,1,2} };

        // 计算前三个点的贡献
        for (int i = 0; i < 3; ++i) {
            ret.push_back(PointsInfo[i].Center);
            avglen += PointsInfo[i].Rect.size.height;
            avglen += PointsInfo[i].Rect.size.width;
        }
        // 第四个点
        ret.push_back(PointsInfo[3].Center);
        avglen += PointsInfo[3].Rect.size.height * 2.0f;
        avglen += PointsInfo[3].Rect.size.width * 2.0f;
        // 归一化平均长度
        float divisor = tag ? 8.0f : 2.0f;
        avglen /= divisor;
        // 应用二阶调整因子
        if (tag) {
            avglen *= config.secondOrderFactor;
        }
        // 计算三个标准角的扩展向量
        pair<float, float> temp[4];
        for (int i = 0; i < 3; ++i) {
            temp[i] = CalExtendVec(ret[id[i][0]], ret[id[i][1]], ret[id[i][2]], avglen);
        }
        // 计算第四个角的扩展向量
        float forthCornerLen = avglen * (tag ? config.forthCornerRatio :
            ((56.0f - 3.5f * std::sqrt(2.0f)) / 56.0f));
        temp[3] = CalExtendVec(ret[id[3][0]], ret[id[3][1]], ret[id[3][2]], forthCornerLen);
        // 应用调整向量
        for (int i = 0; i < 4; ++i) {
            ret[i].x += temp[i].first;
            ret[i].y += temp[i].second;
        }
        return ret;
    }

    //利用 OpenCV 的优化函数，性能更高
    void GetVec(Mat& mat)
    {
        if (mat.empty() || mat.type() != CV_8UC3) {
            throw std::invalid_argument("Input mat must be a 3-channel BGR image.");
        }

        // 采样区域
        cv::Mat sampleRegion = mat(cv::Range(10, 100), cv::Range(10, 100));

        // 使用 OpenCV 的 min/max 函数
        cv::Mat minImg, maxImg;
        cv::Mat temp;

        vector<cv::Mat> channels(3);
        cv::split(sampleRegion, channels);

        uint16_t minVal[3], maxVal[3];
        for (int c = 0; c < 3; ++c) {
            double minValue, maxValue;
            cv::minMaxLoc(channels[c], &minValue, &maxValue);
            minVal[c] = static_cast<uint16_t>(minValue);
            maxVal[c] = static_cast<uint16_t>(maxValue);
        }

        // 计算阈值
        float avg = (minVal[0] + maxVal[0] + minVal[1] + maxVal[1] +
            minVal[2] + maxVal[2]) / 6.0f;

        // 转换为灰度
        cv::Mat grayMat;
        cv::cvtColor(mat, grayMat, cv::COLOR_BGR2GRAY);

        // 使用 threshold 函数进行二值化（比手动循环快）
        cv::threshold(grayMat, grayMat, avg, 255, cv::THRESH_BINARY);

        // 转换回三通道
        vector<cv::Mat> binaryChannels = { grayMat, grayMat, grayMat };
        cv::merge(binaryChannels, mat);
    }

	//DFS函数改为BFS实现，避免了栈溢出问题，加入迭代次数限制，防止无限循环
    struct SearchPoint {
        int x, y;
    };
    bool BfsWhiteRegion(int startI, int startJ, int limi, int limj,
        const int* dir, bool (*ispass)[16], const Mat& mat)
    {
        if (mat.empty() || mat.type() != CV_8UC3) {
            return false;
        }

        std::queue<SearchPoint> queue;
        queue.push({ startI, startJ });
        ispass[(startI - limi) * dir[0]][(startJ - limj) * dir[1]] = true;

        int processedCount = 0;
        const int MAX_ITERATIONS = 1000;  // 防止无限循环

        while (!queue.empty() && processedCount < MAX_ITERATIONS) {
            SearchPoint current = queue.front();
            queue.pop();
            processedCount++;

            // 检查四个方向
            int dRow[] = { 1, -1, 0, 0 };
            int dCol[] = { 0, 0, -1, 1 };

            for (int d = 0; d < 4; ++d) {
                int newI = current.x + dRow[d] * dir[0];
                int newJ = current.y + dCol[d] * dir[1];

                // 边界检查
                if ((limi - newI) * dir[0] > 0 || (limj - newJ) * dir[1] > 0) continue;
                if ((limi - newI) * dir[0] <= -16 || (limj - newJ) * dir[1] <= -16) continue;

                // 访问标记检查
                int idxI = (newI - limi) * dir[0];
                int idxJ = (newJ - limj) * dir[1];
                if (idxI < 0 || idxI >= 16 || idxJ < 0 || idxJ >= 16) continue;
                if (ispass[idxI][idxJ]) continue;

                // 检查像素值
                Vec3b pixel = mat.at<Vec3b>(newI, newJ);
                if (pixel[0] == 255 && pixel[1] == 255 && pixel[2] == 255) {
                    ispass[idxI][idxJ] = true;
                    queue.push({ newI, newJ });
                }
            }
        }

        return processedCount < MAX_ITERATIONS;
    }
    void dfs(int i, int j, int limi, int limj, int* dir, bool(*ispass)[16], const Mat& mat)
    {
        BfsWhiteRegion(i, j, limi, limj, dir, ispass, mat);
    }
        
    struct DistancePoint {
        int x, y;
        int distance;

        bool operator>(const DistancePoint& other) const {
            return distance > other.distance;
        }
    };

    vector<Point2f> FindConner(Mat& mat)
    {
        int dir[4][2] = { {1, 1}, {-1, 1}, {1, -1}, {-1, -1} };
        int poi[4][2] = { {0, 0}, {1079, 0}, {0, 1079}, {1079, 1079} };
        vector<Point2f> ret;

        for (int k = 0; k < 4; ++k) {
            bool ispass[16][16] = { 0 };

            // DFS 标记连通区域
            dfs(poi[k][0] + 10 * dir[k][0], poi[k][1] + 10 * dir[k][1],
                poi[k][0], poi[k][1], dir[k], ispass, mat);

            // 使用优先级队列，按曼哈顿距离排序
            std::priority_queue<DistancePoint, std::vector<DistancePoint>,
                std::greater<DistancePoint>> pq;

            // 初始化优先级队列
            for (int dis = 0; dis <= 15; ++dis) {
                for (int i = 0; i <= dis; ++i) {
                    int j = dis - i;
                    pq.push({ poi[k][0] + i * dir[k][0],
                            poi[k][1] + j * dir[k][1], dis });
                }
            }
            // 从距离最近的点开始搜索
            bool found = false;
            while (!pq.empty() && !found) {
                DistancePoint p = pq.top();
                pq.pop();

                if (p.x < 0 || p.x >= mat.rows || p.y < 0 || p.y >= mat.cols) continue;

                int i = (p.x - poi[k][0]) / (dir[k][0] != 0 ? dir[k][0] : 1);
                int j = (p.y - poi[k][1]) / (dir[k][1] != 0 ? dir[k][1] : 1);

                if (i < 0 || i >= 16 || j < 0 || j >= 16) continue;

                Vec3b pixel = mat.at<Vec3b>(p.x, p.y);
                if (pixel[0] == 255 && pixel[1] == 255 && pixel[2] == 255 && ispass[i][j]) {
                    ret.emplace_back(p.x, p.y);
                    found = true;
                }
            }

            if (!found) break;
        }
        if (ret.size() != 4) return ret;
        swap(ret[1].x, ret[2].y);
        swap(ret[2].x, ret[1].y);
        return ret;
    }
    
    //使用 OpenCV 内置函数优化性能
    void Resize(Mat& mat)
    {
        if (mat.empty() || mat.type() != CV_8UC3) {
            throw std::invalid_argument("Input mat must be a 3-channel BGR image.");
        }
        // 直接使用 OpenCV 的 resize 函数，性能最优
        Mat temp;
        cv::resize(mat, temp, cv::Size(108, 108), 0, 0, cv::INTER_NEAREST);

        // 确保完全二值化
        for (int i = 0; i < temp.rows; ++i) {
            for (int j = 0; j < temp.cols; ++j) {
                Vec3b& pixel = temp.at<Vec3b>(i, j);
                for (int c = 0; c < 3; ++c) {
                    pixel[c] = (pixel[c] > 127) ? 255 : 0;
                }
            }
        }
        mat = temp;
    }

    bool Main(const Mat& srcImg, Mat& disImg,
        const AdjustmentConfig& config = AdjustmentConfig{
            0.125f, 9.0f / 14.0f * std::sqrt(2.0f),
            11.0f / 18.0f, false
        })
    {
        Mat temp;

        // 以下为定位过程
        {
            //Show_Img(srcImg);
            vector<QrcodeParse::ParseInfo> PointsInfo;
            if (Main(srcImg, PointsInfo) || PointsInfo.size() < 4)
                return 1;

            if (FindForthPoint(PointsInfo)) return 1;

            // 一阶裁剪，完成初步筛选
            temp = CropParallelRect(srcImg, AdjustForthPoint(PointsInfo, false, config));
#ifdef FIND_QRPOINT_DEBUG
            Show_Img(temp);
#endif 
            disImg = CropParallelRect(srcImg, AdjustForthPoint(PointsInfo, true, config));
#ifdef FIND_QRPOINT_DEBUG
            Show_Img(disImg);
#endif 

            // 二阶裁剪，完成实际映射，消除二阶像差
            PointsInfo.clear();
            //return 0;
            if (Main(temp, PointsInfo) || PointsInfo.size() < 4)
                ;
            else
            {
                if (FindForthPoint(PointsInfo)) return 0;
                disImg = CropParallelRect(temp, AdjustForthPoint(PointsInfo, true, config));
            }
            //Show_Img(disImg);

            // 三阶微调，完成最终矫正
            disImg.copyTo(temp);
            cv::resize(temp, temp, Size(1080, 1080));
            GetVec(temp);
            auto poi4 = FindConner(temp);
            if (poi4.size() != 4) return 1;
            cv::resize(disImg, disImg, Size(1080, 1080));
            temp = CropParallelRect(disImg, poi4, Size(1080, 1080));
        }

        disImg = temp;
#ifdef FIND_QRPOINT_DEBUG
        Show_Img(disImg);
#endif 
        GetVec(disImg);
        Resize(disImg);
#ifdef FIND_QRPOINT_DEBUG
        Show_Img(disImg);
#endif 
        return 0;
    }

    void __DisPlay(const char* ImgPath)
    {
        Mat srcImg = imread(ImgPath, 1), disImg;
        if (Main(srcImg, disImg))
        {
            puts("ERR");
            return;
        }
        imshow("ans", disImg);
        waitKey(0);
    }
	
}