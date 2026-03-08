# 协议 V1：108x108 帧编码格式

## 固定几何布局
- 逻辑网格：`108 x 108` cells
- 静区（quiet zone）：四条外边各保留 `4` 个 cell
- Finder 图案：左上、右上、左下各放置一个类二维码风格的 `7 x 7` 定位块
- Finder 分隔带：每个 finder 周围保留 `1` 圈白色 cell
- 协议内区：`x = 12..103`、`y = 12..103`，即 `92 x 92` cells
- Header 起点：`(HX, HY) = (12, 12)`
- Header 尺寸：`8 x 8` cells，共 `64 bits`
- Payload 区域：协议内区中除去这个 `8 x 8` header 块后的所有 cell

## Bit 含义
- 颜色映射：`0 = white`，`1 = black`
- Header 遍历顺序：从 `(12,12)` 到 `(19,19)`，按行优先（row-major）
- Payload 遍历顺序：在整个 `92 x 92` 协议内区中按行优先扫描，跳过 header 块
- Header 字段布局：
  - bits `[0..15]`：`frame_seq`（`uint16`，按大端位序写入）
  - bits `[16..31]`：`payload_len_bytes`（`uint16`，按大端位序写入）
  - bits `[32..63]`：`crc32_payload`（`uint32`，按大端位序写入）
- Payload 每个字节内部的 bit 顺序：`MSB-first`

## 容量
- 协议内区总 cell 数：`92 * 92 = 8464 bits`
- Header 占用：`64 bits`
- 剩余 payload 理论容量：`8400 bits = 1050 bytes`
- V1 单个逻辑帧的 payload 上限：`1024 bytes`

## CRC32
- 覆盖范围：仅 payload，不包含 header
- 算法：标准 `CRC32 IEEE`
- 多项式：`0xEDB88320`
- 初始值：`0xFFFFFFFF`
- 最终异或值：`0xFFFFFFFF`

## 视频输出
- 物理输出尺寸：`1080 x 1080`
- `module_px = 10`
- 物理帧率：`60`
- 重复次数：`3`
- 目标像素格式：`yuv420p`
- 实现行为：
  - 先用 OpenCV `VideoWriter` 写出 MP4
  - 如果本机可用 `ffmpeg`，再把临时 MP4 转码为 `yuv420p`
  - 如果 `ffmpeg` 不可用，则保留 OpenCV 直接生成的 MP4，并在 `video_status.txt` 中记录回退信息

## CLI
- `Project1 samples <output_dir>`
- `Project1 demo <input_file> <output_dir>`
- `Project1 encode <input_file> <output_dir>`
- 推荐本地输出根目录：`out/`
- 示例：
  - `Project1 samples out/samples`
  - `Project1 demo input.jpg out/demo/input`
  - `Project1 encode input.bin out/encode/input`

## 输出产物
- `protocol_samples/layout_guide.png`
- `protocol_samples/sample_full_frame.png`
- `protocol_samples/sample_short_frame.png`
- `frames/logical/frame_XXXXX.png`
- `frames/physical/frame_XXXXX.png`
- `frame_manifest.tsv`
- `demo.mp4`
- `video_status.txt`
