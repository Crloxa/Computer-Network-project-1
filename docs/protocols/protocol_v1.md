# 协议 V1.6：108x108 四 Finder 最简闭环格式

> 本文档描述当前默认主线。仓库仍保留 `protocol_iso.*`、`qr_iso.*` 与对应 `v2` 文档，但它们只作为历史实现参考，不再是默认入口。

当前默认交付口径优先强调：

- 自定义编码方案本身
- `samples / demo` 生成的版式和效果图

`decode` 继续保留在仓库中，但只作为自测与联调辅助，不作为主叙事中心。

## 1. 固定几何布局

- 协议标识：`V1.6-108-4F`
- 逻辑网格：`108 x 108`
- 物理输出尺寸：`1080 x 1080`
- `module_px = 9`
- 静区：四边各 `54 px`
- Finder 保留块：四角各 `8 x 8`
  - 左上：`x=0..7, y=0..7`
  - 右上：`x=100..107, y=0..7`
  - 左下：`x=0..7, y=100..107`
  - 右下：`x=100..107, y=100..107`
- Finder 图案：四角均为 `7 x 7`
  - 左上、右上、左下为普通 finder
  - 右下中心 `3 x 3` 反相，用于方向判定
- Header 保留区：`x=8..23, y=8..17`，共 `16 x 10 = 160 bits`
- Timing：
  - 水平：`y=24, x=8..99`
  - 垂直：`x=24, y=8..99`
  - 起始相位为黑，交点 `(24,24)` 固定为黑
- Alignment：`5 x 5`，位置 `x=88..92, y=88..92`

## 2. 最简头部

- 颜色映射：`0 = white`，`1 = black`
- Header 遍历顺序：按行优先扫描 `x=8..23, y=8..17`
- 当前实现只使用前 3 行，其余 7 行固定填白并继续保留不用

### 2.1 Row 0：`frame_type + tail_len_bytes`

- 位置：`y=8`
- bits `[0..3]`：`frame_type`
  - `0000` = `Single`
  - `0011` = `Start`
  - `1111` = `Normal`
  - `1100` = `End`
- bits `[4..15]`：`tail_len_bytes`
  - `Single` / `End`：写当前帧真实 payload 长度
  - `Start` / `Normal`：固定写 `1380`

### 2.2 Row 1：`checkcode16`

- 位置：`y=9`
- 16-bit，大端位序
- 算法为轻量 XOR 校验：
  - 先将 payload 按 16-bit 大端分组做 XOR
  - 若 payload 为奇数长度，最后 1 byte 作为高 8 位参与 XOR
  - 再 XOR `tail_len_bytes`
  - 再 XOR `frame_seq`
  - 再 XOR `flag_word`
- `flag_word` 规则：
  - `Start`：`0x0002`
  - `End`：`0x0001`
  - `Single`：`0x0003`
  - `Normal`：`0x0000`

### 2.3 Row 2：`frame_seq`

- 位置：`y=10`
- 16-bit，大端位序
- 从 `0` 开始递增

## 3. Payload 映射与容量

- Payload 扫描顺序：在整个 `108 x 108` 网格内按 row-major 扫描，跳过所有保留区
- 保留区包含：
  - 四个 finder 保留块
  - `16 x 10` header 区
  - 两条 timing 线
  - `5 x 5` alignment 区
- 理论 payload 容量：`1380 bytes`
- 当前默认运行上限：`1380 bytes / frame`
- Payload 字节位序：`MSB-first`
- 未使用的 payload cell 固定填白

## 4. 编码与视频输出

- 当前默认演示重点是 `encoder + samples/demo`，即“设计编码方案”和“生成效果图/测试输出”。
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
  - 逻辑帧先渲染为 `972 x 972`
  - 再居中 pad 到 `1080 x 1080`
  - 若本机没有 `ffmpeg`，仍会保留帧目录和 manifest，但 `demo.mp4` 不生成

## 5. 解码范围

- `decode` 是仓库内自测辅助，不是当前默认交付重点。
- 当前 `decode` 首版只保证以下输入：
  - 仓库当前编码器生成的 `frames/physical/`
  - 由该目录重复封装出来的 `demo.mp4`
- 当前 `decode` 不保证：
  - 拍屏或手机拍摄输入
  - 透视畸变矫正
  - 外部历史样例
- 解码流程：
  - 读取图片或用 `ffmpeg` 从视频拆帧
  - 按固定 `1080x1080` 版式裁掉静区并采样 `108x108`
  - 校验四个 finder、timing 和 alignment
  - 读取最简头部并校验 `checkcode16`
  - 按 `frame_seq + frame_type + tail_len_bytes` 重组

## 6. CLI

- `Project1 samples <output_dir>`
- `Project1 demo <input_file> <output_dir> [--fps n] [--repeat n]`
- `Project1 encode <input_file> <output_dir> [--fps n] [--repeat n]`
- `Project1 decode <input_video_or_frame_dir> <output_dir>`

## 7. 验收要点

- `Project1 samples out/samples` 能稳定产出三张样例图和清单
- `Project1 encode input.bin out/encode/input` 能生成逻辑帧、物理帧和 manifest
- `Project1 demo input.jpg out/demo/input` 能生成可直接展示的效果输出
- `Project1 decode out/encode/input/frames/physical out/decode/input` 的 `output.bin` 必须与输入逐字节一致，但这项属于仓库内自测，不是默认展示中心
- 多帧输入必须正确走 `Start -> Normal -> End`
- 缺帧必须输出 `missing_frames.txt`
