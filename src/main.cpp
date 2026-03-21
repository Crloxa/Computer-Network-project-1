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
#include <opencv2/core/utils/logger.hpp> 

#define Show_Img(src) do\
{\
	cv::imshow("DEBUG", src);\
	cv::waitKey();\
}while (0);

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

	std::error_code ec;
	std::filesystem::remove_all("outputImg", ec);
	std::filesystem::create_directory("outputImg", ec);

	Code::Main(temp, size, "outputImg", "png", 1LL * fps * timLim / 1000);
	FFMPEG::ImagetoVideo("outputImg", "png", videoPath, fps, 60, 100000);

	std::filesystem::remove_all("outputImg", ec);
	free(temp);
	return 0;
}

int VideoToFile(const char* videoPath, const char* filePath)
{
	std::error_code ec;
	std::filesystem::remove_all("inputImg", ec);
	std::filesystem::create_directory("inputImg", ec);

	// [保留] 创建存放处理后图片的文件夹
	// //
	std::filesystem::create_directory("pic_output", ec);

	std::cout << "Extracting frames from video... Please wait." << std::endl;
	FFMPEG::VideotoImage(videoPath, "inputImg", "jpg");
	std::cout << "Frames extraction completed. Start decoding..." << std::endl;

	std::vector<std::string> imageFiles;
	for (const auto& entry : std::filesystem::directory_iterator("inputImg", ec))
	{
		if (entry.is_regular_file() && entry.path().extension() == ".jpg")
		{
			imageFiles.push_back(entry.path().string());
		}
	}
	std::sort(imageFiles.begin(), imageFiles.end());

	if (imageFiles.empty())
	{
		std::cerr << "Error: No frames found in inputImg directory." << std::endl;
		return 1;
	}

	int precode = -1;
	std::vector<unsigned char> outputFile;
	bool hasStarted = false;
	bool ret = 0;
	std::set<int> parsedFrames;

	for (const auto& imgName : imageFiles)
	{
		cv::Mat srcImg = cv::imread(imgName, cv::IMREAD_COLOR);
		if (srcImg.empty()) continue;

		cv::Mat disImg;
		// 调用 pic.cpp 的解析
		//
		bool parseSuccess = ImgParse::Main(srcImg, disImg);

		if (!parseSuccess)
		{
			// [重要修改] 如果 pic.cpp 解析失败（找不到3个角点），直接跳过此帧！
			// 绝对不能把 srcImg 强制 resize 后当作成品图，那是一团乱码，毫无意义。
			// 录制视频中丢几帧是正常的，只要不是所有帧都丢就行。
			//
			continue;
		}

		// 保存成功解析并裁剪后的图片
		// //
		{
			std::filesystem::path p(imgName);
			std::string filename = p.filename().string();
			std::string outputPath = "pic_output/" + filename;
			cv::imwrite(outputPath, disImg);
		}

		ImageDecode::ImageInfo imageInfo;
		if (ImageDecode::Main(disImg, imageInfo))
		{
			std::cout << "decode failed" << std::endl;
			continue; // 解码内容失败
			//
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
			std::cerr << "Warning: Possible skipped logic frame. Expected " << ((precode + 1) & UINT16_MAX)
				<< ", but got " << imageInfo.FrameBase << std::endl;
		}

		printf("Frame %d is parsed successfully!\n", imageInfo.FrameBase);

		parsedFrames.insert(imageInfo.FrameBase);
		precode = imageInfo.FrameBase;

		for (auto& e : imageInfo.Info) {
			outputFile.push_back(e);
		}

		if (imageInfo.IsEnd) {
			break;
		}
	}

	if (hasStarted)
	{
		printf("\nVideo Parse is success.\nFile Size:%zu B\nTotal Frame:%d\n", outputFile.size(), precode);
		FILE* fp = fopen(filePath, "wb");
		if (fp == nullptr) return 1;
		fwrite(outputFile.data(), sizeof(unsigned char), outputFile.size(), fp);
		fclose(fp);

		std::filesystem::remove_all("inputImg", ec);
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
	puts("Usage: encoder <inputFile> <outputVideo> <timeLimit> [fps]");
#else
	if (argc == 3)
		return VideoToFile(argv[1], argv[2]);
	puts("Usage: decoder <inputVideo> <outputFile>");
#endif
	return 1;
}