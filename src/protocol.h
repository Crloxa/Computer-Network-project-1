#pragma once

#include <array>

namespace Protocol
{
	struct DataArea
	{
		int top;
		int left;
		int height;
		int width;
		int trimRight;
	};

	constexpr int BaseFrameSize = 133;
	constexpr int LayoutScale = 4;
	constexpr int FrameSize = BaseFrameSize * LayoutScale;
	constexpr int FrameOutputRate = 10;
	constexpr int FrameOutputSize = FrameSize * FrameOutputRate;

	constexpr int BaseBytesPerFrame = 1878;

	constexpr int BaseSafeAreaWidth = 2;
	constexpr int SafeAreaWidth = BaseSafeAreaWidth * LayoutScale;

	constexpr int BaseQrPointSize = 21;
	constexpr int QrPointSize = BaseQrPointSize * LayoutScale;

	constexpr int BaseSmallQrPointBias = 7;
	constexpr int SmallQrPointBias = BaseSmallQrPointBias * LayoutScale;

	constexpr int BaseSmallQrPointRadius = 3;
	constexpr int BaseCornerReserveSize = 21;
	constexpr int CornerReserveSize = BaseCornerReserveSize * LayoutScale;

	constexpr int BaseHeaderHeight = 3;
	constexpr int BaseHeaderWidth = 16;
	constexpr int BaseHeaderLeft = 21;
	constexpr int BaseHeaderTop = 3;

	constexpr int HeaderHeight = BaseHeaderHeight * LayoutScale;
	constexpr int HeaderWidth = BaseHeaderWidth * LayoutScale;
	constexpr int HeaderLeft = BaseHeaderLeft * LayoutScale;
	constexpr int HeaderTop = BaseHeaderTop * LayoutScale;
	constexpr int HeaderFieldHeight = LayoutScale;
	constexpr int HeaderFieldBits = HeaderWidth;

	constexpr int HeaderMetaBits = 20;
	constexpr int HeaderCheckBits = 16;
	constexpr int HeaderFrameBits = 16;
	constexpr int HeaderPayloadCells =
		HeaderHeight * HeaderWidth - HeaderFieldHeight * (HeaderMetaBits + HeaderCheckBits + HeaderFrameBits);
	constexpr int HeaderPayloadBytes = HeaderPayloadCells / 8;
	constexpr int BytesPerFrame = BaseBytesPerFrame * LayoutScale * LayoutScale + HeaderPayloadBytes;

	constexpr int BaseTopDataLeft = BaseHeaderLeft + BaseHeaderWidth;
	constexpr int BaseTopDataWidth = 75;
	constexpr int DataAreaCount = 5;
	constexpr int BasePaddingCellCount = 2;
	constexpr int PaddingCellCount = BasePaddingCellCount * LayoutScale * LayoutScale;

	constexpr int BaseFinderCenter = BaseQrPointSize / 2;
	constexpr int FinderCenter = BaseFinderCenter * LayoutScale;
	constexpr int OppositeFinderCenter = (BaseFrameSize - BaseFinderCenter - 1) * LayoutScale;
	constexpr int SmallQrCenter = (BaseFrameSize - BaseSmallQrPointBias) * LayoutScale;
	constexpr int OrientationCornerSize = (BaseQrPointSize + 1) * LayoutScale;

	constexpr std::array<DataArea, DataAreaCount> kBaseDataAreas =
	{{
		{3, BaseTopDataLeft, 3, BaseTopDataWidth, 0},
		{6, 21, 15, 91, 0},
		{21, 3, 88, 127, 0},
		{109, 3, 3, 127, 0},
		{112, 21, 18, 91, 0}
	}};

	constexpr int scaleCoord(int value)
	{
		return value * LayoutScale;
	}
}
