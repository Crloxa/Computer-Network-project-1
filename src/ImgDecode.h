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

	constexpr int BytesPerFrame = 1878;
	constexpr int FrameSize = 133;
	constexpr int FrameOutputRate = 10;
	constexpr int SafeAreaWidth = 2;
	constexpr int QrPointSize = 21;
	constexpr int SmallQrPointbias = 7;

	bool Main(Mat& mat, ImageInfo& imageInfo);
}
