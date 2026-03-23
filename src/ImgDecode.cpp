// This file implements decoding for the logical code frame.
#include "ImgDecode.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "code.h"
#include "pic.h"

namespace ImageDecode
{
	enum color
	{
		Black = 0,
		White = 7
	};

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

	enum class FrameType
	{
		Start = 0,
		End = 1,
		StartAndEnd = 2,
		Normal = 3
	};

	constexpr int SmallQrPointRadius = 3;
	constexpr int CornerReserveSize = 21;
	constexpr int HeaderLeft = 21;
	constexpr int HeaderTop = 3;
	constexpr int HeaderFieldBits = 16;
	constexpr int HeaderWidth = 16;
	constexpr int TopDataLeft = HeaderLeft + HeaderWidth;
	constexpr int TopDataWidth = 75;
	constexpr int DataAreaCount = 5;
	constexpr int PaddingCellCount = 4;
	constexpr int BitsPerCell = 3;

	const std::array<DataArea, DataAreaCount> kDataAreas =
	{{
		{3, TopDataLeft, 3, TopDataWidth, 0},
		{6, 21, 15, 91, 0},
		{21, 3, 88, 127, 0},
		{109, 3, 3, 127, 0},
		{112, 21, 18, 91, 0}
	}};

	bool isWhiteCell(const Vec3b& cell)
	{
		return cell[0] + cell[1] + cell[2] >= 384;
	}

	int readCellValue(const Vec3b& cell)
	{
		const int b = cell[0];
		const int g = cell[1];
		const int r = cell[2];
		return ((b >= 128) ? 4 : 0)
			| ((g >= 128) ? 2 : 0)
			| ((r >= 128) ? 1 : 0);
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

	std::vector<CellPos> buildMergedDataCells()
	{
		std::vector<CellPos> cells;
		for (const auto& area : kDataAreas)
		{
			const auto areaCells = buildAreaCells(area);
			cells.insert(cells.end(), areaCells.begin(), areaCells.end());
		}
		const auto cornerCells = buildCornerDataCells();
		cells.insert(cells.end(), cornerCells.begin(), cornerCells.end());
		if (cells.size() > PaddingCellCount)
		{
			cells.resize(cells.size() - PaddingCellCount);
		}
		return cells;
	}

	std::vector<CellPos> getPaddingCells()
	{
		std::vector<CellPos> cells;
		for (const auto& area : kDataAreas)
		{
			const auto areaCells = buildAreaCells(area);
			cells.insert(cells.end(), areaCells.begin(), areaCells.end());
		}
		const auto cornerCells = buildCornerDataCells();
		cells.insert(cells.end(), cornerCells.begin(), cornerCells.end());
		if (cells.size() <= PaddingCellCount)
		{
			return {};
		}
		return std::vector<CellPos>(cells.end() - PaddingCellCount, cells.end());
	}

	int readTailLenHighBit(const Mat& mat)
	{
		int whiteCount = 0;
		for (const auto& cell : getPaddingCells())
		{
			if (isWhiteCell(mat.at<Vec3b>(cell.row, cell.col)))
			{
				++whiteCount;
			}
		}
		return whiteCount * 2 >= PaddingCellCount ? 1 : 0;
	}

	uint16_t readHeaderField(const Mat& mat, int fieldId)
	{
		uint16_t value = 0;
		const int row = HeaderTop + fieldId;
		for (int bit = 0; bit < HeaderFieldBits; ++bit)
		{
			if (isWhiteCell(mat.at<Vec3b>(row, HeaderLeft + bit)))
			{
				value |= static_cast<uint16_t>(1u << bit);
			}
		}
		return value;
	}

	FrameType parseFrameType(uint16_t headerValue, bool& isStart, bool& isEnd)
	{
		const uint16_t flagBits = headerValue & 0xF;
		switch (flagBits)
		{
		case 0b0011:
			isStart = true;
			isEnd = false;
			return FrameType::Start;
		case 0b1100:
			isStart = false;
			isEnd = true;
			return FrameType::End;
		case 0b1111:
			isStart = true;
			isEnd = true;
			return FrameType::StartAndEnd;
		default:
			isStart = false;
			isEnd = false;
			return FrameType::Normal;
		}
	}

	void readPayload(const Mat& mat, std::vector<unsigned char>& info)
	{
		const auto cells = buildMergedDataCells();
		info.assign(BytesPerFrame, 0);
		const int totalCells = (BytesPerFrame * 8 + BitsPerCell - 1) / BitsPerCell;
		const int mask = (1 << BitsPerCell) - 1;
		for (int cellIndex = 0;
			cellIndex < totalCells && cellIndex < static_cast<int>(cells.size());
			++cellIndex)
		{
			const int bitIndex = cellIndex * BitsPerCell;
			const int byteIndex = bitIndex / 8;
			const int offset = bitIndex % 8;
			const int value = readCellValue(mat.at<Vec3b>(cells[cellIndex].row, cells[cellIndex].col));
			info[byteIndex] |= static_cast<unsigned char>((value & mask) << offset);
			if (offset + BitsPerCell > 8 && byteIndex + 1 < BytesPerFrame)
			{
				info[byteIndex + 1] |= static_cast<unsigned char>(value >> (8 - offset));
			}
		}
	}

	bool hasLegalSize(const Mat& mat)
	{
		return mat.rows == FrameSize && mat.cols == FrameSize && mat.type() == CV_8UC3;
	}

	bool Main(Mat& mat, ImageInfo& imageInfo)
	{
		imageInfo.Info.clear();
		imageInfo.CheckCode = 0;
		imageInfo.FrameBase = 0;
		imageInfo.IsStart = false;
		imageInfo.IsEnd = false;

		if (!hasLegalSize(mat))
		{
			return true;
		}

		const uint16_t headerValue = readHeaderField(mat, 0);
		parseFrameType(headerValue, imageInfo.IsStart, imageInfo.IsEnd);
		const int codeLenLow = headerValue >> 4;
		const int codeLen = imageInfo.IsEnd
			? (codeLenLow | (readTailLenHighBit(mat) << 12))
			: BytesPerFrame;
		if (codeLen > BytesPerFrame)
		{
			return true;
		}

		imageInfo.CheckCode = readHeaderField(mat, 1);
		imageInfo.FrameBase = readHeaderField(mat, 2);

		std::vector<unsigned char> payload;
		readPayload(mat, payload);
		payload.resize(codeLen);
		imageInfo.Info.swap(payload);

		return imageInfo.CheckCode != Code::CalCheckCode(
			imageInfo.Info.data(),
			codeLen,
			imageInfo.IsStart,
			imageInfo.IsEnd,
			imageInfo.FrameBase
		);
	}
}
