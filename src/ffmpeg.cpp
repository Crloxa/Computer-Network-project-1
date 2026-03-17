//这个文件封装了ffmpeg的调用
#include "ffmpeg.h"
namespace FFMPEG
{
	constexpr int MAXBUFLEN = 256;	// 最大命令长度
	const char ffmpegPath[] = "ffmpeg\\bin\\";	//ffmpeg.exe 的路径
	const char tmpPath[] = "tmpdir";	// 视频拆帧时的临时文件夹
	// 图片序列编码成视频
	int ImagetoVideo(const char* imagePath,	//图片来源
		const char* imageFormat,	//图片格式
		const char* videoPath,		//输出视频位置
		unsigned rawFrameRates,		//输入帧率
		unsigned outputFrameRates,	//输出帧率
		unsigned kbps)	//视频码率
	{
		char BUF[MAXBUFLEN];
		if (kbps)
			snprintf(BUF, MAXBUFLEN,
				"\"%s\"ffmpeg.exe -r %u  -f image2 -i %s\\%%05d.%s -b:v %uK -vcodec libx264  -r %u %s",
				ffmpegPath, rawFrameRates, imagePath, imageFormat, kbps, outputFrameRates, videoPath);
		else
			snprintf(BUF, MAXBUFLEN,
				"\"%s\"ffmpeg.exe -r %u -f image2 -i %s\\%%05d.%s  -vcodec libx264 -r %u %s",
				ffmpegPath, rawFrameRates, imagePath, imageFormat, outputFrameRates, videoPath);
		return system(BUF);
	}
	int VideotoImage(const char* videoPath,
		const char* imagePath,
		const char* imageFormat)
	{
		char BUF[MAXBUFLEN];
		snprintf(BUF, MAXBUFLEN, "md %s", imagePath); //生成文件目录
		system(BUF);
		snprintf(BUF, MAXBUFLEN,
			"\"%s\"ffmpeg.exe -i %s -q:v 2 -f image2  %s\\%%05d.%s",
			ffmpegPath, videoPath, imagePath, imageFormat);
		return system(BUF);
	}
	int test(void)
	{
		bool tag = VideotoImage("test.mp4", tmpPath, "png");
		if (tag)
			return tag;
		tag = ImagetoVideo(tmpPath, "png", "out.mp4", 30, 30);
		return tag;
	}
} // namespace ffmpeg
