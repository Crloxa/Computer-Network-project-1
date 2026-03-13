#include "code.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>

// 定义下面这个宏来开启编码端调试图显示。
//#define Code_DEBUG
#define Show_Scale_Img(src) do\
{\
	Mat temp = ScaleToDisSize(src);\
	imshow("Code_DEBUG", temp);\
	waitKey();\
} while (0)

namespace Code
{
	constexpr int BytesPerFrame = 1242;
	constexpr int FrameSize = 133;
	constexpr int FrameOutputRate = 10;
	constexpr int FrameOutputSize = FrameSize * FrameOutputRate;
	constexpr int SafeAreaWidth = 2;
	constexpr int QrPointSize = 21;
	constexpr int SmallQrPointbias = 6;
	constexpr int SmallQrPointRadius = 3;
	constexpr int HeaderRowCount = 3;
	constexpr int HeaderStartRow = QrPointSize;
	constexpr int DataStart = QrPointSize + HeaderRowCount;
	constexpr int CornerReserveSize = 21;
	constexpr int SmallAreaSplit = 8;
	constexpr int RectAreaCount = 7;

	struct DataRegion
	{
		int top;
		int left;
		int height;
		int width;
	};

	enum color
	{
		Black = 0,
		White = 7
	};

	enum class FrameType
	{
		Start = 0,
		End = 1,
		StartAndEnd = 2,
		Normal = 3
	};

	const Vec3b pixel[8] =
	{
		Vec3b(0, 0, 0), Vec3b(0, 0, 255), Vec3b(0, 255, 0), Vec3b(0, 255, 255),
		Vec3b(255, 0, 0), Vec3b(255, 0, 255), Vec3b(255, 255, 0), Vec3b(255, 255, 255)
	};

	const std::array<DataRegion, RectAreaCount> kDataRegions =
	{{
		{DataStart, SafeAreaWidth, FrameSize - CornerReserveSize - DataStart, 16},
		{SafeAreaWidth, DataStart, 16, FrameSize - QrPointSize - DataStart},
		{DataStart, DataStart, 72, 72},
		{DataStart, 96, 72, 16},
		{96, DataStart, 16, 72},
		{96, 96, 16, SmallAreaSplit},
		{104, 104, SmallAreaSplit, SmallAreaSplit}
	}};

	int getRegionCapacityBytes(const DataRegion& region)
	{
		return region.height * (region.width / 8);
	}

	bool isInsideSmallQrPoint(int row, int col)
	{
		const int center = FrameSize - SmallQrPointbias;
		return std::abs(row - center) <= SmallQrPointRadius && std::abs(col - center) <= SmallQrPointRadius;
	}

	void fillBinaryNoiseCell(Vec3b& cell)
	{
		cell = pixel[(std::rand() & 1) ? White : Black];
	}

	Mat ScaleToDisSize(const Mat& src)
	{
		Mat dis(FrameOutputSize, FrameOutputSize, CV_8UC3);
		for (int i = 0; i < FrameOutputSize; ++i)
		{
			for (int j = 0; j < FrameOutputSize; ++j)
			{
				dis.at<Vec3b>(i, j) = src.at<Vec3b>(i / FrameOutputRate, j / FrameOutputRate);
			}
		}
		return dis;
	}

	uint16_t CalCheckCode(const unsigned char* info, int len, bool isStart, bool isEnd, uint16_t frameBase)
	{
		uint16_t ans = 0;
		const int cutlen = (len / 2) * 2;
		for (int i = 0; i < cutlen; i += 2)
			ans ^= (static_cast<uint16_t>(info[i]) << 8) | info[i + 1];
		if (len & 1)
			ans ^= static_cast<uint16_t>(info[cutlen]) << 8;
		ans ^= len;
		ans ^= frameBase;
		ans ^= static_cast<uint16_t>((isStart << 1) + isEnd);
		return ans;
	}

	void BulidSafeArea(Mat& mat)
	{
		for (int i = 0; i < FrameSize; ++i)
		{
			for (int j = 0; j < SafeAreaWidth; ++j)
			{
				mat.at<Vec3b>(i, j) = pixel[White];
				mat.at<Vec3b>(i, FrameSize - SafeAreaWidth + j) = pixel[White];
				mat.at<Vec3b>(j, i) = pixel[White];
				mat.at<Vec3b>(FrameSize - SafeAreaWidth + j, i) = pixel[White];
			}
		}
#ifdef Code_DEBUG
		Show_Scale_Img(mat);
#endif
	}

	void fillCornerNoiseArea(Mat& mat)
	{
		const int start = FrameSize - CornerReserveSize;
		for (int i = start; i < FrameSize; ++i)
		{
			for (int j = start; j < FrameSize; ++j)
			{
				if (isInsideSmallQrPoint(i, j))
					continue;
				fillBinaryNoiseCell(mat.at<Vec3b>(i, j));
			}
		}
	}

	void drawSmallQrPoint(Mat& mat)
	{
		const int center = FrameSize - SmallQrPointbias;
		const Vec3b vec3bsmall[5] =
		{
			pixel[Black],
			pixel[Black],
			pixel[White],
			pixel[Black],
			pixel[White],
		};
		for (int i = -SmallQrPointRadius; i <= SmallQrPointRadius; ++i)
		{
			for (int j = -SmallQrPointRadius; j <= SmallQrPointRadius; ++j)
			{
				mat.at<Vec3b>(center + i, center + j) = vec3bsmall[std::max(std::abs(i), std::abs(j))];
			}
		}
	}

	void BulidQrPoint(Mat& mat)
	{
		const std::array<std::array<int, 2>, 3> pointPos =
		{{
			{0, 0},
			{0, FrameSize - QrPointSize},
			{FrameSize - QrPointSize, 0}
		}};
		const Vec3b vec3bBig[11] =
		{
			pixel[Black], pixel[Black], pixel[Black], pixel[Black],
			pixel[White], pixel[White],
			pixel[Black], pixel[Black],
			pixel[White], pixel[White], pixel[White]
		};
		for (const auto& pos : pointPos)
		{
			for (int i = 0; i < QrPointSize; ++i)
			{
				for (int j = 0; j < QrPointSize; ++j)
				{
					const int index = std::max(std::abs(i - QrPointSize / 2), std::abs(j - QrPointSize / 2));
					mat.at<Vec3b>(pos[0] + i, pos[1] + j) = vec3bBig[index];
				}
			}
		}
		fillCornerNoiseArea(mat);
		drawSmallQrPoint(mat);
#ifdef Code_DEBUG
		Show_Scale_Img(mat);
#endif
	}

	void fillDataNoise(Mat& mat)
	{
		for (const auto& region : kDataRegions)
		{
			for (int i = 0; i < region.height; ++i)
			{
				for (int j = 0; j < region.width; ++j)
				{
					fillBinaryNoiseCell(mat.at<Vec3b>(region.top + i, region.left + j));
				}
			}
		}
	}

	void BulidCheckCodeAndFrameNo(Mat& mat, uint16_t checkcode, uint16_t FrameNo)
	{
		for (int i = 0; i < 16; ++i)
		{
			mat.at<Vec3b>(HeaderStartRow + 1, SafeAreaWidth + i) = pixel[(checkcode & 1) ? White : Black];
			checkcode >>= 1;
		}
		for (int i = 0; i < 16; ++i)
		{
			mat.at<Vec3b>(HeaderStartRow + 2, SafeAreaWidth + i) = pixel[(FrameNo & 1) ? White : Black];
			FrameNo >>= 1;
		}
#ifdef Code_DEBUG
		Show_Scale_Img(mat);
#endif
	}

	void BulidInfoRect(Mat& mat, const char* info, int len, int areaID)
	{
		const DataRegion& region = kDataRegions[areaID];
		const unsigned char* pos = reinterpret_cast<const unsigned char*>(info);
		const unsigned char* end = pos + len;
		for (int i = 0; i < region.height; ++i)
		{
			for (int j = 0; j < region.width / 8; ++j)
			{
				if (pos == end)
					break;
				unsigned char outputCode = *pos++;
				for (int k = 0; k < 8; ++k)
				{
					mat.at<Vec3b>(region.top + i, region.left + j * 8 + k) = pixel[(outputCode & 1) ? White : Black];
					outputCode >>= 1;
				}
			}
			if (pos == end)
				break;
		}
#ifdef Code_DEBUG
		Show_Scale_Img(mat);
#endif
	}

	void BulidFrameFlag(Mat& mat, FrameType frameType, int tailLen)
	{
		switch (frameType)
		{
		case FrameType::Start:
			mat.at<Vec3b>(HeaderStartRow, SafeAreaWidth) = pixel[White];
			mat.at<Vec3b>(HeaderStartRow, SafeAreaWidth + 1) = pixel[White];
			mat.at<Vec3b>(HeaderStartRow, SafeAreaWidth + 2) = pixel[Black];
			mat.at<Vec3b>(HeaderStartRow, SafeAreaWidth + 3) = pixel[Black];
			break;
		case FrameType::End:
			mat.at<Vec3b>(HeaderStartRow, SafeAreaWidth) = pixel[Black];
			mat.at<Vec3b>(HeaderStartRow, SafeAreaWidth + 1) = pixel[Black];
			mat.at<Vec3b>(HeaderStartRow, SafeAreaWidth + 2) = pixel[White];
			mat.at<Vec3b>(HeaderStartRow, SafeAreaWidth + 3) = pixel[White];
			break;
		case FrameType::StartAndEnd:
			mat.at<Vec3b>(HeaderStartRow, SafeAreaWidth) = pixel[White];
			mat.at<Vec3b>(HeaderStartRow, SafeAreaWidth + 1) = pixel[White];
			mat.at<Vec3b>(HeaderStartRow, SafeAreaWidth + 2) = pixel[White];
			mat.at<Vec3b>(HeaderStartRow, SafeAreaWidth + 3) = pixel[White];
			break;
		default:
			mat.at<Vec3b>(HeaderStartRow, SafeAreaWidth) = pixel[Black];
			mat.at<Vec3b>(HeaderStartRow, SafeAreaWidth + 1) = pixel[Black];
			mat.at<Vec3b>(HeaderStartRow, SafeAreaWidth + 2) = pixel[Black];
			mat.at<Vec3b>(HeaderStartRow, SafeAreaWidth + 3) = pixel[Black];
			break;
		}
		for (int i = 4; i < 16; ++i)
		{
			mat.at<Vec3b>(HeaderStartRow, SafeAreaWidth + i) = pixel[(tailLen & 1) ? White : Black];
			tailLen >>= 1;
		}
#ifdef Code_DEBUG
		Show_Scale_Img(mat);
#endif
	}

	Mat CodeFrame(FrameType frameType, const char* info, int tailLen, int FrameNo)
	{
		Mat codeMat(FrameSize, FrameSize, CV_8UC3, Vec3d(255, 255, 255));
		if (frameType != FrameType::End && frameType != FrameType::StartAndEnd)
			tailLen = BytesPerFrame;
		BulidSafeArea(codeMat);
		BulidQrPoint(codeMat);
		fillDataNoise(codeMat);

		const int checkCode = CalCheckCode(reinterpret_cast<const unsigned char*>(info), tailLen,
			frameType == FrameType::Start || frameType == FrameType::StartAndEnd,
			frameType == FrameType::End || frameType == FrameType::StartAndEnd,
			FrameNo);
		BulidFrameFlag(codeMat, frameType, tailLen);
		BulidCheckCodeAndFrameNo(codeMat, checkCode, FrameNo % 65536);

		int bytesToWrite = BytesPerFrame;
		for (int i = 0; i < RectAreaCount && bytesToWrite > 0; ++i)
		{
			const int lennow = std::min(bytesToWrite, getRegionCapacityBytes(kDataRegions[i]));
			BulidInfoRect(codeMat, info, lennow, i);
			bytesToWrite -= lennow;
			info += lennow;
		}
		return codeMat;
	}

	void Main(const char* info, int len, const char* savePath, const char* outputFormat, int FrameCountLimit)
	{
		Mat output;
		char fileName[128];
		int counter = 0;
		if (FrameCountLimit == 0)
			return;
		if (len <= 0)
			return;
		if (len <= BytesPerFrame)
		{
			unsigned char BUF[BytesPerFrame + 5];
			std::memcpy(BUF, info, sizeof(unsigned char) * len);
			for (int i = len; i <= BytesPerFrame; ++i)
				BUF[i] = std::rand() % 256;
			output = ScaleToDisSize(CodeFrame(FrameType::StartAndEnd, reinterpret_cast<char*>(BUF), len, 0));
			std::snprintf(fileName, sizeof(fileName), "%s\\%05d.%s", savePath, counter++, outputFormat);
			imwrite(fileName, output);
		}
		else
		{
			int i = 0;
			len -= BytesPerFrame;
			output = ScaleToDisSize(CodeFrame(FrameType::Start, info, len, 0));
			--FrameCountLimit;

			std::snprintf(fileName, sizeof(fileName), "%s\\%05d.%s", savePath, counter++, outputFormat);
			imwrite(fileName, output);

			while (len > 0 && FrameCountLimit > 0)
			{
				info += BytesPerFrame;
				--FrameCountLimit;
				if (len - BytesPerFrame > 0)
				{
					if (FrameCountLimit > 0)
						output = ScaleToDisSize(CodeFrame(FrameType::Normal, info, BytesPerFrame, ++i));
					else
						output = ScaleToDisSize(CodeFrame(FrameType::End, info, BytesPerFrame, ++i));
				}
				else
				{
					unsigned char BUF[BytesPerFrame + 5];
					std::memcpy(BUF, info, sizeof(unsigned char) * len);
					for (int j = len; j <= BytesPerFrame; ++j)
						BUF[j] = std::rand() % 256;
					output = ScaleToDisSize(CodeFrame(FrameType::End, reinterpret_cast<char*>(BUF), len, ++i));
				}
				len -= BytesPerFrame;
				std::snprintf(fileName, sizeof(fileName), "%s\\%05d.%s", savePath, counter++, outputFormat);
				imwrite(fileName, output);
			}
		}
	}
}
