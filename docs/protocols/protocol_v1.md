# 协议 V1.6：108x108 四 Finder 鲁棒编码格式（历史参考）

> 本文档描述旧的自定义方案。当前有效主线已切换为 `docs/protocols/protocol_iso_qr_v2_course.md` 中的 ISO QR 方案。

## 固定几何布局
- 协议标识：`V1.6-108-4F`
- 逻辑网格：`108 x 108` cells
- 物理输出尺寸：`1080 x 1080`
- `module_px = 9`
- 外围静区：四边各 `6 modules = 54 px`
- Finder 保留块：四角各保留 `8 x 8` cells
  - 左上：`x=0..7, y=0..7`
  - 右上：`x=100..107, y=0..7`
  - 左下：`x=0..7, y=100..107`
  - 右下：`x=100..107, y=100..107`
- Finder 图案：四角均为 `7 x 7`，右下 finder 的中心 `3 x 3` 反相，用于方向判定
- Header 起点：`(8, 8)`
- Header 尺寸：`16 x 10 = 160 bits`
- Timing 线：
  - 水平：`y=24, x=8..99`
  - 垂直：`x=24, y=8..99`
  - 起始相位均为黑，交点 `(24,24)` 固定为黑
- Alignment：固定 1 个 `5 x 5`，位置 `x=88..92, y=88..92`

## Header 与位序
- 颜色映射：`0 = white`，`1 = black`
- Header 遍历顺序：按行优先扫描 `x=8..23, y=8..17`
- Header 固定为三段：
  - bits `[0..63]`：主 header
  - bits `[64..127]`：主 header 镜像副本
  - bits `[128..159]`：`crc32_header_128`
- 主 header 字段：
  - `frame_seq`：16 bits
  - `payload_len_bytes`：16 bits
  - `crc32_payload`：32 bits
- 整数字段按大端位序写入
- Payload 字节内位序：`MSB-first`

## Payload 映射与容量
- Payload 扫描顺序：在整个 `108 x 108` 网格内按 row-major 扫描，跳过保留区
- 保留区包含：
  - 四个 finder 保留块
  - `16 x 10` header 区
  - 两条 timing 线
  - `5 x 5` alignment 区
- 理论 payload 容量：`1380 bytes`
- 当前编码器演示上限：`1024 bytes / frame`
- 若输入帧超过 `1024 bytes`，编码器会拆成多帧处理

## CRC32
- `crc32_payload` 覆盖范围：仅 payload，不包含 header
- `crc32_header_128` 覆盖范围：header 前 `128 bits`
- 算法：标准 `CRC32 IEEE`
- 多项式：`0xEDB88320`
- 初始值：`0xFFFFFFFF`
- 最终异或值：`0xFFFFFFFF`

## 视频输出
- 物理帧率：`60`
- 重复次数：`3`
- 目标像素格式：`yuv420p`
- 实现行为：
  - 先在 `972 x 972` 画出逻辑网格内容
  - 再居中 pad 到 `1080 x 1080`，形成四边 `54 px` 白边
  - 优先用 OpenCV `VideoWriter` 写临时 MP4
  - 若可用 `ffmpeg`，再转码为 `yuv420p`

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
