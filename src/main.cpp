#include "pic.h"
#include "code.h"
#include "ffmpeg.h"
#include "ImgDecode.h"
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <set>
#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/logger.hpp> 

#define Show_Img(src) do\
{\
	cv::imshow("DEBUG", src);\
	cv::waitKey();\
}while (0);

int FileToVideo(const char* filePath, const char* videoPath, int timLim = INT_MAX, int fps = 15)
{
	FILE* fp = fopen(filePath, "rb");
	if (fp == nullptr) {
		std::cerr << "Cannot open input file: " << filePath << std::endl;
		return 1;
	}
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	rewind(fp);
	char* temp = (char*)malloc(sizeof(char) * size);
	if (temp == nullptr) { fclose(fp); return 1; }
	fread(temp, 1, size, fp);
	fclose(fp);

	std::error_code ec;
	std::filesystem::remove_all("outputImg", ec);
	std::filesystem::create_directory("outputImg", ec);

	long long maxFrames = 1LL * fps * timLim / 1000;
	if (timLim == INT_MAX) maxFrames = INT_MAX;

	Code::Main(temp, size, "outputImg", "png", maxFrames);
	FFMPEG::ImagetoVideo("outputImg", "png", videoPath, fps, 60, 100000);

	std::filesystem::remove_all("outputImg", ec);
	free(temp);
	return 0;
}

int VideoToFile(const char* videoPath, const char* filePath, const char* voutPath = nullptr)
{
	std::cout << "Opening video file directly in memory: " << videoPath << std::endl;

	// 【核心提速】：不再调用 FFMPEG::VideotoImage 写废硬盘
	// 直接使用 OpenCV VideoCapture 逐帧内存硬解！
	//
	cv::VideoCapture cap(videoPath);
	if (!cap.isOpened()) {
		std::cerr << "Error: Could not open video file." << std::endl;
		return 1;
	}

	int precode = -1;
	std::vector<unsigned char> outputFile;
	std::vector<unsigned char> voutFile;
	bool hasStarted = false;
	std::set<int> parsedFrames;

	cv::Mat srcImg;
	int frameCount = 0;

	// 循环从内存读取视频帧，彻底消灭 IO 瓶颈
	//
	while (cap.read(srcImg))
	{
		frameCount++;
		if (srcImg.empty()) continue;

		cv::Mat disImg;
		bool parseSuccess = ImgParse::Main(srcImg, disImg);

		if (!parseSuccess)
		{
			continue;
		}

		ImageDecode::ImageInfo imageInfo;
		if (ImageDecode::Main(disImg, imageInfo))
		{
			continue;
		}

		if (!hasStarted)
		{
			if (imageInfo.IsStart) {
				hasStarted = true;
			}
			else continue;
		}

		if (parsedFrames.count(imageInfo.FrameBase) > 0)
			continue;

		if (precode != -1 && ((precode + 1) & UINT16_MAX) != imageInfo.FrameBase)
		{
			int skippedFrames = imageInfo.FrameBase - (precode + 1);
			if (skippedFrames < 0) {
				skippedFrames += UINT16_MAX + 1;
			}

			std::cerr << "Warning: Skipped " << skippedFrames << " logic frame(s). Expected "
				<< ((precode + 1) & UINT16_MAX) << ", but got " << imageInfo.FrameBase << std::endl;

			for (int i = 0; i < skippedFrames; ++i) {
				for (int j = 0; j < ImageDecode::BytesPerFrame; ++j) {
					outputFile.push_back(0x00);
					if (voutPath != nullptr) voutFile.push_back(0x00);
				}
			}
		}

		printf("Logic Frame %d (Video Frame %d) is parsed successfully!\n", imageInfo.FrameBase, frameCount);

		parsedFrames.insert(imageInfo.FrameBase);
		precode = imageInfo.FrameBase;

		for (auto& e : imageInfo.Info) {
			outputFile.push_back(e);
			if (voutPath != nullptr) voutFile.push_back(0xFF);
		}

		if (imageInfo.IsEnd) {
			break;
		}
	}

	cap.release();

	if (hasStarted)
	{
		printf("\nVideo Parse is success.\nFile Size:%zu B\nTotal Logic Frame:%d\n", outputFile.size(), precode);

		FILE* fp = fopen(filePath, "wb");
		if (fp == nullptr) return 1;
		fwrite(outputFile.data(), sizeof(unsigned char), outputFile.size(), fp);
		fclose(fp);

		if (voutPath != nullptr) {
			FILE* vout_fp = fopen(voutPath, "wb");
			if (vout_fp != nullptr) {
				fwrite(voutFile.data(), sizeof(unsigned char), voutFile.size(), vout_fp);
				fclose(vout_fp);
			}
			else {
				std::cerr << "Warning: Cannot create vout file: " << voutPath << std::endl;
			}
		}

		return 0;
	}

	std::cerr << "[ERROR] Failed to parse video into file. Start frame was never found or all frames corrupted." << std::endl;
	return 1;
}

int main(int argc, char* argv[])
{
	cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

#if defined(BUILD_ENCODER)
	if (argc == 4)
		return FileToVideo(argv[1], argv[2], std::stoi(argv[3]));
	else if (argc == 5)
		return FileToVideo(argv[1], argv[2], std::stoi(argv[3]), std::stoi(argv[4]));
	puts("Usage: encoder <inputFile> <outputVideo> <timeLimit_ms> [fps]");
#else
	if (argc == 3)
		return VideoToFile(argv[1], argv[2]);
	else if (argc == 4)
		return VideoToFile(argv[1], argv[2], argv[3]);
	puts("Usage: decoder <inputVideo> <outputFile> [vout.bin]");
#endif
	return 1;
}