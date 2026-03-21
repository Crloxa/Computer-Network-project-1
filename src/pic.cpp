#include"pic.h"

// 开启下面两个宏可以开启调试
//#define FIND_QRPOINT_DEBUG 1
//#define CropParallelRect_DEBUG 1

#define Show_Img(src) do\
{\
	imshow("DEBUG", src);\
	waitKey();\
}while (0);

namespace ImgParse
{
	constexpr float MinRightAngel = 75.0, MaxRightAngel = 105.0;
	constexpr float MaxQrTypeRate = 2.2, minQrTypeRate = 1.8;
	constexpr float MaxDistanceRate = 1.1, minDistanceRate = 0.9;

	constexpr int LogicalFrameSize = 133;
	constexpr int FrameOutputRate = 10;
	constexpr int OutputFrameSize = LogicalFrameSize * FrameOutputRate; // 1330
	constexpr int CornerSearchSize = 16;

	Mat Rotation_90(const Mat& srcImg)
	{
		Mat tempImg;
		transpose(srcImg, tempImg);
		flip(tempImg, tempImg, 1);
		return tempImg;
	}

	Point2f CalRectCenter(const vector<Point>& contours)
	{
		float centerx = 0, centery = 0;
		int n = contours.size();
		centerx = (contours[n / 4].x + contours[n * 2 / 4].x + contours[3 * n / 4].x + contours[n - 1].x) / 4;
		centery = (contours[n / 4].y + contours[n * 2 / 4].y + contours[3 * n / 4].y + contours[n - 1].y) / 4;
		return Point2f(centerx, centery);
	}

	bool IsClockWise(const Point& basePoint, const Point& point1, const Point& point2)
	{
		float ax = point1.x - basePoint.x, ay = point1.y - basePoint.y;
		float bx = point2.x - basePoint.x, by = point2.y - basePoint.y;
		return (ax * by - bx * ay) > 0;
	}

	float Cal3PointAngle(const Point& point0, const Point& point1, const Point& point2)
	{
		float dx1 = point1.x - point0.x, dy1 = point1.y - point0.y;
		float dx2 = point2.x - point0.x, dy2 = point2.y - point0.y;
		return acos((dx1 * dx2 + dy1 * dy2) / sqrt((dx1 * dx1 + dy1 * dy1) * (dx2 * dx2 + dy2 * dy2) + 1e-10f)) * 180.0f / 3.141592653f;
	}

	Mat CropRect(const Mat& srcImg, const RotatedRect& rotatedRect)
	{
		cv::Mat srcPoints, disImg;
		boxPoints(rotatedRect, srcPoints);
		vector<Point2f> dis_points =
		{
			Point2f(0,rotatedRect.size.height - 1),
			Point2f(0,0),
			Point2f(rotatedRect.size.width - 1,0),
			Point2f(rotatedRect.size.width - 1,rotatedRect.size.height - 1)
		};
		auto M = getPerspectiveTransform(srcPoints, dis_points);
		warpPerspective(srcImg, disImg, M, rotatedRect.size);
		return disImg;
	}

	float distance(const Point2f& a, const Point2f& b)
	{
		return sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
	}

	Point CalForthPoint(const Point& poi0, const Point& poi1, const Point& poi2)
	{
		return Point(poi2.x + poi1.x - poi0.x, poi2.y + poi1.y - poi0.y);
	}

	pair<float, float> CalExtendVec(const Point2f& poi0, const Point2f& poi1, const Point2f& poi2, float bias)
	{
		float dis0 = distance(poi0, poi1), dis1 = distance(poi0, poi2);
		float rate = dis1 / dis0;
		float x1 = poi0.x - poi2.x, y1 = poi0.y - poi2.y;
		float x2 = (poi0.x - poi1.x) * rate, y2 = (poi0.y - poi1.y) * rate;
		float totx = x1 + x2, toty = y1 + y2, distot = sqrt(totx * totx + toty * toty);
		if (distot < 1e-5) return { 0, 0 }; // 防止除 0 崩溃//
		//
		return { totx / distot * bias, toty / distot * bias };
	}

	Mat CropParallelRect(const Mat& srcImg, const vector<Point2f>& srcPoints, Size size = { 0,0 })
	{
		cv::Mat disImg;
		if (size == Size(0, 0))
			size = Size(distance(srcPoints[0], srcPoints[1]), distance(srcPoints[1], srcPoints[3]));

		// 防御：如果 size 极其离谱，直接返回空图
		//
		if (size.width <= 0 || size.height <= 0 || size.width > 10000 || size.height > 10000) {
			return disImg;
		}

		vector<Point2f> dis_points =
		{
			Point2f(0,0),
			Point2f(size.width - 1,0),
			Point2f(0,size.height - 1),
			Point2f(size.width - 1,size.height - 1)
		};
		auto M = getPerspectiveTransform(srcPoints, dis_points);
		warpPerspective(srcImg, disImg, M, size);
		return disImg;
	}

	bool isRightlAngle(float angle)
	{
		return MinRightAngel <= angle && MaxRightAngel >= angle;
	}

	namespace QrcodeParse
	{
		struct ParseInfo
		{
			Point2f Center;
			int size;
			RotatedRect Rect;
			ParseInfo(const vector<Point>& pointSet) :
				Center(CalRectCenter(pointSet)),
				size(pointSet.size()),
				Rect(minAreaRect(pointSet)) {
			}
			ParseInfo() = default;
		};

		constexpr float MaxQRBWRate = 2.25, MinQRBWRate = 0.40;
		constexpr int MinQRSize = 10;
		constexpr float MaxQRScale = 0.25, MinQRXYRate = 5.0 / 6.0, MaxQRXYRate = 6.0 / 5.0;

		double Cal3NumVariance(const int a, const int b, const int c)
		{
			double avg = (a + b + c) / 3.0;
			return (a - avg) * (a - avg) + (b - avg) * (b - avg) + (c - avg) * (c - avg);
		}

		bool IsQrBWRateLegal(const float rate)
		{
			return rate < MaxQRBWRate && rate > MinQRBWRate;
		}

		bool BWRatePreprocessing(Mat& image, vector<int>& vValueCount)
		{
			int count = 0, nc = image.cols * image.channels(), nr = image.rows / 2;
			uchar lastColor = 0, * data = image.ptr<uchar>(nr);
			for (int i = 0; i < nc; i++)
			{
				uchar color = data[i];
				if (color > 0)
					color = 255;
				if (i == 0)
				{
					lastColor = color;
					count++;
				}
				else
				{
					if (lastColor != color)
					{
						vValueCount.push_back(count);
						count = 0;
					}
					count++;
					lastColor = color;
				}
			}
			if (count) vValueCount.push_back(count);
			return vValueCount.size() >= 5;
		}

		bool IsQrBWRateXLabel(Mat& image)
		{
			vector<int> vValueCount;
			if (!BWRatePreprocessing(image, vValueCount))
				return false;

			int index = -1, maxCount = -1;
			for (int i = 0; i < vValueCount.size(); i++)
			{
				if (i == 0)
				{
					index = i;
					maxCount = vValueCount[i];
				}
				else if (vValueCount[i] > maxCount)
				{
					index = i;
					maxCount = vValueCount[i];
				}
			}

			if (index < 2 || (vValueCount.size() - index) < 3)
				return false;

			float rate = ((float)maxCount) / 3.00;
			bool checkTag = 1;
			for (int i = -2; i < 3; ++i)
			{
				float rateNow = vValueCount[index + i] / rate;
				if (i) checkTag &= IsQrBWRateLegal(rateNow);
			}
			return checkTag;
		}

		bool IsQrBWRate(Mat& image)
		{
			bool xTest = IsQrBWRateXLabel(image);
			if (!xTest) return false;
			Mat image_rotation_90 = Rotation_90(image);
			bool yTest = IsQrBWRateXLabel(image_rotation_90);
			return yTest;
		}

		bool IsQrSizeLegal(const Size2f& qrSize, const Size2f&& imgSize)
		{
			float xYScale = qrSize.height / qrSize.width;
			if (qrSize.height < MinQRSize || qrSize.width < MinQRSize)
				return false;
			if (qrSize.height / imgSize.height >= MaxQRScale || qrSize.width / imgSize.width >= MaxQRScale)
				return false;
			if (xYScale < MinQRXYRate || xYScale > MaxQRXYRate)
				return false;
			return true;
		}

		bool IsQrPoint(const vector<Point>& contour, const Mat& img)
		{
			RotatedRect rotatedRect = minAreaRect(contour);
			cv::Mat cropImg = CropRect(img, rotatedRect);
			if (!IsQrSizeLegal(rotatedRect.size, img.size())) return false;
			return IsQrBWRate(cropImg);
		}

		Mat ImgPreprocessing(const Mat& srcImg, float blurRate = 0.0005)
		{
			Mat tmpImg;
			// 外层防线：绝对确保传入 cvtColor 的图片不为空
			//
			if (srcImg.empty()) return tmpImg;

			cvtColor(srcImg, tmpImg, COLOR_BGR2GRAY);
			float BlurSize = 1.0 + srcImg.rows * blurRate;
			blur(tmpImg, tmpImg, Size2f(BlurSize, BlurSize));
			threshold(tmpImg, tmpImg, 0, 255, THRESH_BINARY | THRESH_OTSU);
			return tmpImg;
		}

		bool ScreenQrPoint(const Mat& srcImg, vector<vector<Point>>& qrPoints)
		{
			vector<vector<Point> > contours;
			vector<Vec4i> hierarchy;

			// 防空图崩
			//
			if (srcImg.empty()) return true;

			findContours(srcImg, contours, hierarchy, RETR_TREE, CHAIN_APPROX_NONE, Point(0, 0));

			int parentIdx = -1;
			int ic = 0;

			for (int i = 0; i < contours.size(); i++)
			{
				if (hierarchy[i][2] != -1 && ic == 0)
				{
					parentIdx = i;
					ic++;
				}
				else if (hierarchy[i][2] != -1)
				{
					ic++;
				}
				else if (hierarchy[i][2] == -1)
				{
					ic = 0;
					parentIdx = -1;
				}
				if (ic == 2)
				{
					bool isQrPoint = IsQrPoint(contours[parentIdx], srcImg);
					if (isQrPoint)
						qrPoints.push_back(contours[parentIdx]);
					ic = 0;
					parentIdx = -1;
				}
			}
			return qrPoints.size() < 3;
		}

		bool isRightAngleExist(const Point& point0, const Point& point1, const Point& point2)
		{
			return isRightlAngle(Cal3PointAngle(point0, point1, point2)) ||
				isRightlAngle(Cal3PointAngle(point1, point0, point2)) ||
				isRightlAngle(Cal3PointAngle(point2, point0, point1));
		}

		bool DumpExcessQrPoint(vector<vector<Point>>& qrPoints)
		{
			sort(
				qrPoints.begin(), qrPoints.end(),
				[](const vector<Point>& a, const vector<Point>& b) {return a.size() < b.size(); }
			);
			double mindis = INFINITY;
			int pos = -1;
			Point Point0 = CalRectCenter(qrPoints[0]), Point1 = CalRectCenter(qrPoints[1]), Point2;
			for (int i = 2; i < qrPoints.size(); ++i)
			{
				bool tag = 0;
				if (!isRightAngleExist(Point2 = CalRectCenter(qrPoints[i]), Point1, Point0))
					tag = 1;
				if (!tag)
				{
					auto temp = Cal3NumVariance(qrPoints[i].size(), qrPoints[i - 1].size(), qrPoints[i - 2].size());
					if (mindis > temp)
					{
						mindis = temp;
						pos = i;
					}
				}
				Point0 = Point1;
				Point1 = Point2;
			}
			if (pos == -1) return 1;
			else
			{
				vector<vector<Point>> temp =
				{
					std::move(qrPoints[pos - 2]),
					std::move(qrPoints[pos - 1]),
					std::move(qrPoints[pos])
				};
				for (int i = 0; i < pos - 2; ++i)
					temp.push_back(std::move(qrPoints[i]));
				for (int i = pos + 1; i < qrPoints.size(); ++i)
					temp.push_back(std::move(qrPoints[i]));
				std::swap(temp, qrPoints);
				return 0;
			}
		}

		void AdjustPointsOrder(vector<vector<Point>>& src3Points)
		{
			vector<vector<Point>> temp;
			Point p3[3] = { CalRectCenter(src3Points[0]),CalRectCenter(src3Points[1]),CalRectCenter(src3Points[2]) };
			int index[3][3] = { { 0,1,2 },{1,0,2},{2,0,1} };
			for (int i = 0; i < 3; i++)
			{
				if (isRightlAngle(Cal3PointAngle(p3[index[i][0]], p3[index[i][1]], p3[index[i][2]])))
				{
					temp.push_back(std::move(src3Points[index[i][0]]));
					if (IsClockWise(p3[index[i][0]], p3[index[i][1]], p3[index[i][2]]))
					{
						temp.push_back(std::move(src3Points[index[i][1]]));
						temp.push_back(std::move(src3Points[index[i][2]]));
					}
					else
					{
						temp.push_back(std::move(src3Points[index[i][2]]));
						temp.push_back(std::move(src3Points[index[i][1]]));
					}
					for (int j = 3; j < src3Points.size(); ++j)
						temp.push_back(std::move(src3Points[j]));
					std::swap(temp, src3Points);
					return;
				}
			}
			return;
		}

		bool Main(const Mat& srcImg, vector<vector<Point>>& qrPoints)
		{
			if (srcImg.empty()) return 1;
			vector<vector<Point>> qrPointsTemp;
			std::array<float, 5> ar = { 0.0005,0.0000,0.00025,0.001,0.0001 };
			for (auto& rate : ar)
			{
				qrPointsTemp.clear();
				Mat preprocessed = ImgPreprocessing(srcImg, rate);
				if (preprocessed.empty()) continue;

				if (!ScreenQrPoint(preprocessed, qrPointsTemp))
				{
					if (qrPointsTemp.size() >= 3 && !DumpExcessQrPoint(qrPointsTemp))
					{
						qrPointsTemp.swap(qrPoints);
						AdjustPointsOrder(qrPoints);
						return 0;
					}
				}
			}
			return 1;
		}

		bool Main(const Mat& srcImg, vector<ParseInfo>& Points3Info)
		{
			vector<vector<Point>> qrPoints;
			if (Main(srcImg, qrPoints)) return 1;
			for (auto& e : qrPoints)
				Points3Info.emplace_back(e);
			return 0;
		}
	}

	bool IsQrTypeRateLegal(float rate)
	{
		return rate <= MaxQrTypeRate && rate >= minQrTypeRate;
	}

	bool isLegalDistanceRate(float rate)
	{
		return rate <= MaxDistanceRate && rate >= minDistanceRate;
	}

	bool FindForthPoint(vector<QrcodeParse::ParseInfo>& PointsInfo)
	{
		float avgSize = (PointsInfo[0].size + PointsInfo[1].size + PointsInfo[2].size) / 3.0;
		float expectDistance = distance(PointsInfo[0].Center, CalForthPoint(PointsInfo[0].Center, PointsInfo[1].Center, PointsInfo[2].Center));
		QrcodeParse::ParseInfo possiblePoints;
		bool tag = 1;
		for (int i = 3; i < PointsInfo.size(); ++i)
		{
			if (IsQrTypeRateLegal(avgSize / PointsInfo[i].size))
				if (isRightlAngle(Cal3PointAngle(PointsInfo[i].Center, PointsInfo[1].Center, PointsInfo[2].Center)))
					if (isLegalDistanceRate(distance(PointsInfo[i].Center, PointsInfo[0].Center) / expectDistance))
					{
						possiblePoints = std::move(PointsInfo[i]);
						tag = 0;
						break;
					}
		}

		PointsInfo.erase(PointsInfo.begin() + 3, PointsInfo.end());

		if (tag)
		{
			possiblePoints.Center = CalForthPoint(PointsInfo[0].Center, PointsInfo[1].Center, PointsInfo[2].Center);
			possiblePoints.size = avgSize / 2.0;
			possiblePoints.Rect.center = possiblePoints.Center;
			possiblePoints.Rect.size = Size2f(PointsInfo[0].Rect.size.width / 2.0, PointsInfo[0].Rect.size.height / 2.0);
		}

		PointsInfo.push_back(std::move(possiblePoints));
		return 0;
	}

	vector<Point2f> AdjustForthPoint(const vector<QrcodeParse::ParseInfo> PointsInfo, bool tag)
	{
		float avglen = 0.0;
		vector<Point2f> ret;
		int id[4][3] = { { 0,1,2 },{1,0,3},{2,0,3},{3,1,2} };
		for (int i = 0; i < 3; ++i)
		{
			ret.push_back(PointsInfo[i].Center);
			avglen += PointsInfo[i].Rect.size.height;
			avglen += PointsInfo[i].Rect.size.width;
		}
		ret.push_back(PointsInfo[3].Center);
		avglen += PointsInfo[3].Rect.size.height * 2.0;
		avglen += PointsInfo[3].Rect.size.width * 2.0;
		avglen /= (tag) ? 8.0 : 2.0;
		if (tag) avglen = avglen / 14.0 * 9 * sqrt(2);
		pair<float, float> temp[4];
		for (int i = 0; i < 3; ++i)
			temp[i] = CalExtendVec(ret[id[i][0]], ret[id[i][1]], ret[id[i][2]], avglen);
		float forthCornerLen = avglen * ((tag) ? (11.0 / 18.0) : ((56.0 - 3.5 * sqrt(2.0)) / 56.0));
		temp[3] = CalExtendVec(ret[id[3][0]], ret[id[3][1]], ret[id[3][2]], forthCornerLen);
		for (int i = 0; i < 4; ++i)
		{
			ret[i].x += temp[i].first;
			ret[i].y += temp[i].second;
		}
		return ret;
	}

	void GetVec(Mat& mat)
	{
		if (mat.empty()) return;

		uint16_t minVec[3] = { 255,255,255 }, maxVec[3] = { 0, 0, 0 };
		int margin = FrameOutputRate;

		if (mat.rows <= margin * 2 || mat.cols <= margin * 2) return;

		for (int i = margin; i < mat.rows - margin; ++i)
		{
			for (int j = margin; j < mat.cols - margin; ++j)
			{
				Vec3b& temp = mat.at<Vec3b>(i, j);
				minVec[0] = min(minVec[0], (uint16_t)temp[0]);
				minVec[1] = min(minVec[1], (uint16_t)temp[1]);
				minVec[2] = min(minVec[2], (uint16_t)temp[2]);
				maxVec[0] = max(maxVec[0], (uint16_t)temp[0]);
				maxVec[1] = max(maxVec[1], (uint16_t)temp[1]);
				maxVec[2] = max(maxVec[2], (uint16_t)temp[2]);
			}
		}
		float avg = (minVec[0] + maxVec[0] + minVec[1] + maxVec[1] + minVec[2] + maxVec[2]) / 6.0;
		for (int i = 0; i < mat.rows; ++i)
		{
			for (int j = 0; j < mat.cols; ++j)
			{
				Vec3b& temp = mat.at<Vec3b>(i, j);
				float sum = (temp[0] + temp[1] + temp[2]) / 3.0;
				if (sum < avg) temp = Vec3b(0, 0, 0);
				else temp = Vec3b(255, 255, 255);
			}
		}
	}

	void dfs(int i, int j, int limi, int limj, int* dir, bool(*ispass)[CornerSearchSize], const Mat& mat)
	{
		if (i < 0 || i >= mat.rows || j < 0 || j >= mat.cols) return; // 边界保护//
		//
		if ((limi - i) * dir[0] > 0 || (limj - j) * dir[1] > 0) return;
		if ((limi - i) * dir[0] <= -CornerSearchSize || (limj - j) * dir[1] <= -CornerSearchSize) return;
		if (ispass[(i - limi) * dir[0]][(j - limj) * dir[1]]) return;
		auto temp = mat.at<Vec3b>(i, j);
		if (temp[0] == temp[1] && temp[1] == temp[2] && temp[2] == 255)
		{
			ispass[(i - limi) * dir[0]][(j - limj) * dir[1]] = 1;
			dfs(i + 1 * dir[0], j, limi, limj, dir, ispass, mat);
			dfs(i - 1 * dir[0], j, limi, limj, dir, ispass, mat);
			dfs(i, j - 1 * dir[0], limi, limj, dir, ispass, mat);
			dfs(i, j + 1 * dir[0], limi, limj, dir, ispass, mat);
		}
	}

	vector<Point2f> FindConner(Mat& mat)
	{
		vector<Point2f> ret;
		if (mat.empty() || mat.rows == 0 || mat.cols == 0) return ret;

		int dis = 0;
		int dir[4][2] = { {1,1},{-1,1},{1,-1},{-1,-1} };
		int poi[4][2] = { {0,0},{mat.rows - 1,0},{0,mat.cols - 1},{mat.rows - 1,mat.cols - 1} };

		for (int k = 0; k < 4; ++k)
		{
			bool ispass[CornerSearchSize][CornerSearchSize] = { 0 };
			dfs(poi[k][0] + FrameOutputRate * dir[k][0], poi[k][1] + FrameOutputRate * dir[k][1], poi[k][0], poi[k][1], dir[k], ispass, mat);
			for (dis = 0; dis < CornerSearchSize; ++dis)
			{
				for (int i = 0; i <= dis; ++i)
				{
					int j = dis - i;
					int r = poi[k][0] + i * dir[k][0];
					int c = poi[k][1] + j * dir[k][1];
					if (r >= 0 && r < mat.rows && c >= 0 && c < mat.cols) { // 边界保护
						//
						auto temp = mat.at<Vec3b>(r, c);
						if (temp[0] == temp[1] && temp[1] == temp[2] && temp[2] == 255 && ispass[i][j])
						{
							ret.emplace_back(r, c);
							goto Final;
						}
					}
				}
			}
		Final:
			if (dis == CornerSearchSize)
				break;
		}
		if (ret.size() != 4) {
			ret.clear();
			return ret;
		}
		std::swap(ret[1].x, ret[2].y);
		std::swap(ret[2].x, ret[1].y);
		return ret;
	}

	void Resize(Mat& mat)
	{
		if (mat.empty() || mat.rows < LogicalFrameSize || mat.cols < LogicalFrameSize) return;

		Mat temp = Mat(LogicalFrameSize, LogicalFrameSize, CV_8UC3);
		for (int i = 0; i < LogicalFrameSize; ++i)
		{
			for (int j = 0; j < LogicalFrameSize; ++j)
			{
				int counter[8] = { 0 };
				for (int k = 4; k <= 5; ++k)
				{
					for (int l = 4; l <= 5; ++l)
					{
						int r = i * FrameOutputRate + k;
						int c = j * FrameOutputRate + l;
						if (r < mat.rows && c < mat.cols) {
							int id = 0;
							const auto& vec = mat.at<Vec3b>(r, c);
							if (vec[0] == 255) id += 4;
							if (vec[1] == 255) id += 2;
							if (vec[2] == 255) id += 1;
							++counter[id];
						}
					}
				}
				int maxpos = 0;
				for (int m = 1; m < 8; ++m)
					if (counter[maxpos] < counter[m])
						maxpos = m;
				temp.at<Vec3b>(i, j) = Vec3b((maxpos >> 2) * 255, ((maxpos >> 1) & 1) * 255, (maxpos & 1) * 255);
			}
		}
		mat = temp;
	}

	bool Main(const Mat& srcImg, Mat& disImg)
	{
		if (srcImg.empty()) return 1;

		Mat temp;
		vector<QrcodeParse::ParseInfo> PointsInfo;

		// 1. 获取角点
		//
		if (QrcodeParse::Main(srcImg, PointsInfo) || PointsInfo.size() < 3)
			return 1;

		if (FindForthPoint(PointsInfo)) return 1;

		// 一阶裁剪
		//
		temp = CropParallelRect(srcImg, AdjustForthPoint(PointsInfo, 0));
		if (temp.empty()) return 1;
		// 防御：一阶裁剪失败，立刻截断，丢给外层兜底
		//

		disImg = CropParallelRect(srcImg, AdjustForthPoint(PointsInfo, 1));
		if (disImg.empty()) return 1;

		// 2. 二阶裁剪
		//
		PointsInfo.clear();
		if (QrcodeParse::Main(temp, PointsInfo) || PointsInfo.size() < 3) {
			// 二阶找不到角点，我们直接拿着一阶的 disImg 交差，不要往下跑导致崩溃
			//
			GetVec(disImg);
			Resize(disImg);
			return 0;
		}

		if (!FindForthPoint(PointsInfo)) {
			Mat tempDis = CropParallelRect(temp, AdjustForthPoint(PointsInfo, 1));
			if (!tempDis.empty()) disImg = tempDis;
		}

		// 3. 三阶微调
		//
		disImg.copyTo(temp);
		cv::resize(temp, temp, Size(OutputFrameSize, OutputFrameSize));
		GetVec(temp);
		auto poi4 = FindConner(temp);

		if (poi4.size() == 4) {
			cv::resize(disImg, disImg, Size(OutputFrameSize, OutputFrameSize));
			Mat finalTemp = CropParallelRect(disImg, poi4, Size(OutputFrameSize, OutputFrameSize));
			if (!finalTemp.empty()) disImg = finalTemp;
		}

		// 4. 重采样
		//
		GetVec(disImg);
		Resize(disImg);

		return 0;
	}
}