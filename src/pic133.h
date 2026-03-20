#pragma once
#include<opencv2/opencv.hpp>
#include<cstdio>

namespace ImgParse133
{
	using namespace cv;

	using namespace std;

	namespace QrcodeParse133
	{

		struct ParseInfo;

		double Cal3NumVariance(const int a, const int b, const int c);

		bool IsQrBWRateLegal(const float rate);

		bool BWRatePreprocessing(Mat& image, vector<int>& vValueCount);

		bool IsQrBWRateXLabel(Mat& image);

		bool IsQrBWRate(Mat& image);

		bool IsQrSizeLegal(const Size2f& qrSize, const Size2f&& imgSize);

		bool IsQrPoint(const vector<Point>& contour, const Mat& img);

		Mat ImgPreprocessing(const Mat& srcImg, float blurRate);

		bool ScreenQrPoint(const Mat& srcImg, vector<vector<Point>>& qrPoints);

		bool isRightAngleExist(const Point& point0, const Point& point1, const Point& point2);

		bool DumpExcessQrPoint(vector<vector<Point>>& qrPoints);

		void AdjustPointsOrder(vector<vector<Point>>& src3Points);

		bool Main(const Mat& srcImg, vector<vector<Point>>& qrPoints);

		bool Main(const Mat& srcImg, Mat& disImg, vector<ParseInfo>& Points3Info);

	}
	Mat Rotation_90(const Mat& srcImg);

	Point2f CalRectCenter(const vector<Point>& contours);

	bool IsClockWise(const Point& basePoint, const Point& point1, const Point& point2);

	float Cal3PointAngle(const Point& point0, const Point& point1, const Point& point2);

	Mat CropRect(const Mat& srcImg, const RotatedRect& rotatedRect);

	float distance(const Point2f& a, const Point2f& b);

	Point CalForthPoint(const Point& poi0, const Point& poi1, const Point& poi2);

	pair<float, float> CalExtendVec(const Point2f& poi0, const Point2f& poi1, const Point2f& poi2, float bias);

	Mat CropParallelRect(const Mat& srcImg, const vector<Point2f>& srcPoints, Size size);

	bool isRightlAngle(float angle);

	bool IsQrTypeRateLegal(float rate);

	bool isLegalDistanceRate(float rate);

	bool FindForthPoint(vector<QrcodeParse133::ParseInfo>& PointsInfo);

	bool Main(const Mat& srcImg, Mat& disImg);

	void Resize(Mat& mat);

	vector<Point2f> FindConner(Mat& mat);

	void dfs(int i, int j, int limi, int limj, int* dir, bool(*ispass)[16], const Mat& mat);

}