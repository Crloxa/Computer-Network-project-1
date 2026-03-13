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
	constexpr int BytesPerFrame = 3968;
	constexpr int EncodedPayloadBytesPerFrame = 4477;
	constexpr int FrameSize = 144;
	constexpr int FrameOutputRate = 10;
	constexpr int FrameOutputSize = FrameSize * FrameOutputRate;
	constexpr int SafeAreaWidth = 3;
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
	constexpr int PayloadCellCount = EncodedPayloadBytesPerFrame * 4;
	constexpr int SmallQrSafetyWidth = 3;
	constexpr int CalibrationTop = 3;
	constexpr int CalibrationLeft = 37;
	constexpr int CalibrationHeight = 2;
	constexpr int CalibrationWidth = 8;
	constexpr int CalibrationBlockWidth = 2;
	constexpr int InterleaveBlockSize = 8;
	constexpr int EccDataBytes = 16;
	constexpr int EccBytes = 2;
	constexpr int EccGroupBytes = EccDataBytes + EccBytes;
	constexpr int EccGroupCount = BytesPerFrame / EccDataBytes;

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

	const std::array<int, 4> payloadColorIndex =
	{{
		Black, 1, 2, White
	}};

	const std::array<DebugRegion, 8> kDebugRegions =
	{{
		{"header", HeaderTop, HeaderLeft, HeaderHeight, HeaderWidth, Vec3b(0, 0, 255)},
		{"calibration", CalibrationTop, CalibrationLeft, CalibrationHeight, CalibrationWidth, Vec3b(255, 128, 0)},
		{"qr_top_left", 0, 0, QrPointSize, QrPointSize, Vec3b(255, 0, 0)},
		{"qr_top_right", 0, FrameSize - QrPointSize, QrPointSize, QrPointSize, Vec3b(0, 255, 0)},
		{"qr_bottom_left", FrameSize - QrPointSize, 0, QrPointSize, QrPointSize, Vec3b(0, 255, 255)},
		{"payload", SafeAreaWidth, SafeAreaWidth, FrameSize - SafeAreaWidth * 2, FrameSize - SafeAreaWidth * 2, Vec3b(255, 255, 0)},
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
		return row >= FrameSize - SafeAreaWidth || col >= FrameSize - SafeAreaWidth;
	}

	bool isInsideCornerSafetyZone(int row, int col)
	{
		const int center = FrameSize - SmallQrPointbias;
		return std::abs(row - center) <= SmallQrPointRadius + SmallQrSafetyWidth &&
			std::abs(col - center) <= SmallQrPointRadius + SmallQrSafetyWidth;
	}

	bool isInsideSafeArea(int row, int col)
	{
		return row < SafeAreaWidth || col < SafeAreaWidth ||
			row >= FrameSize - SafeAreaWidth || col >= FrameSize - SafeAreaWidth;
	}

	bool isInsideMainQrPoint(int row, int col)
	{
		return (row < QrPointSize && col < QrPointSize) ||
			(row < QrPointSize && col >= FrameSize - QrPointSize) ||
			(row >= FrameSize - QrPointSize && col < QrPointSize);
	}

	bool isInsideHeader(int row, int col)
	{
		return row >= HeaderTop && row < HeaderTop + HeaderHeight &&
			col >= HeaderLeft && col < HeaderLeft + HeaderWidth;
	}

	bool isInsideCalibrationStrip(int row, int col)
	{
		return row >= CalibrationTop && row < CalibrationTop + CalibrationHeight &&
			col >= CalibrationLeft && col < CalibrationLeft + CalibrationWidth;
	}

	bool isReservedCell(int row, int col)
	{
		if (isInsideSafeArea(row, col) || isInsideMainQrPoint(row, col) || isInsideHeader(row, col) || isInsideCalibrationStrip(row, col))
		{
			return true;
		}
		if (row >= FrameSize - CornerReserveSize && col >= FrameSize - CornerReserveSize)
		{
			return isInsideSmallQrPoint(row, col) || isInsideCornerSafetyZone(row, col);
		}
		return false;
	}

	std::vector<CellPos> BuildInterleavedPayloadCells()
	{
		std::vector<CellPos> payloadCells;
		payloadCells.reserve(FrameSize * FrameSize);
		for (int row = 0; row < FrameSize; ++row)
		{
			for (int col = 0; col < FrameSize; ++col)
			{
				if (!isReservedCell(row, col))
				{
					payloadCells.push_back({ row, col });
				}
			}
		}
		const int blockRows = (FrameSize + InterleaveBlockSize - 1) / InterleaveBlockSize;
		const int blockCols = (FrameSize + InterleaveBlockSize - 1) / InterleaveBlockSize;
		std::vector<std::vector<CellPos>> blocks(blockRows * blockCols);
		for (const auto& cell : payloadCells)
		{
			const int blockRow = cell.row / InterleaveBlockSize;
			const int blockCol = cell.col / InterleaveBlockSize;
			blocks[blockRow * blockCols + blockCol].push_back(cell);
		}

		std::vector<CellPos> interleaved;
		interleaved.reserve(payloadCells.size());
		size_t written = 0;
		while (written < payloadCells.size())
		{
			for (auto& block : blocks)
			{
				if (written >= block.size())
				{
					continue;
				}
				interleaved.push_back(block[written]);
			}
			++written;
		}
		if (interleaved.size() < PayloadCellCount)
		{
			std::abort();
		}
		interleaved.resize(PayloadCellCount);
		return interleaved;
	}

	const std::vector<CellPos>& getPayloadCells()
	{
		static const std::vector<CellPos> cells = BuildInterleavedPayloadCells();
		return cells;
	}

	void writePayloadColorCell(Mat& mat, int row, int col, unsigned char twoBits)
	{
		mat.at<Vec3b>(row, col) = pixel[payloadColorIndex[twoBits & 0x03]];
	}

	void writeBytesToColorCells(Mat& mat, const unsigned char* info, int len, const std::vector<CellPos>& cells)
	{
		int cellIndex = 0;
		const int totalCells = len * 4;
		for (const auto& cell : cells)
		{
			if (cellIndex >= totalCells)
			{
				break;
			}
			const int byteIndex = cellIndex / 4;
			const int shift = (cellIndex % 4) * 2;
			const unsigned char twoBits = static_cast<unsigned char>((info[byteIndex] >> shift) & 0x03);
			writePayloadColorCell(mat, cell.row, cell.col, twoBits);
			++cellIndex;
		}
	}

	const std::array<unsigned char, BytesPerFrame>& getScrambleMask()
	{
		static const std::array<unsigned char, BytesPerFrame> mask = []()
		{
			std::array<unsigned char, BytesPerFrame> values = {};
			uint16_t lfsr = 0xACE1u;
			for (int i = 0; i < BytesPerFrame; ++i)
			{
				unsigned char byte = 0;
				for (int bit = 0; bit < 8; ++bit)
				{
					byte |= static_cast<unsigned char>(lfsr & 1u) << bit;
					const uint16_t feedback = static_cast<uint16_t>((lfsr ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u);
					lfsr = static_cast<uint16_t>((lfsr >> 1) | (feedback << 15));
				}
				values[i] = byte;
			}
			return values;
		}();
		return mask;
	}

	void ScramblePayload(unsigned char* dst, const unsigned char* src, int len)
	{
		const auto& mask = getScrambleMask();
		const int copyLen = std::min(len, BytesPerFrame);
		for (int i = 0; i < copyLen; ++i)
		{
			dst[i] = static_cast<unsigned char>(src[i] ^ mask[i]);
		}
		for (int i = copyLen; i < BytesPerFrame; ++i)
		{
			dst[i] = 0;
		}
	}

	void EncodePayloadWithEcc(unsigned char* dst, const unsigned char* src, int rawLen)
	{
		std::memset(dst, 0, EncodedPayloadBytesPerFrame);
		const int encodeLen = std::min(rawLen, BytesPerFrame);
		for (int group = 0; group < EccGroupCount; ++group)
		{
			const int srcBase = group * EccDataBytes;
			const int dstBase = group * EccGroupBytes;
			if (srcBase >= encodeLen || dstBase + EccGroupBytes > EncodedPayloadBytesPerFrame)
			{
				break;
			}
			unsigned char ecc0 = 0;
			unsigned char ecc1 = 0;
			for (int i = 0; i < EccDataBytes; ++i)
			{
				const unsigned char value = src[srcBase + i];
				dst[dstBase + i] = value;
				ecc0 ^= value;
				ecc1 ^= static_cast<unsigned char>((i & 1) == 0 ? value : ((value << 1) | (value >> 7)));
			}
			dst[dstBase + EccDataBytes] = ecc0;
			dst[dstBase + EccDataBytes + 1] = ecc1;
		}
	}

	void PackFramePayload(unsigned char* encoded, const unsigned char* raw, int rawLen)
	{
		unsigned char scrambled[BytesPerFrame];
		ScramblePayload(scrambled, raw, rawLen);
		EncodePayloadWithEcc(encoded, scrambled, BytesPerFrame);
	}

	void BulidCalibrationStrip(Mat& mat)
	{
		for (int block = 0; block < 4; ++block)
		{
			const Vec3b& blockColor = pixel[payloadColorIndex[block]];
			const int left = CalibrationLeft + block * CalibrationBlockWidth;
			for (int row = CalibrationTop; row < CalibrationTop + CalibrationHeight; ++row)
			{
				for (int col = left; col < left + CalibrationBlockWidth; ++col)
				{
					mat.at<Vec3b>(row, col) = blockColor;
				}
			}
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
		const int cutlen = (EncodedPayloadBytesPerFrame / 2) * 2;
		for (int i = 0; i < cutlen; i += 2)
		{
			ans ^= (static_cast<uint16_t>(info[i]) << 8) | info[i + 1];
		}
		if (EncodedPayloadBytesPerFrame & 1)
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
		(void)areaID;
		writeBytesToColorCells(mat, reinterpret_cast<const unsigned char*>(info), len, getPayloadCells());
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
		BulidCalibrationStrip(codeMat);

		const int checkCode = CalCheckCode(reinterpret_cast<const unsigned char*>(info), tailLen,
			frameType == FrameType::Start || frameType == FrameType::StartAndEnd,
			frameType == FrameType::End || frameType == FrameType::StartAndEnd,
			FrameNo);
		BulidFrameFlag(codeMat, frameType, tailLen);
		BulidCheckCodeAndFrameNo(codeMat, checkCode, FrameNo % 65536);

		writeBytesToColorCells(codeMat, reinterpret_cast<const unsigned char*>(info), EncodedPayloadBytesPerFrame, getPayloadCells());
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
			unsigned char BUF[EncodedPayloadBytesPerFrame + 5];
			PackFramePayload(BUF, reinterpret_cast<const unsigned char*>(info), len);
			WriteFrameImage(CodeFrame(FrameType::StartAndEnd, reinterpret_cast<char*>(BUF), len, 0), savePath, outputFormat, counter++);
		}
		else
		{
			int i = 0;
			len -= BytesPerFrame;
			unsigned char BUF[EncodedPayloadBytesPerFrame + 5];
			PackFramePayload(BUF, reinterpret_cast<const unsigned char*>(info), BytesPerFrame);
			Mat output = CodeFrame(FrameType::Start, reinterpret_cast<char*>(BUF), len, 0);
			--FrameCountLimit;
			WriteFrameImage(output, savePath, outputFormat, counter++);

			while (len > 0 && FrameCountLimit > 0)
			{
				info += BytesPerFrame;
				--FrameCountLimit;
				const bool hasMoreFullFrames = len - BytesPerFrame > 0;
				const int currentLen = hasMoreFullFrames ? BytesPerFrame : len;
				const FrameType frameType = hasMoreFullFrames ?
					(FrameCountLimit > 0 ? FrameType::Normal : FrameType::End) :
					FrameType::End;
				PackFramePayload(BUF, reinterpret_cast<const unsigned char*>(info), currentLen);
				output = CodeFrame(frameType, reinterpret_cast<char*>(BUF), currentLen, ++i);
				len -= BytesPerFrame;
				WriteFrameImage(output, savePath, outputFormat, counter++);
			}
		}
	}
}
