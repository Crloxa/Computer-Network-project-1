#pragma once
#include <cstdlib>
#include <cstring>
#include <cstdio>
namespace FFMPEG
{
	int VideotoImage(const char* videoPath,//输入视频文件的路径
		const char* imagePath,             //输出图片路径
		const char* imageFormat);          //图片文件后缀
	//返回值0成功 1出错

	int ImagetoVideo(const char* imagePath,//包含按序号命名图片的目录
		const char* imageFormat,           //后缀格式
		const char* videoPath,             //输出视频文件路径
		unsigned rawFrameRates = 30,       //输入图片序列的帧率
		unsigned outputFrameRates = 30,    //输出视频的帧率
		unsigned kbps = 0);                //֡输出视频码率默认其0
	//通过文件系统（目录和文件）在模块间传递数据
}

