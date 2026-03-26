#include "code.h"
#include "protocol.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
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
	using Protocol::BaseCornerReserveSize;
	using Protocol::BaseFrameSize;
	using Protocol::BasePaddingCellCount;
	using Protocol::BaseQrPointSize;
	using Protocol::BaseSafeAreaWidth;
	using Protocol::BaseSmallQrPointBias;
	using Protocol::BaseSmallQrPointRadius;
	using Protocol::BytesPerFrame;
	using Protocol::CornerReserveSize;
	using Protocol::DataArea;
	using Protocol::FrameOutputSize;
	using Protocol::FrameSize;
	using Protocol::HeaderCheckBits;
	using Protocol::HeaderFieldBits;
	using Protocol::HeaderFieldHeight;
	using Protocol::HeaderFrameBits;
	using Protocol::HeaderHeight;
	using Protocol::HeaderLeft;
	using Protocol::HeaderMetaBits;
	using Protocol::HeaderTop;
	using Protocol::HeaderWidth;
	using Protocol::kBaseDataAreas;
	using Protocol::LayoutScale;

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

	const std::array<DebugRegion, 10> kDebugRegions =
	{{
		{"header", HeaderTop, HeaderLeft, HeaderHeight, HeaderWidth, Vec3b(0, 0, 255)},
		{"data1", Protocol::scaleCoord(kBaseDataAreas[0].top), Protocol::scaleCoord(kBaseDataAreas[0].left), Protocol::scaleCoord(kBaseDataAreas[0].height), Protocol::scaleCoord(kBaseDataAreas[0].width), Vec3b(255, 0, 0)},
		{"data1_lower", Protocol::scaleCoord(kBaseDataAreas[1].top), Protocol::scaleCoord(kBaseDataAreas[1].left), Protocol::scaleCoord(kBaseDataAreas[1].height), Protocol::scaleCoord(kBaseDataAreas[1].width), Vec3b(255, 0, 0)},
		{"data2", Protocol::scaleCoord(kBaseDataAreas[2].top), Protocol::scaleCoord(kBaseDataAreas[2].left), Protocol::scaleCoord(kBaseDataAreas[2].height), Protocol::scaleCoord(kBaseDataAreas[2].width), Vec3b(0, 255, 0)},
		{"data4", Protocol::scaleCoord(kBaseDataAreas[3].top), Protocol::scaleCoord(kBaseDataAreas[3].left), Protocol::scaleCoord(kBaseDataAreas[3].height), Protocol::scaleCoord(kBaseDataAreas[3].width), Vec3b(0, 255, 255)},
		{"data3", Protocol::scaleCoord(kBaseDataAreas[4].top), Protocol::scaleCoord(kBaseDataAreas[4].left), Protocol::scaleCoord(kBaseDataAreas[4].height), Protocol::scaleCoord(kBaseDataAreas[4].width), Vec3b(255, 255, 0)},
		{"corner_data_v", Protocol::scaleCoord(112), Protocol::scaleCoord(112), Protocol::scaleCoord(18), Protocol::scaleCoord(9), Vec3b(0, 200, 255)},
		{"corner_data_h", Protocol::scaleCoord(112), Protocol::scaleCoord(121), Protocol::scaleCoord(9), Protocol::scaleCoord(9), Vec3b(0, 200, 255)},
		{"corner", FrameSize - CornerReserveSize, FrameSize - CornerReserveSize, CornerReserveSize, CornerReserveSize, Vec3b(255, 0, 255)},
		{"small_qr", Protocol::scaleCoord(BaseFrameSize - BaseSmallQrPointBias - BaseSmallQrPointRadius), Protocol::scaleCoord(BaseFrameSize - BaseSmallQrPointBias - BaseSmallQrPointRadius), Protocol::scaleCoord(BaseSmallQrPointRadius * 2 + 1), Protocol::scaleCoord(BaseSmallQrPointRadius * 2 + 1), Vec3b(0, 128, 255)}
	}};

	bool isInsideBaseSmallQrPoint(int row, int col)
	{
		const int center = BaseFrameSize - BaseSmallQrPointBias;
		return std::abs(row - center) <= BaseSmallQrPointRadius && std::abs(col - center) <= BaseSmallQrPointRadius;
	}

	bool isInsideBaseCornerQuietZone(int row, int col)
	{
		return row >= BaseFrameSize - 3 || col >= BaseFrameSize - 3;
	}

	bool isInsideBaseCornerSafetyZone(int row, int col)
	{
		const int center = BaseFrameSize - BaseSmallQrPointBias;
		return std::abs(row - center) <= BaseSmallQrPointRadius + 2 && std::abs(col - center) <= BaseSmallQrPointRadius + 2;
	}

	void fillScaledBaseCell(Mat& mat, int baseRow, int baseCol, const Vec3b& value)
	{
		const int rowStart = Protocol::scaleCoord(baseRow);
		const int colStart = Protocol::scaleCoord(baseCol);
		for (int row = rowStart; row < rowStart + LayoutScale; ++row)
		{
			for (int col = colStart; col < colStart + LayoutScale; ++col)
			{
				mat.at<Vec3b>(row, col) = value;
			}
		}
	}

	void fillBinaryNoiseCell(Vec3b& cell)
	{
		cell = pixel[(std::rand() & 1) ? White : Black];
	}

	std::vector<CellPos> buildBaseAreaCells(const DataArea& area)
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

	void appendScaledSubcells(std::vector<CellPos>& cells, const CellPos& baseCell)
	{
		const int rowStart = Protocol::scaleCoord(baseCell.row);
		const int colStart = Protocol::scaleCoord(baseCell.col);
		for (int row = rowStart; row < rowStart + LayoutScale; ++row)
		{
			for (int col = colStart; col < colStart + LayoutScale; ++col)
			{
				cells.push_back({ row, col });
			}
		}
	}

	std::vector<CellPos> buildCornerDataCells()
	{
		std::vector<CellPos> cells;
		for (int row = BaseFrameSize - BaseCornerReserveSize; row < BaseFrameSize; ++row)
		{
			for (int col = BaseFrameSize - BaseCornerReserveSize; col < BaseFrameSize; ++col)
			{
				if (isInsideBaseCornerQuietZone(row, col))
				{
					continue;
				}
				if (isInsideBaseCornerSafetyZone(row, col))
				{
					continue;
				}
				appendScaledSubcells(cells, { row, col });
			}
		}
		return cells;
	}

	std::vector<CellPos> buildHeaderPayloadCells()
	{
		std::vector<CellPos> cells;
		const std::array<int, 3> usedBits = { HeaderMetaBits, HeaderCheckBits, HeaderFrameBits };
		for (int fieldId = 0; fieldId < static_cast<int>(usedBits.size()); ++fieldId)
		{
			const int top = HeaderTop + fieldId * HeaderFieldHeight;
			for (int row = top; row < top + HeaderFieldHeight; ++row)
			{
				for (int col = HeaderLeft + usedBits[fieldId]; col < HeaderLeft + HeaderWidth; ++col)
				{
					cells.push_back({ row, col });
				}
			}
		}
		return cells;
	}

	std::vector<CellPos> buildFullDataCells()
	{
		std::vector<CellPos> cells;
		const auto headerCells = buildHeaderPayloadCells();
		cells.insert(cells.end(), headerCells.begin(), headerCells.end());
		for (const auto& area : kBaseDataAreas)
		{
			for (const auto& baseCell : buildBaseAreaCells(area))
			{
				appendScaledSubcells(cells, baseCell);
			}
		}
		const auto cornerCells = buildCornerDataCells();
		cells.insert(cells.end(), cornerCells.begin(), cornerCells.end());
		return cells;
	}

	std::vector<CellPos> buildMergedDataCells()
	{
		auto cells = buildFullDataCells();
		if (cells.size() > Protocol::PaddingCellCount)
		{
			cells.resize(cells.size() - Protocol::PaddingCellCount);
		}
		return cells;
	}

	std::vector<CellPos> getPaddingCells()
	{
		const auto cells = buildFullDataCells();
		if (cells.size() <= Protocol::PaddingCellCount)
		{
			return {};
		}
		return std::vector<CellPos>(cells.end() - Protocol::PaddingCellCount, cells.end());
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

	unsigned char payloadWhiteningMask(uint16_t frameNo, int byteIndex)
	{
		uint32_t value = static_cast<uint32_t>(frameNo) * 0x9E3779B1u + static_cast<uint32_t>(byteIndex);
		value ^= value >> 16;
		value *= 0x7FEB352Du;
		value ^= value >> 15;
		value *= 0x846CA68Bu;
		value ^= value >> 16;
		return static_cast<unsigned char>(value & 0xFFu);
	}

	std::vector<unsigned char> whitenPayload(const unsigned char* info, int len, uint16_t frameNo)
	{
		std::vector<unsigned char> whitened(len);
		for (int i = 0; i < len; ++i)
		{
			whitened[i] = static_cast<unsigned char>(info[i] ^ payloadWhiteningMask(frameNo, i));
		}
		return whitened;
	}

	void writeHeaderField(Mat& mat, int fieldId, uint32_t value, int bitCount)
	{
		const int top = HeaderTop + fieldId * HeaderFieldHeight;
		for (int bit = 0; bit < bitCount && bit < HeaderFieldBits; ++bit)
		{
			const bool isWhite = ((value >> bit) & 1u) != 0;
			const int left = HeaderLeft + bit;
			for (int row = top; row < top + HeaderFieldHeight; ++row)
			{
				mat.at<Vec3b>(row, left) = pixel[isWhite ? White : Black];
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
		Mat dis;
		resize(src, dis, Size(FrameOutputSize, FrameOutputSize), 0.0, 0.0, INTER_NEAREST);
		return dis;
	}

	void WriteFrameImage(const Mat& logicalFrame, const char* savePath, const char* outputFormat, int frameIndex)
	{
		const Mat output = ScaleToDisSize(logicalFrame);
		const auto framePath =
			(std::filesystem::path(savePath) / (cv::format("%05d.%s", frameIndex, outputFormat))).string();
		imwrite(framePath, output);
#ifdef Layout_DEBUG
		const Mat layoutPreview = ScaleToDisSize(BuildLayoutPreview(logicalFrame));
		const auto layoutPath =
			(std::filesystem::path(savePath) / (cv::format("%05d_layout.%s", frameIndex, outputFormat))).string();
		imwrite(layoutPath, layoutPreview);
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
		for (int i = 0; i < BaseFrameSize; ++i)
		{
			for (int j = 0; j < BaseSafeAreaWidth; ++j)
			{
				fillScaledBaseCell(mat, i, j, pixel[White]);
				fillScaledBaseCell(mat, i, BaseFrameSize - BaseSafeAreaWidth + j, pixel[White]);
				fillScaledBaseCell(mat, j, i, pixel[White]);
				fillScaledBaseCell(mat, BaseFrameSize - BaseSafeAreaWidth + j, i, pixel[White]);
			}
		}
#ifdef Code_DEBUG
		Show_Scale_Img(mat);
#endif
	}

	void fillCornerNoiseArea(Mat& mat)
	{
		const int start = BaseFrameSize - BaseCornerReserveSize;
		for (int row = start; row < BaseFrameSize; ++row)
		{
			for (int col = start; col < BaseFrameSize; ++col)
			{
				if (isInsideBaseSmallQrPoint(row, col))
				{
					continue;
				}
				fillScaledBaseCell(mat, row, col, pixel[White]);
			}
		}
	}

	void drawSmallQrPoint(Mat& mat)
	{
		const Vec3b vec3bsmall[4] =
		{
			pixel[Black],
			pixel[Black],
			pixel[White],
			pixel[Black],
		};
		const int center = BaseFrameSize - BaseSmallQrPointBias;
		for (int row = -BaseSmallQrPointRadius; row <= BaseSmallQrPointRadius; ++row)
		{
			for (int col = -BaseSmallQrPointRadius; col <= BaseSmallQrPointRadius; ++col)
			{
				const int index = std::max(std::abs(row), std::abs(col));
				fillScaledBaseCell(mat, center + row, center + col, vec3bsmall[index]);
			}
		}
	}

	void BulidQrPoint(Mat& mat)
	{
		const std::array<std::array<int, 2>, 3> pointPos =
		{{
			{0, 0},
			{0, BaseFrameSize - BaseQrPointSize},
			{BaseFrameSize - BaseQrPointSize, 0}
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
			for (int row = 0; row < BaseQrPointSize; ++row)
			{
				for (int col = 0; col < BaseQrPointSize; ++col)
				{
					const int index = std::max(std::abs(row - BaseQrPointSize / 2), std::abs(col - BaseQrPointSize / 2));
					fillScaledBaseCell(mat, pos[0] + row, pos[1] + col, vec3bBig[index]);
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
		writeHeaderField(mat, 1, checkcode, HeaderCheckBits);
		writeHeaderField(mat, 2, FrameNo, HeaderFrameBits);
#ifdef Code_DEBUG
		Show_Scale_Img(mat);
#endif
	}

	void BulidInfoRect(Mat& mat, const char* info, int len, int areaID)
	{
		std::vector<CellPos> cells;
		for (const auto& baseCell : buildBaseAreaCells(kBaseDataAreas[areaID]))
		{
			appendScaledSubcells(cells, baseCell);
		}
		writeBytesToCells(mat, reinterpret_cast<const unsigned char*>(info), len, cells);
#ifdef Code_DEBUG
		Show_Scale_Img(mat);
#endif
	}

	void BulidFrameFlag(Mat& mat, FrameType frameType, int tailLen)
	{
		uint32_t headerValue = 0;
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
		headerValue |= static_cast<uint32_t>(tailLen) << 4;
		writeHeaderField(mat, 0, headerValue, HeaderMetaBits);
#ifdef Code_DEBUG
		Show_Scale_Img(mat);
#endif
	}

	Mat CodeFrame(FrameType frameType, const char* info, int tailLen, int FrameNo)
	{
		Mat codeMat(FrameSize, FrameSize, CV_8UC3, Scalar(255, 255, 255));
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
		const auto whitenedPayload =
			whitenPayload(reinterpret_cast<const unsigned char*>(info), BytesPerFrame, static_cast<uint16_t>(FrameNo % 65536));
		writeBytesToCells(codeMat, whitenedPayload.data(), BytesPerFrame, mergedCells);
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
