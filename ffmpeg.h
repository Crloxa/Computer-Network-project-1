#pragma once
#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace FFMPEG
{
	// 保留现有对外接口
	int VideotoImage(const char* videoPath,//输入视频文件的路径
		const char* imagePath,             //输出图片路径
		const char* imageFormat);          //图片文件后缀

	int ImagetoVideo(const char* imagePath,//包含按序号命名图片的目录
		const char* imageFormat,           //后缀格式
		const char* videoPath,             //输出视频文件路径
		unsigned rawFrameRates = 30,       //输入图片序列的帧率
		unsigned outputFrameRates = 30,    //输出视频的帧率
		unsigned kbps = 0);                //输出视频码率默认其0

	int test(void);

	// 解耦支持：允许外部设置 ffmpeg 可执行文件目录与临时目录（不破坏原有逻辑）
	// 在单元测试或 CI 中由调用方注入路径，避免硬编码耦合
	void SetFfmpegPath(const char* path);
	const char* GetFfmpegPath();

	void SetTmpPath(const char* path);
	const char* GetTmpPath();
}
