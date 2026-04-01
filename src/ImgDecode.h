#pragma once

#include <opencv2/opencv.hpp>

#define Show_Img(src) do\
{\
	cv::imshow("src", src);\
	cv::waitKey();\
}while (0);

namespace ImageDecode
{
	using namespace std;
	using namespace cv;

	struct ImageInfo
	{
		vector<unsigned char> Info;
		uint16_t CheckCode;
		uint16_t FrameBase;
		bool IsStart;
		bool IsEnd;
	};

	constexpr int BytesPerFrame = 7613;
	constexpr int FrameSize = 266;
	constexpr int FrameOutputRate = 10;
	constexpr int SafeAreaWidth = 4;
	constexpr int QrPointSize = 42;
	constexpr int SmallQrPointbias = 14;

	bool Main(Mat& mat, ImageInfo& imageInfo);
}
