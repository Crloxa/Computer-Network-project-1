#pragma once
#include<opencv2/opencv.hpp>
#include<cstdio>

namespace ImgParse {			//命名空间，包含了所有图像处理相关的函数
	
	using namespace cv;
	using namespace std;
	
	namespace QrcodeParse {		//子命名空间，包含了所有二维码解析相关的函数和结构体
		struct ParseInfo;


	}
	Mat Rotation_90(const Mat& srcImg);		////90度旋转

	Point2f CalRectCenter(const vector<Point>& contours);		////计算轮廓中心

	bool IsClockWise(const Point& basePoint, const Point& point1, const Point& point2);			//判断三点是否按顺时针排列

	float Cal3PointAngle(const Point& point0, const Point& point1, const Point& point2);		//计算三点夹角

	Mat CropRect(const Mat& srcImg, const RotatedRect& rotatedRect);		//根据旋转矩形裁剪图像

	float distance(const Point2f& a, const Point2f& b);		//计算两点距离

	Point CalForthPoint(const Point& poi0, const Point& poi1, const Point& poi2);		//计算第四个点

	pair<float, float> CalExtendVec(const Point2f& poi0, const Point2f& poi1, const Point2f& poi2, float bias);		//计算外角平分向量

	Mat CropParallelRect(const Mat& srcImg, const vector<Point2f>& srcPoints, Size size);		//四边形透视变换

	bool isRightlAngle(float angle);//判断角度是否为直角

	bool IsQrTypeRateLegal(float rate);//验证二维码类型比例是否合法

	bool isLegalDistanceRate(float rate);//验证距离比例是否合法

	bool FindForthPoint(vector<QrcodeParse::ParseInfo>& PointsInfo,
		float typeRatioMin = 1.8f, float typeRatioMax = 2.2f,
		float tolerance = 15.0f,
		float distExpectedDist = 100.0f);//查找/计算第四个点信息

	vector<Point2f> AdjustForthPoint(const vector<QrcodeParse::ParseInfo> PointsInfo,
		bool tag,
		const AdjustmentConfig& config = AdjustmentConfig{
			0.125f, 9.0f / 14.0f * std::sqrt(2.0f),
			11.0f / 18.0f, false
		});//根据 QR 码四个定位角的几何信息，计算出这四个角的外延向量

	void GetVec(Mat& mat);		//对图像进行自适应二值化处理，将其转换为纯黑白图像

	void dfs(int i, int j, int limi, int limj, int* dir, bool(*ispass)[16], const Mat& mat);  //在图像中找到连通的白色像素区域

	vector<Point2f> FindConner(Mat& mat);	//从图像的四个角落出发，沿特定方向搜索找到白色区域的边界点
	
	void Resize(Mat& mat);		//缩放到108x108

	bool Main(const Mat& srcImg, Mat& disImg);//主函数：完整的二维码检测流程

	void __DisPlay(const char* ImgPath);//调试显示函数

}
