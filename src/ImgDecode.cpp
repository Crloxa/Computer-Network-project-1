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

	struct HeaderInfo
	{
		int codeLen;
		bool isStart;
		bool isEnd;
		bool valid;
	};

	constexpr int SmallQrPointRadius = 6; // 2x main-branch radius (3 × 2)
	constexpr int CornerReserveSize = 42;
	constexpr int SmallQrPointStart = FrameSize - SmallQrPointbias - SmallQrPointRadius;
	constexpr int SmallQrPointEnd = FrameSize - SmallQrPointbias + SmallQrPointRadius + 1;
	constexpr int HeaderLeft = 42;
	constexpr int HeaderTop = 6;
	constexpr int HeaderFieldBits = 16;
	constexpr int HeaderWidth = 16;
	constexpr int TopDataLeft = HeaderLeft + HeaderWidth;
	constexpr int TopDataWidth = 166;
	constexpr int DataAreaCount = 5;
	constexpr int PaddingCellCount = 2;

	const std::array<DataArea, DataAreaCount> kDataAreas =
	{ {
		{6, 58, 3, 166, 0},
		{9, 42, 33, 182, 0},
		{42, 5, 179, 256, 0},
		{221, 5, 3, 256, 0},
		{224, 42, 37, 182, 0}
	} };

	bool isWhiteCell(const Vec3b& cell)
	{
		return cell[0] + cell[1] + cell[2] >= 384;
	}

	bool isInsideCornerQuietZone(int row, int col)
	{
		return row >= (SmallQrPointEnd + 1) || col >= (SmallQrPointEnd + 1);
	}

	bool isInsideCornerSafetyZone(int row, int col)
	{
		const int safetyStart = SmallQrPointStart - 4;
		const int safetyEnd = SmallQrPointEnd + 4;
		return row >= safetyStart && row <= safetyEnd && col >= safetyStart && col <= safetyEnd;
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

	FrameType parseFrameType3BitFlags(uint16_t headerValue, bool& isStart, bool& isEnd)
	{
		const uint16_t flagBits = headerValue & 0x7;
		switch (flagBits)
		{
		case 0b001:
			isStart = true;
			isEnd = false;
			return FrameType::Start;
		case 0b010:
			isStart = false;
			isEnd = true;
			return FrameType::End;
		case 0b011:
			isStart = true;
			isEnd = true;
			return FrameType::StartAndEnd;
		default:
			isStart = false;
			isEnd = false;
			return FrameType::Normal;
		}
	}

	FrameType parseFrameTypeLegacy4BitFlags(uint16_t headerValue, bool& isStart, bool& isEnd)
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

	HeaderInfo parseHeader3BitFlags(uint16_t headerValue)
	{
		HeaderInfo info{};
		parseFrameType3BitFlags(headerValue, info.isStart, info.isEnd);
		info.codeLen = headerValue >> 3;
		info.valid = info.codeLen >= 0 && info.codeLen <= BytesPerFrame;
		return info;
	}

	HeaderInfo parseHeaderLegacy4BitFlags(uint16_t headerValue)
	{
		HeaderInfo info{};
		parseFrameTypeLegacy4BitFlags(headerValue, info.isStart, info.isEnd);
		info.codeLen = headerValue >> 4;
		info.valid = info.codeLen >= 0 && info.codeLen <= BytesPerFrame;
		return info;
	}

	void readPayload(const Mat& mat, std::vector<unsigned char>& info)
	{
		const auto cells = buildMergedDataCells();
		info.assign(BytesPerFrame, 0);
		for (int bitIndex = 0; bitIndex < BytesPerFrame * 8 && bitIndex < static_cast<int>(cells.size()); ++bitIndex)
		{
			if (isWhiteCell(mat.at<Vec3b>(cells[bitIndex].row, cells[bitIndex].col)))
			{
				const int byteIndex = bitIndex / 8;
				const int offset = bitIndex % 8;
				info[byteIndex] |= static_cast<unsigned char>(1u << offset);
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
		imageInfo.CheckCode = readHeaderField(mat, 1);
		imageInfo.FrameBase = readHeaderField(mat, 2);

		std::vector<unsigned char> payload;
		readPayload(mat, payload);
		const HeaderInfo header3BitFlags = parseHeader3BitFlags(headerValue);
		const HeaderInfo headerLegacy4BitFlags = parseHeaderLegacy4BitFlags(headerValue);
		const HeaderInfo* selected = nullptr;

		if (header3BitFlags.valid)
		{
			const uint16_t check3BitFlags = Code::CalCheckCode(
				payload.data(),
				header3BitFlags.codeLen,
				header3BitFlags.isStart,
				header3BitFlags.isEnd,
				imageInfo.FrameBase
			);
			if (check3BitFlags == imageInfo.CheckCode)
			{
				selected = &header3BitFlags;
			}
		}
		if (selected == nullptr && headerLegacy4BitFlags.valid)
		{
			const uint16_t checkLegacy4BitFlags = Code::CalCheckCode(
				payload.data(),
				headerLegacy4BitFlags.codeLen,
				headerLegacy4BitFlags.isStart,
				headerLegacy4BitFlags.isEnd,
				imageInfo.FrameBase
			);
			if (checkLegacy4BitFlags == imageInfo.CheckCode)
			{
				selected = &headerLegacy4BitFlags;
			}
		}
		if (selected == nullptr)
		{
			return true;
		}

		imageInfo.IsStart = selected->isStart;
		imageInfo.IsEnd = selected->isEnd;
		payload.resize(selected->codeLen);
		imageInfo.Info.swap(payload);
		return false;
	}
}
