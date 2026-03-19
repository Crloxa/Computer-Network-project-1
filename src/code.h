#pragma once
#include "frame_constants.h"
#include<opencv2/opencv.hpp>

#include<cstdio>

namespace Code
{
	using namespace cv;
	using namespace std;
	uint16_t CalCheckCode(const unsigned char* info, int len, bool isStart, bool isEnd, uint16_t frameBase);
	void BulidSafeArea(Mat& mat);
	void BulidQrPoint(Mat& mat);
	void BulidCheckCodeAndFrameNo(Mat& mat, uint16_t checkcode, uint16_t FrameNo);
	void BulidFrameFlag(Mat& mat, FrameLayout::FrameType frameType, int tailLen);
	Mat CodeFrame(FrameLayout::FrameType frameType, const char* info, int tailLen, int FrameNo);
	void Main(const char* info, int len,const char* savePath, const char* outputFormat, int FrameCountLimit = INT_MAX);
}
