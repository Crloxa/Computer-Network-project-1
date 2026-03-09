# ISO 标准二维码主线方案

## 1. 目标
- 当前仓库主线改为真实标准 `ISO/IEC 18004 QR Code Model 2`。
- 传输效果向“大尺寸、可拍屏、可透视拉正”的展示方式靠拢，但不修改 QR 本体标准。
- QR 本体以 OpenCV 标准编码/解码能力为实现基础，外围允许存在自定义 carrier 和定位标记。

## 2. 尺寸与版本策略
- ISO QR 逻辑边长必须满足 `21 + 4n`，不能做严格 `108x108`。
- 当前支持三档大尺寸 profile：
  - `iso109`：`Version 23`，`109 x 109`
  - `iso145`：`Version 32`，`145 x 145`
  - `iso177`：`Version 40`，`177 x 177`
- 默认 profile：`iso145`
- 默认纠错等级：`ECC = Q`
- 对照实验档位：`ECC = M`

## 3. 标准二维码本体
- 保持标准 quiet zone，不在 QR 本体上叠加任何非标准图形。
- Finder、alignment、timing、format、mask 与 RS 纠错均由标准 QR 机制决定。
- 编码模式固定使用 `Byte mode`。
- QR 本体只承载应用层字节流，不承载 carrier 额外定位信息。

## 4. 应用层帧格式
- 单帧字节布局：
  1. `header`：8 bytes
  2. `payload`：N bytes
  3. `crc32`：4 bytes，大端
- Header 固定字段：
  - `protocol_id`：1 byte，固定 `0xA2`
  - `protocol_ver`：1 byte，固定 `0x01`
  - `frame_seq`：2 bytes，大端
  - `total_frames`：2 bytes，大端
  - `payload_len`：2 bytes，大端
- `crc32` 覆盖范围：`header + payload`
- `CRC32` 规则：
  - 多项式：`0xEDB88320`
  - 初始值：`0xFFFFFFFF`
  - 最终异或：`0xFFFFFFFF`

## 5. Carrier 设计
- 视频传输帧不是“裸 QR 图”，而是“中心标准 QR + 外围 carrier”。
- carrier 规则：
  - 白底正方形画布
  - 中央放置标准 QR
  - 四角可放非数据定位标记
  - 右下角定位标记允许做方向区分
- 注意：这些定位标记不属于 QR 本体，只用于视频链中的定位、透视矫正和裁切。

## 6. 编码流程
1. 读取输入文件为字节流。
2. 根据 profile 和 ECC，用标准编码器可接受的最大字节数自动探测单帧容量。
3. 将原文件切片为多帧，写入 `header + payload + crc32`。
4. 使用标准 QR 编码器生成单帧 QR。
5. 将 QR 放入 carrier 画布中央，并绘制外围定位标记。
6. 输出逐帧 PNG，并合成为 `demo.mp4`。

## 7. 解码流程
1. 从视频或帧目录读取图像。
2. 优先直接对整帧做标准 QR 检测和解码。
3. 若直接失败，则使用外围 carrier 标记做透视矫正，再裁出中央 QR 区。
4. 对解码得到的字节流解析 `header + payload + crc32`。
5. 通过 `frame_seq`、`total_frames` 和 `crc32` 去重、校验、重组。
6. 输出 `output.bin`、逐帧解析报告和调试图片。

## 8. CLI
- `Project1 samples <output_dir>`
- `Project1 encode <input_file> <output_dir> [--profile iso109|iso145|iso177] [--ecc M|Q] [--canvas px] [--fps n] [--repeat n]`
- `Project1 decode <input_video_or_frame_dir> <output_dir> [--profile iso109|iso145|iso177] [--ecc M|Q] [--canvas px]`

## 9. 输出产物
- 编码：
  - `frames/qr/frame_XXXXX.png`
  - `frames/carrier/frame_XXXXX.png`
  - `frame_manifest.tsv`
  - `input_info.txt`
  - `demo.mp4`
  - `video_status.txt`
- 解码：
  - `output.bin`
  - `decode_report.tsv`
  - `decode_summary.txt`
  - `missing_frames.txt`（仅缺帧时）
  - `decode_debug/source/`
  - `decode_debug/warped/`
  - `decode_debug/qr_crop/`

## 10. 兼容性说明
- 当前主线不再扩展旧的 `V1.6-108-4F` 自定义协议。
- 若后续需要“颜色承载额外 bit”的彩色协议，应另开新协议，不应修改本方案的 ISO QR 本体定义。
