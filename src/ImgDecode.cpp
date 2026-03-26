// This file implements decoding for the logical code frame.
#include "ImgDecode.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "code.h"
#include "pic.h"

namespace ImageDecode
{
	using Protocol::BaseCornerReserveSize;
	using Protocol::BaseFrameSize;
	using Protocol::BaseSmallQrPointBias;
	using Protocol::BaseSmallQrPointRadius;
	using Protocol::DataArea;
	using Protocol::HeaderCheckBits;
	using Protocol::HeaderFieldHeight;
	using Protocol::HeaderFrameBits;
	using Protocol::HeaderLeft;
	using Protocol::HeaderMetaBits;
	using Protocol::HeaderTop;
	using Protocol::kBaseDataAreas;
	using Protocol::LayoutScale;

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

	bool isWhiteCell(const Vec3b& cell)
	{
		return cell[0] + cell[1] + cell[2] >= 384;
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
				for (int col = HeaderLeft + usedBits[fieldId]; col < HeaderLeft + Protocol::HeaderWidth; ++col)
				{
					cells.push_back({ row, col });
				}
			}
		}
		return cells;
	}

	std::vector<CellPos> buildMergedDataCells()
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
		if (cells.size() > Protocol::PaddingCellCount)
		{
			cells.resize(cells.size() - Protocol::PaddingCellCount);
		}
		return cells;
	}

	bool isWhiteHeaderBit(const Mat& mat, int fieldId, int bit)
	{
		const int top = HeaderTop + fieldId * HeaderFieldHeight;
		const int col = HeaderLeft + bit;
		int whiteCount = 0;
		for (int row = top; row < top + HeaderFieldHeight; ++row)
		{
			if (isWhiteCell(mat.at<Vec3b>(row, col)))
			{
				++whiteCount;
			}
		}
		return whiteCount * 2 >= HeaderFieldHeight;
	}

	uint32_t readHeaderField(const Mat& mat, int fieldId, int bitCount)
	{
		uint32_t value = 0;
		for (int bit = 0; bit < bitCount; ++bit)
		{
			if (isWhiteHeaderBit(mat, fieldId, bit))
			{
				value |= static_cast<uint32_t>(1u << bit);
			}
		}
		return value;
	}

	FrameType parseFrameType(uint32_t headerValue, bool& isStart, bool& isEnd)
	{
		const uint32_t flagBits = headerValue & 0xF;
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

	void unwhitenPayload(std::vector<unsigned char>& info, uint16_t frameNo)
	{
		for (int i = 0; i < static_cast<int>(info.size()); ++i)
		{
			info[i] = static_cast<unsigned char>(info[i] ^ payloadWhiteningMask(frameNo, i));
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

		const uint32_t headerValue = readHeaderField(mat, 0, HeaderMetaBits);
		parseFrameType(headerValue, imageInfo.IsStart, imageInfo.IsEnd);
		const int codeLen = static_cast<int>(headerValue >> 4);
		if (codeLen > BytesPerFrame)
		{
			return true;
		}

		imageInfo.CheckCode = static_cast<uint16_t>(readHeaderField(mat, 1, HeaderCheckBits));
		imageInfo.FrameBase = static_cast<uint16_t>(readHeaderField(mat, 2, HeaderFrameBits));

		std::vector<unsigned char> payload;
		readPayload(mat, payload);
		unwhitenPayload(payload, imageInfo.FrameBase);
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
