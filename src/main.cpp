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

	// 根据要求，时间限制 (ms) 决定最大帧数
	//
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
	std::error_code ec;
	std::filesystem::remove_all("inputImg", ec);
	std::filesystem::create_directory("inputImg", ec);

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
	std::vector<unsigned char> voutFile; // 用于存储标记每位是否有效的标志位
	//
	bool hasStarted = false;
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
			// 如果解析失败（如定位点不足），跳过此帧
			//
			continue;
		}

		ImageDecode::ImageInfo imageInfo;
		if (ImageDecode::Main(disImg, imageInfo))
		{
			std::cout << "decode failed" << std::endl;
			continue; 
			// 解码内容失败，跳过该帧
			//
		}

		if (!hasStarted)
		{
			if (imageInfo.IsStart) {
				hasStarted = true;
			}
			else continue; 
			// 忽略 Start 帧之前的杂帧
			//
		}

		if (parsedFrames.count(imageInfo.FrameBase) > 0)
			continue; 
		// 忽略重复处理的帧
		//

	// 检查是否有跳帧现象
	//
		if (precode != -1 && ((precode + 1) & UINT16_MAX) != imageInfo.FrameBase)
		{
			int skippedFrames = imageInfo.FrameBase - (precode + 1);
			if (skippedFrames < 0) {
				// 处理溢出翻转情况 (很少见，但是符合逻辑)
				//
				skippedFrames += UINT16_MAX + 1;
			}

			std::cerr << "Warning: Skipped " << skippedFrames << " logic frame(s). Expected "
				<< ((precode + 1) & UINT16_MAX) << ", but got " << imageInfo.FrameBase << std::endl;

			// 填补因为跳帧而丢失的数据，使用 0x00 填充数据
			// 并把对应的 voutFile 标记为 0 (无效)
			//
			for (int i = 0; i < skippedFrames; ++i) {
				for (int j = 0; j < ImageDecode::BytesPerFrame; ++j) {
					outputFile.push_back(0x00);
					if (voutPath != nullptr) voutFile.push_back(0x00); 
					// 0 代表错误/无效
					//
				}
			}
		}

		printf("Frame %d is parsed successfully!\n", imageInfo.FrameBase);

		parsedFrames.insert(imageInfo.FrameBase);
		precode = imageInfo.FrameBase;

		// 将当前正常解码的帧数据写入 outputFile
		// 并把对应的 voutFile 标记为 1 (有效)
		//
		for (auto& e : imageInfo.Info) {
			outputFile.push_back(e);
			if (voutPath != nullptr) voutFile.push_back(0xff); 
			// 1 代表正确/有效
			//
		}

		if (imageInfo.IsEnd) {
			break;
		}
	}

	if (hasStarted)
	{
		printf("\nVideo Parse is success.\nFile Size:%zu B\nTotal Frame:%d\n", outputFile.size(), precode);

		// 写入解码结果数据文件
		//
		FILE* fp = fopen(filePath, "wb");
		if (fp == nullptr) return 1;
		fwrite(outputFile.data(), sizeof(unsigned char), outputFile.size(), fp);
		fclose(fp);

		// 如果提供了 voutPath 并且我们需要输出有效性标记文件
		//
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
	// 编码器接口规范
	// argv[1]: 输入二进制文件 (in.bin)
	// argv[2]: 输出视频文件 (out.mp4)
	// argv[3]: 视频最大时长限制(毫秒) 
	//
	if (argc == 4)
		return FileToVideo(argv[1], argv[2], std::stoi(argv[3]));
	else if (argc == 5)
		return FileToVideo(argv[1], argv[2], std::stoi(argv[3]), std::stoi(argv[4])); 
	puts("Usage: encoder <inputFile> <outputVideo> <timeLimit_ms> [fps]");
#else
	// 解码器接口规范
	// argv[1]: 输入视频文件 (recorded.mp4)
	// argv[2]: 解码后输出文件 (out.bin)
	// argv[3] (���选): 每位有效性标记文件 (vout.bin)
	//
	if (argc == 3)
		return VideoToFile(argv[1], argv[2]);
	else if (argc == 4)
		return VideoToFile(argv[1], argv[2], argv[3]); // 包含 vout.bin 标记文件输出
	puts("Usage: decoder <inputVideo> <outputFile> [vout.bin]");
#endif
	return 1;
}