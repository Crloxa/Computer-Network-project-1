#pragma once

// 协议帧几何布局常量、公用类型与单元格遍历工具
// 供编码端（code.cpp）与解码端（ImgDecode.cpp / ImgDecode.h）共同使用，
// 避免在多个翻译单元中重复定义相同的数值和逻辑。

#include <array>
#include <cmath>
#include <vector>

namespace FrameLayout
{
	// ── 基本帧尺寸 ──────────────────────────────────────────────
	constexpr int BytesPerFrame      = 1878;   // 每帧可携带的最大字节数（含末帧 tailLen 对齐）
	constexpr int FrameSize          = 133;    // 逻辑帧边长（单元格数）
	constexpr int FrameOutputRate    = 10;     // 逻辑单元格 → 物理像素放大倍率
	constexpr int FrameOutputSize    = FrameSize * FrameOutputRate;  // 物理帧边长（像素）

	// ── 安全区与 Finder 图案 ─────────────────────────────────────
	constexpr int SafeAreaWidth      = 2;      // 四边白色安全区宽度（单元格）
	constexpr int QrPointSize        = 21;     // 三角 Finder 图案边长
	constexpr int SmallQrPointbias   = 7;      // 右下角小 Finder 距帧边偏移
	constexpr int SmallQrPointRadius = 3;      // 右下角小 Finder 半径（含中心格）
	constexpr int CornerReserveSize  = 21;     // 右下角保留区边长（含 Finder + 静区）

	// ── 头部区域布局 ─────────────────────────────────────────────
	constexpr int HeaderHeight       = 3;      // Header 行数
	constexpr int HeaderWidth        = 16;     // Header 每行列宽（单元格，即位数）
	constexpr int HeaderLeft         = 21;     // Header 区左侧起始列
	constexpr int HeaderTop          = 3;      // Header 区顶部起始行
	constexpr int HeaderFieldHeight  = 1;      // 每个 Header 字段占用的行数
	constexpr int HeaderFieldBits    = 16;     // 每个 Header 字段的位宽
	constexpr int HeaderBitWidth     = 1;      // 每 bit 占用的单元格列数
	constexpr int HeaderInnerLeft    = 0;      // Header 内部列起始偏移

	// ── 数据区布局 ───────────────────────────────────────────────
	constexpr int TopDataLeft        = HeaderLeft + HeaderWidth;  // 顶部数据区左边界
	constexpr int TopDataWidth       = 75;     // 顶部数据区宽度
	constexpr int DataAreaCount      = 5;      // 矩形数据区数量
	constexpr int PaddingCellCount   = 4;      // 末尾固定白色 padding 单元格数

	// ── 公用结构体 ───────────────────────────────────────────────

	// 矩形数据区描述；trimRight 表示最后几列不参与数据填充
	struct DataArea
	{
		int top;
		int left;
		int height;
		int width;
		int trimRight;
	};

	// 单元格坐标（行、列，均为逻辑帧坐标）
	struct CellPos
	{
		int row;
		int col;
	};

	// 帧类型，编解码两端保持一致
	enum class FrameType
	{
		Start      = 0,
		End        = 1,
		StartAndEnd = 2,
		Normal     = 3
	};

	// 五块矩形数据区的几何描述（与协议 V1.6-108-4F 对应）
	inline const std::array<DataArea, DataAreaCount> kDataAreas =
	{{
		{3,   TopDataLeft, 3,  TopDataWidth, 0},
		{6,   21,          15, 91,           0},
		{21,  3,           88, 127,          0},
		{109, 3,           3,  127,          0},
		{112, 21,          18, 91,           0}
	}};

	// ── 公用几何判断函数 ─────────────────────────────────────────

	// 判断单元格是否位于右下角静区（需跳过，不作数据区）
	inline bool isInsideCornerQuietZone(int row, int col)
	{
		return row >= 130 || col >= 130;
	}

	// 判断单元格是否位于小 Finder 安全缓冲区（需跳过，不作数据区）
	inline bool isInsideCornerSafetyZone(int row, int col)
	{
		const int center = FrameSize - SmallQrPointbias;
		return std::abs(row - center) <= SmallQrPointRadius + 2
			&& std::abs(col - center) <= SmallQrPointRadius + 2;
	}

	// ── 数据单元格集合构建工具 ───────────────────────────────────

	// 生成单个矩形数据区内所有有效单元格坐标
	inline std::vector<CellPos> buildAreaCells(const DataArea& area)
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

	// 生成右下角保留区中可用作数据的单元格坐标（排除静区和小 Finder 缓冲区）
	inline std::vector<CellPos> buildCornerDataCells()
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

	// 生成全帧所有数据单元格（五块矩形区 + 右下角区）
	inline std::vector<CellPos> buildFullDataCells()
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

	// 生成去除末尾 padding 后的有效数据单元格（用于读写 payload）
	inline std::vector<CellPos> buildMergedDataCells()
	{
		auto cells = buildFullDataCells();
		if (cells.size() > PaddingCellCount)
		{
			cells.resize(cells.size() - PaddingCellCount);
		}
		return cells;
	}

	// 生成末尾固定白色 padding 单元格（编码时置白，解码时忽略）
	inline std::vector<CellPos> getPaddingCells()
	{
		const auto cells = buildFullDataCells();
		if (cells.size() <= PaddingCellCount)
		{
			return {};
		}
		return std::vector<CellPos>(cells.end() - PaddingCellCount, cells.end());
	}
}
