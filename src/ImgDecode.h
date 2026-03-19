#pragma once

#include <opencv2/opencv.hpp>
#include "frame_constants.h"

namespace ImageDecode
{
	using namespace std;
	using namespace cv;

	// 协议几何常量由共享头文件统一提供，此处重新导出以保持 ImageDecode:: 访问路径兼容
	using FrameLayout::BytesPerFrame;
	using FrameLayout::FrameSize;
	using FrameLayout::FrameOutputRate;
	using FrameLayout::SafeAreaWidth;
	using FrameLayout::QrPointSize;
	using FrameLayout::SmallQrPointbias;

	struct ImageInfo
	{
		vector<unsigned char> Info;
		uint16_t CheckCode;
		uint16_t FrameBase;
		bool IsStart;
		bool IsEnd;
	};

	bool Main(Mat& mat, ImageInfo& imageInfo);
}
