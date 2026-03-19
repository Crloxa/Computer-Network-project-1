// This file implements decoding for the logical code frame.
#include "ImgDecode.h"

#include <algorithm>
#include <array>
#include <vector>

#include "code.h"
#include "pic.h"

namespace ImageDecode
{
	// 协议几何常量、DataArea / CellPos / FrameType 及单元格构建工具均来自共享头文件
	using namespace FrameLayout;

	enum color
	{
		Black = 0,
		White = 7
	};

	bool isWhiteCell(const Vec3b& cell)
	{
		return cell[0] + cell[1] + cell[2] >= 384;
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
		parseFrameType(headerValue, imageInfo.IsStart, imageInfo.IsEnd);
		const int codeLen = headerValue >> 4;
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
