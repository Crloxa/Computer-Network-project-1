#include "code.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// 定义下面这个宏来开启编码端调试图显示。
//#define Code_DEBUG
// 定义下面这个宏来额外输出彩色分区边界预览图。
//#define Layout_DEBUG
#define Show_Scale_Img(src) do\
{\
	Mat temp = ScaleToDisSize(src);\
	imshow("Code_DEBUG", temp);\
	waitKey();\
} while (0)

namespace Code
{
	constexpr int BytesPerFrame = 1878;
	constexpr int FrameSize = 133;
	constexpr int FrameOutputRate = 8;
	constexpr int FrameOutputSize = FrameSize * FrameOutputRate;
	constexpr int SafeAreaWidth = 2;
	constexpr int QrPointSize = 21;
	constexpr int SmallQrPointbias = 7;
	constexpr int SmallQrPointRadius = 3;
	constexpr int CornerReserveSize = 21;
	constexpr int HeaderHeight = 3;
	constexpr int HeaderWidth = 16;
	constexpr int HeaderLeft = 21;
	constexpr int HeaderTop = 3;
	constexpr int HeaderFieldHeight = 1;
	constexpr int HeaderFieldBits = 16;
	constexpr int HeaderBitWidth = 1;
	constexpr int HeaderInnerLeft = 0;
	constexpr int TopDataLeft = HeaderLeft + HeaderWidth;
	constexpr int TopDataWidth = 75;
	constexpr int DataAreaCount = 5;
	constexpr int PaddingCellCount = 4;

	struct DataArea
	{
		int top;
		int left;
		int height;
		int width;
		int trimRight;
	};

	struct CellPos
	{
		int row;
		int col;
	};

	struct DebugRegion
	{
		const char* name;
		int top;
		int left;
		int height;
		int width;
		Vec3b color;
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

	const std::array<DataArea, DataAreaCount> kDataAreas =
	{{
		{3, TopDataLeft, 3, TopDataWidth, 0},
		{6, 21, 15, 91, 0},
		{21, 3, 88, 127, 0},
		{109, 3, 3, 127, 0},
		{112, 21, 18, 91, 0}
	}};

	const std::array<DebugRegion, 10> kDebugRegions =
	{{
		{"header", HeaderTop, HeaderLeft, HeaderHeight, HeaderWidth, Vec3b(0, 0, 255)},
		{"data1", kDataAreas[0].top, kDataAreas[0].left, kDataAreas[0].height, kDataAreas[0].width, Vec3b(255, 0, 0)},
		{"data1_lower", kDataAreas[1].top, kDataAreas[1].left, kDataAreas[1].height, kDataAreas[1].width, Vec3b(255, 0, 0)},
		{"data2", kDataAreas[2].top, kDataAreas[2].left, kDataAreas[2].height, kDataAreas[2].width, Vec3b(0, 255, 0)},
		{"data4", kDataAreas[3].top, kDataAreas[3].left, kDataAreas[3].height, kDataAreas[3].width, Vec3b(0, 255, 255)},
		{"data3", kDataAreas[4].top, kDataAreas[4].left, kDataAreas[4].height, kDataAreas[4].width, Vec3b(255, 255, 0)},
		{"corner_data_v", 112, 112, 18, 9, Vec3b(0, 200, 255)},
		{"corner_data_h", 112, 121, 9, 9, Vec3b(0, 200, 255)},
		{"corner", FrameSize - CornerReserveSize, FrameSize - CornerReserveSize, CornerReserveSize, CornerReserveSize, Vec3b(255, 0, 255)},
		{"small_qr", FrameSize - SmallQrPointbias - SmallQrPointRadius, FrameSize - SmallQrPointbias - SmallQrPointRadius, SmallQrPointRadius * 2 + 1, SmallQrPointRadius * 2 + 1, Vec3b(0, 128, 255)}
	}};

	bool isInsideSmallQrPoint(int row, int col)
	{
		const int center = FrameSize - SmallQrPointbias;
		return std::abs(row - center) <= SmallQrPointRadius && std::abs(col - center) <= SmallQrPointRadius;
	}

	bool isInsideCornerQuietZone(int row, int col)
	{
		return row >= 130 || col >= 130;
	}

	bool isInsideCornerSafetyZone(int row, int col)
	{
		const int center = FrameSize - SmallQrPointbias;
		return std::abs(row - center) <= SmallQrPointRadius + 2 && std::abs(col - center) <= SmallQrPointRadius + 2;
	}

	void fillBinaryNoiseCell(Vec3b& cell)
	{
		cell = pixel[(std::rand() & 1) ? White : Black];
	}

	std::vector<CellPos> buildAreaCells(const DataArea& area)
	{
		std::vector<CellPos> cells;
		for (int row = area.top; row < area.top + area.height; ++row)
		{
			const int rowWidth = area.width - area.trimRight;
			for (int col = area.left; col < area.left + rowWidth; ++col)
			{
				cells.push_back({ row, col });
			}
		}
		return cells;
	}

	std::vector<CellPos> buildCornerDataCells()
	{
		std::vector<CellPos> cells;
		for (int row = FrameSize - CornerReserveSize; row < FrameSize; ++row)
		{
			for (int col = FrameSize - CornerReserveSize; col < FrameSize; ++col)
			{
				if (isInsideCornerQuietZone(row, col))
				{
					continue;
				}
				if (isInsideCornerSafetyZone(row, col))
				{
					continue;
				}
				cells.push_back({ row, col });
			}
		}
		return cells;
	}

	std::vector<CellPos> buildFullDataCells()
	{
		std::vector<CellPos> cells;
		for (const auto& area : kDataAreas)
		{
			const auto areaCells = buildAreaCells(area);
			cells.insert(cells.end(), areaCells.begin(), areaCells.end());
		}
		const auto cornerCells = buildCornerDataCells();
		cells.insert(cells.end(), cornerCells.begin(), cornerCells.end());
		return cells;
	}

	std::vector<CellPos> buildMergedDataCells()
	{
		auto cells = buildFullDataCells();
		if (cells.size() > PaddingCellCount)
		{
			cells.resize(cells.size() - PaddingCellCount);
		}
		return cells;
	}

	std::vector<CellPos> getPaddingCells()
	{
		const auto cells = buildFullDataCells();
		if (cells.size() <= PaddingCellCount)
		{
			return {};
		}
		return std::vector<CellPos>(cells.end() - PaddingCellCount, cells.end());
	}

	void fillAreaNoise(Mat& mat, const DataArea& area)
	{
		for (int row = area.top; row < area.top + area.height; ++row)
		{
			const int rowWidth = area.width - area.trimRight;
			for (int col = area.left; col < area.left + rowWidth; ++col)
			{
				fillBinaryNoiseCell(mat.at<Vec3b>(row, col));
			}
		}
	}

	void writeBytesToCells(Mat& mat, const unsigned char* info, int len, const std::vector<CellPos>& cells)
	{
		int bitIndex = 0;
		const int totalBits = len * 8;
		for (const auto& cell : cells)
		{
			if (bitIndex >= totalBits)
			{
				break;
			}
			const int byteIndex = bitIndex / 8;
			const int offset = bitIndex % 8;
			const bool bit = ((info[byteIndex] >> offset) & 1) != 0;
			mat.at<Vec3b>(cell.row, cell.col) = pixel[bit ? White : Black];
			++bitIndex;
		}
	}

	void writeHeaderField(Mat& mat, int fieldId, uint16_t value)
	{
		const int top = HeaderTop + fieldId * HeaderFieldHeight;
		for (int bit = 0; bit < HeaderFieldBits; ++bit)
		{
			const bool isWhite = ((value >> bit) & 1) != 0;
			const int left = HeaderLeft + HeaderInnerLeft + bit * HeaderBitWidth;
			for (int row = top; row < top + HeaderFieldHeight; ++row)
			{
				for (int col = left; col < left + HeaderBitWidth; ++col)
				{
					mat.at<Vec3b>(row, col) = pixel[isWhite ? White : Black];
				}
			}
		}
	}

	void drawRegionOutline(Mat& mat, const DebugRegion& region)
	{
		const int bottom = region.top + region.height - 1;
		const int right = region.left + region.width - 1;
		for (int col = region.left; col <= right; ++col)
		{
			mat.at<Vec3b>(region.top, col) = region.color;
			mat.at<Vec3b>(bottom, col) = region.color;
		}
		for (int row = region.top; row <= bottom; ++row)
		{
			mat.at<Vec3b>(row, region.left) = region.color;
			mat.at<Vec3b>(row, right) = region.color;
		}
	}

	Mat BuildLayoutPreview(const Mat& src)
	{
		Mat preview = src.clone();
		for (const auto& region : kDebugRegions)
		{
			drawRegionOutline(preview, region);
		}
		return preview;
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

	void WriteFrameImage(const Mat& logicalFrame, const char* savePath, const char* outputFormat, int frameIndex)
	{
		char fileName[128];
		const Mat output = ScaleToDisSize(logicalFrame);
		std::snprintf(fileName, sizeof(fileName), "%s\\%05d.%s", savePath, frameIndex, outputFormat);
		imwrite(fileName, output);
#ifdef Layout_DEBUG
		const Mat layoutPreview = ScaleToDisSize(BuildLayoutPreview(logicalFrame));
		std::snprintf(fileName, sizeof(fileName), "%s\\%05d_layout.%s", savePath, frameIndex, outputFormat);
		imwrite(fileName, layoutPreview);
#endif
	}

	uint16_t CalCheckCode(const unsigned char* info, int len, bool isStart, bool isEnd, uint16_t frameBase)
	{
		uint16_t ans = 0;
		const int cutlen = (len / 2) * 2;
		for (int i = 0; i < cutlen; i += 2)
		{
			ans ^= (static_cast<uint16_t>(info[i]) << 8) | info[i + 1];
		}
		if (len & 1)
		{
			ans ^= static_cast<uint16_t>(info[cutlen]) << 8;
		}
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
		for (int row = start; row < FrameSize; ++row)
		{
			for (int col = start; col < FrameSize; ++col)
			{
				if (isInsideSmallQrPoint(row, col))
				{
					continue;
				}
				mat.at<Vec3b>(row, col) = pixel[White];
			}
		}
	}

	void drawSmallQrPoint(Mat& mat)
	{
		const int center = FrameSize - SmallQrPointbias;
		const Vec3b vec3bsmall[4] =
		{
			pixel[Black],
			pixel[Black],
			pixel[White],
			pixel[Black],
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
			for (int row = 0; row < QrPointSize; ++row)
			{
				for (int col = 0; col < QrPointSize; ++col)
				{
					const int index = std::max(std::abs(row - QrPointSize / 2), std::abs(col - QrPointSize / 2));
					mat.at<Vec3b>(pos[0] + row, pos[1] + col) = vec3bBig[index];
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
		const auto mergedCells = buildMergedDataCells();
		for (const auto& cell : mergedCells)
		{
			fillBinaryNoiseCell(mat.at<Vec3b>(cell.row, cell.col));
		}
		for (const auto& cell : getPaddingCells())
		{
			mat.at<Vec3b>(cell.row, cell.col) = pixel[White];
		}
	}

	void BulidCheckCodeAndFrameNo(Mat& mat, uint16_t checkcode, uint16_t FrameNo)
	{
		writeHeaderField(mat, 1, checkcode);
		writeHeaderField(mat, 2, FrameNo);
#ifdef Code_DEBUG
		Show_Scale_Img(mat);
#endif
	}

	void BulidInfoRect(Mat& mat, const char* info, int len, int areaID)
	{
		const auto cells = buildAreaCells(kDataAreas[areaID]);
		writeBytesToCells(mat, reinterpret_cast<const unsigned char*>(info), len, cells);
#ifdef Code_DEBUG
		Show_Scale_Img(mat);
#endif
	}

	void BulidFrameFlag(Mat& mat, FrameType frameType, int tailLen)
	{
		uint16_t headerValue = 0;
		switch (frameType)
		{
		case FrameType::Start:
			headerValue = 0b0011;
			break;
		case FrameType::End:
			headerValue = 0b1100;
			break;
		case FrameType::StartAndEnd:
			headerValue = 0b1111;
			break;
		default:
			headerValue = 0;
			break;
		}
		headerValue |= static_cast<uint16_t>(tailLen) << 4;
		writeHeaderField(mat, 0, headerValue);
#ifdef Code_DEBUG
		Show_Scale_Img(mat);
#endif
	}

	Mat CodeFrame(FrameType frameType, const char* info, int tailLen, int FrameNo)
	{
		Mat codeMat(FrameSize, FrameSize, CV_8UC3, Vec3d(255, 255, 255));
		if (frameType != FrameType::End && frameType != FrameType::StartAndEnd)
		{
			tailLen = BytesPerFrame;
		}
		BulidSafeArea(codeMat);
		BulidQrPoint(codeMat);
		fillDataNoise(codeMat);

		const int checkCode = CalCheckCode(reinterpret_cast<const unsigned char*>(info), tailLen,
			frameType == FrameType::Start || frameType == FrameType::StartAndEnd,
			frameType == FrameType::End || frameType == FrameType::StartAndEnd,
			FrameNo);
		BulidFrameFlag(codeMat, frameType, tailLen);
		BulidCheckCodeAndFrameNo(codeMat, checkCode, FrameNo % 65536);

		const auto mergedCells = buildMergedDataCells();
		writeBytesToCells(codeMat, reinterpret_cast<const unsigned char*>(info), BytesPerFrame, mergedCells);
		return codeMat;
	}

	void Main(const char* info, int len, const char* savePath, const char* outputFormat, int FrameCountLimit)
	{
		int counter = 0;
		if (FrameCountLimit == 0 || len <= 0)
		{
			return;
		}
		if (len <= BytesPerFrame)
		{
			unsigned char BUF[BytesPerFrame + 5];
			std::memcpy(BUF, info, sizeof(unsigned char) * len);
			for (int i = len; i <= BytesPerFrame; ++i)
			{
				BUF[i] = std::rand() % 256;
			}
			WriteFrameImage(CodeFrame(FrameType::StartAndEnd, reinterpret_cast<char*>(BUF), len, 0), savePath, outputFormat, counter++);
		}
		else
		{
			int i = 0;
			len -= BytesPerFrame;
			Mat output = CodeFrame(FrameType::Start, info, len, 0);
			--FrameCountLimit;
			WriteFrameImage(output, savePath, outputFormat, counter++);

			while (len > 0 && FrameCountLimit > 0)
			{
				info += BytesPerFrame;
				--FrameCountLimit;
				if (len - BytesPerFrame > 0)
				{
					if (FrameCountLimit > 0)
					{
						output = CodeFrame(FrameType::Normal, info, BytesPerFrame, ++i);
					}
					else
					{
						output = CodeFrame(FrameType::End, info, BytesPerFrame, ++i);
					}
				}
				else
				{
					unsigned char BUF[BytesPerFrame + 5];
					std::memcpy(BUF, info, sizeof(unsigned char) * len);
					for (int j = len; j <= BytesPerFrame; ++j)
					{
						BUF[j] = std::rand() % 256;
					}
					output = CodeFrame(FrameType::End, reinterpret_cast<char*>(BUF), len, ++i);
				}
				len -= BytesPerFrame;
				WriteFrameImage(output, savePath, outputFormat, counter++);
			}
		}
	}
}
