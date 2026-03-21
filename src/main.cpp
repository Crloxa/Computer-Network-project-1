#include "pic.h"
#include "code.h"
#include "ffmpeg.h"
#include "ImgDecode.h"
#include <thread>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <filesystem>
#include <iostream>

#define Show_Img(src) do\
{\
	cv::imshow("DEBUG", src);\
	cv::waitKey();\
}while (0);

// 文件转视频 (编码器逻辑)
int FileToVideo(const char* filePath, const char* videoPath, int timLim = INT_MAX, int fps = 15)
{
	FILE* fp = fopen(filePath, "rb");
	if (fp == nullptr) return 1;
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	rewind(fp);
	char* temp = (char*)malloc(sizeof(char) * size);
	if (temp == nullptr) { fclose(fp); return 1; }
	fread(temp, 1, size, fp);
	fclose(fp);

	// 使用 C++17 filesystem 替代 system("md ...") 和 system("rd ...")
	std::filesystem::remove_all("outputImg");
	std::filesystem::create_directory("outputImg");

	Code::Main(temp, size, "outputImg", "png", 1LL * fps * timLim / 1000);
	FFMPEG::ImagetoVideo("outputImg", "png", videoPath, fps, 60, 100000);

	std::filesystem::remove_all("outputImg");
	free(temp);
	return 0;
}

// 视频转文件 (解码器逻辑)
int VideoToFile(const char* videoPath, const char* filePath)
{
	char imgName[256];
	std::filesystem::remove_all("inputImg");
	std::filesystem::create_directory("inputImg");

	std::atomic<bool> isThreadOver = false;
	std::thread th([&] {
		FFMPEG::VideotoImage(videoPath, "inputImg", "jpg");
		isThreadOver = true;
		});

	int precode = -1;
	std::vector<unsigned char> outputFile;
	bool hasStarted = false;
	bool ret = 0;

	for (int i = 1;; ++i)
	{
		snprintf(imgName, 256, "inputImg/%05d.jpg", i);
		FILE* fp = nullptr;
		do
		{
			fp = fopen(imgName, "rb");
			if (fp == nullptr && !isThreadOver) {
				std::this_thread::yield(); // 等待 FFmpeg 抽帧
			}
		} while (fp == nullptr && !isThreadOver);

		if (fp == nullptr)
		{
			// 线程结束且文件仍然不存在，说明视频读取完毕
			break;
		}

		cv::Mat srcImg = cv::imread(imgName, 1);
		cv::Mat disImg;
		fclose(fp);

		// 读完图片后立即通过 C++ 标准库删除，替代 system("del ...")
		std::filesystem::remove(imgName);

		// ★ 结合 pic.cpp 和纯数字后备逻辑！
		if (ImgParse::Main(srcImg, disImg))
		{
			// pic.cpp 物理透视纠正失败时，走纯数字像素的等比缩放逻辑
			if (srcImg.rows == ImageDecode::FrameSize && srcImg.cols == ImageDecode::FrameSize) {
				disImg = srcImg;
			}
			else {
				cv::resize(srcImg, disImg, cv::Size(ImageDecode::FrameSize, ImageDecode::FrameSize), 0.0, 0.0, cv::INTER_NEAREST);
			}
		}

		ImageDecode::ImageInfo imageInfo;
		bool ans = ImageDecode::Main(disImg, imageInfo);
		if (ans)
		{
			continue;
		}

		if (!hasStarted)
		{
			if (imageInfo.IsStart)
				hasStarted = true;
			else continue;
		}

		if (precode == imageInfo.FrameBase)
			continue;

		if (precode != -1 && ((precode + 1) & UINT16_MAX) != imageInfo.FrameBase)
		{
			puts("error, there is a skipped frame, there are some images parsed failed.");
			ret = 1;
			break;
		}
		printf("Frame %d is parsed!\n", imageInfo.FrameBase);

		precode = imageInfo.FrameBase;
		for (auto& e : imageInfo.Info)
			outputFile.push_back(e);

		if (imageInfo.IsEnd)
			break;
	}

	th.join();

	if (ret == 0)
	{
		printf("\nVideo Parse is success.\nFile Size:%zu B\nTotal Frame:%d\n", outputFile.size(), precode);
		FILE* fp = fopen(filePath, "wb");
		if (fp == nullptr) return 1;
		fwrite(outputFile.data(), sizeof(unsigned char), outputFile.size(), fp);
		fclose(fp);
		return 0;
	}

	exit(1);
}

int main(int argc, char* argv[])
{
	// 使用 CMake 中配置的宏 BUILD_ENCODER 和 BUILD_DECODER 代替 constexpr 控制流
	// 彻底解决 "C2181 没有匹配 if 的非法 else" 问题
#if defined(BUILD_ENCODER)
	if (argc == 4)
		return FileToVideo(argv[1], argv[2], std::stoi(argv[3]));
	else if (argc == 5)
		return FileToVideo(argv[1], argv[2], std::stoi(argv[3]), std::stoi(argv[4]));

	puts("Usage: encoder <inputFile> <outputVideo> <timeLimit> [fps]");
#else
	if (argc == 3)
		return VideoToFile(argv[1], argv[2]);

	puts("Usage: decoder <inputVideo> <outputFile>");
#endif

	return 1;
}