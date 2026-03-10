//ffmpeg
#include "ffmpeg.h"
namespace FFMPEG
{
	constexpr int MAXBUFLEN = 256;//缓冲区大小
	const char ffmpegPath[] = "D:/ffmpeg/bin/";
	const char tmpPath[] = "tmpdir";//临时文件目录

	int ImagetoVideo(const char* imagePath,//路径格式等
		const char* imageFormat,
		const char* videoPath,
		unsigned rawFrameRates,//输出图片序列帧率
		unsigned outputFrameRates, //输出视频帧率
		unsigned kbps)//码率
	{
		char BUF[MAXBUFLEN];
		// 使用 -framerate 指定输入帧率，使用快速 preset 与多线程，加上 yuv420p 保证解码兼容性
		if (kbps)
			snprintf(BUF, MAXBUFLEN,
				"\"%sffmpeg.exe\" -y -framerate %u -f image2 -i %s\\%%05d.%s -c:v libx264 -preset ultrafast -b:v %uK -threads 0 -pix_fmt yuv420p -r %u %s",
				ffmpegPath, rawFrameRates, imagePath, imageFormat, kbps, outputFrameRates, videoPath);
		else
			snprintf(BUF, MAXBUFLEN,
				"\"%sffmpeg.exe\" -y -framerate %u -f image2 -i %s\\%%05d.%s -c:v libx264 -preset ultrafast -threads 0 -pix_fmt yuv420p -r %u %s",
				ffmpegPath, rawFrameRates, imagePath, imageFormat, outputFrameRates, videoPath);
		return system(BUF);
	}
	int VideotoImage(const char* videoPath,//输入视频
		const char* imagePath,
		const char* imageFormat)
	{
		char BUF[MAXBUFLEN];
		snprintf(BUF, MAXBUFLEN, "md %s", imagePath);
		system(BUF);//创建图片目录
		// 解码时启用多线程，禁用 vsync 自动重复/丢帧，保留 q:v 控制质量
		snprintf(BUF, MAXBUFLEN,
			"\"%sffmpeg.exe\" -y -i %s -threads 0 -vsync 0 -q:v 2 -f image2 %s\\%%05d.%s",
			ffmpegPath, videoPath, imagePath, imageFormat);
		return system(BUF);
	}
	int test(void)
	{
		bool tag = VideotoImage("test.mp4", tmpPath, "png");
		if (tag)
			return tag;
		tag = ImagetoVideo(tmpPath, "png", "out.mp4", 30, 30, 0);
		return tag;
	}
} // namespace ffmpeg
