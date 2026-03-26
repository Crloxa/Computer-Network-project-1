#pragma once

#include <opencv2/opencv.hpp>

#include "protocol.h"

#ifndef Show_Img
#define Show_Img(src) do\
{\
	cv::imshow("src", src);\
	cv::waitKey();\
}while (0);
#endif

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

	constexpr int BytesPerFrame = Protocol::BytesPerFrame;
	constexpr int FrameSize = Protocol::FrameSize;
	constexpr int FrameOutputRate = Protocol::FrameOutputRate;
	constexpr int SafeAreaWidth = Protocol::SafeAreaWidth;
	constexpr int QrPointSize = Protocol::QrPointSize;
	constexpr int SmallQrPointbias = Protocol::SmallQrPointBias;

	bool Main(Mat& mat, ImageInfo& imageInfo);
}
