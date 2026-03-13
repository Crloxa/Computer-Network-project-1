# 协议 V1.7：133x133 三主定位器 + 右下辅助定位格式

> 本文档描述当前 `protocol_v1` 主线的 `133x133 @ module_px=8` 方案。仓库仍保留历史 `protocol_iso.*`、`qr_iso.*` 与对应 `v2` 文档，但它们不属于当前默认入口。

当前这版的目标很直接：

- 在保留 `3 个主定位器 + 右下辅助定位块 + timing + 最简头部` 的基础上，把逻辑网格从 `108x108` 提升到 `133x133`
- 参考 `Visual-Net` 的“三大定位器 + 右下额外标定点 + 局部紧凑头部”思路，缩小头部保留区占用
- 将单帧 payload 上限从 `1380 bytes` 提升到 `2127 bytes`

## 1. 固定几何布局

- 协议标识：`V1.7-133-4F`
- 逻辑网格：`133 x 133`
- `module_px = 8`
- 逻辑渲染尺寸：`1064 x 1064`
- 静区：四边各 `4 modules = 32 px`
- 物理输出尺寸：`1128 x 1128`

### 1.1 主定位器与右下辅助定位

- 主定位器保留块：左上、右上、左下各 `10 x 10`
  - 左上：`x=0..9, y=0..9`
  - 右上：`x=123..132, y=0..9`
  - 左下：`x=0..9, y=123..132`
- 主定位器实际图案：`9 x 9`
  - 左上图案：`x=0..8, y=0..8`
  - 右上图案：`x=124..132, y=0..8`
  - 左下图案：`x=0..8, y=124..132`
- 主定位器安全边界：
  - 每个 `10x10` 保留块里都包含了 `1` 模块级的留白/隔离边界
  - Header 和 payload 不会写入这三个保留块
- 右下辅助定位块：`7 x 7`
  - 实际图案位置：`x=108..114, y=108..114`
  - 保留区位置：`x=107..115, y=107..115`，共 `9 x 9`
- 设计意图参考 `Visual-Net`
  - 左上、右上、左下负责主定位和透视矫正
  - 右下额外小块负责补右下角的几何约束
- 右下辅助定位安全边界：
  - 实际 `7x7` 图案外围预留 `1` 模块白边
  - 该保留区与右边界、下边界之间仍有 `17` 模块净空
  - 该保留区与右上/左下主定位器区域之间有明显间隔，不与 timing、header 相接触
- 与 `Visual-Net` 的区别
  - `Visual-Net` 的右下额外标定点是更小的独立定位器
  - 当前 `protocol_v1` 用 `7x7` 辅助定位块复用这类思路，兼顾版式稳定性和实现简单性

### 1.2 Header / Timing

- Header 保留区：`x=10..29, y=10..12`
  - 尺寸：`20 x 3 = 60 bits`
  - 这是一个紧凑控制头，思路上参考 `Visual-Net` 的“局部帧头”，不再保留大块空白 header 区
- Timing：
  - 水平：`y=30, x=10..122`
  - 垂直：`x=30, y=10..122`
  - 起始相位为黑，交点 `(30,30)` 固定为黑
- 右下辅助定位块：
  - 实际图案尺寸：`7 x 7`
  - 保留区尺寸：`9 x 9`
  - 实际图案位置：`x=108..114, y=108..114`
  - 保留区位置：`x=107..115, y=107..115`
  - 它在当前实现中承担“辅助定位 / 右下角标定”的角色

## 2. 最简头部

- 颜色映射：`0 = white`，`1 = black`
- Header 遍历顺序：按行优先扫描 `x=10..29, y=10..12`
- 当前实现正好使用全部 `3` 行，共 `60 bits`
- 其中前 `48 bits` 为有效字段，后 `12 bits` 固定填白，作为保留扩展位

### 2.1 Row 0：`frame_type + tail_len_bytes + reserved`

- 位置：`y=10`
- bits `[0..3]`：`frame_type`
  - `0000` = `Single`
  - `0011` = `Start`
  - `1111` = `Normal`
  - `1100` = `End`
- bits `[4..15]`：`tail_len_bytes`
  - `Single` / `End`：写当前帧真实 payload 长度
  - `Start` / `Normal`：固定写 `2127`
- bits `[16..19]`：当前固定保留为 `0`

### 2.2 Row 1：`checkcode16 + reserved`

- 位置：`y=11`
- bits `[20..35]`：`checkcode16`
- bits `[36..39]`：当前固定保留为 `0`
- 校验算法仍为轻量 XOR：
  - 先将 payload 按 16-bit 大端分组做 XOR
  - 若 payload 为奇数长度，最后 1 byte 作为高 8 位参与 XOR
  - 再 XOR `tail_len_bytes`
  - 再 XOR `frame_seq`
  - 再 XOR `flag_word`

### 2.3 Row 2：`frame_seq + reserved`

- 位置：`y=12`
- bits `[40..55]`：`frame_seq`
- bits `[56..59]`：当前固定保留为 `0`
- `frame_seq` 从 `0` 开始递增

## 3. Payload 映射与容量

- Payload 扫描顺序：在整个 `133 x 133` 网格内按 row-major 扫描，跳过所有保留区
- 保留区包含：
  - 三个主定位器保留块
  - `20 x 3` header 区
  - 两条 timing 线
  - 右下 `9 x 9` 辅助定位保留区
- 理论 payload 容量：`17023 bits`
- 当前默认运行上限：`2127 bytes / frame`
- Payload 字节位序：`MSB-first`
- 未使用的 payload cell 固定填白

说明：

- `17023 bits` 不能被 `8` 整除，因此真正按字节可用容量取整为 `2127 bytes`
- 相比旧版 `1380 bytes / frame`，当前方案多出 `747 bytes / frame`

## 4. 编码与视频输出

- `samples` 生成：
  - `layout_guide.*`
  - `sample_full_frame.*`
  - `sample_short_frame.*`
  - `sample_manifest.tsv`
- `encode` / `demo` 生成：
  - `frames/logical/frame_XXXXX.*`
  - `frames/physical/frame_XXXXX.*`
  - `frame_manifest.tsv`
  - `input_info.txt`
  - `protocol_samples/`
  - `demo.mp4`
  - `video_status.txt`
- 视频输出规则：
  - 逻辑帧先渲染为 `1064 x 1064`
  - 再居中 pad 到 `1128 x 1128`
  - 若本机没有 `ffmpeg`，仍会保留帧目录和 manifest，但 `demo.mp4` 不生成

## 5. 解码范围

- `decode` 仍是仓库内自测辅助，不是当前默认交付重点
- 当前 `decode` 首版只保证以下输入：
  - 仓库当前编码器生成的 `frames/physical/`
  - 由该目录重复封装出来的 `demo.mp4`
- 当前 `decode` 不保证：
  - 拍屏或手机拍摄输入
  - 透视畸变矫正
  - 外部历史样例
- 解码流程：
  - 读取图片或用 `ffmpeg` 从视频拆帧
  - 按固定 `1128x1128` 版式裁掉静区并采样 `133x133`
  - 校验三个主定位器、timing 和右下辅助定位块
  - 读取最简头部并校验 `checkcode16`
  - 按 `frame_seq + frame_type + tail_len_bytes` 重组

## 6. CLI

- `Project1 samples <output_dir>`
- `Project1 demo <input_file> <output_dir> [--fps n] [--repeat n]`
- `Project1 encode <input_file> <output_dir> [--fps n] [--repeat n]`
- `Project1 decode <input_video_or_frame_dir> <output_dir>`

## 7. 验收要点

- `Project1 samples out/samples` 能稳定产出新版三张样例图和清单
- `Project1 encode input.bin out/encode/input` 能生成逻辑帧、物理帧和 manifest
- `Project1 demo input.jpg out/demo/input` 能生成可直接展示的效果输出
- `Project1 decode out/encode/input/frames/physical out/decode/input` 的 `output.bin` 必须与输入逐字节一致
- 多帧输入必须正确走 `Start -> Normal -> End`
- 缺帧必须输出 `missing_frames.txt`
