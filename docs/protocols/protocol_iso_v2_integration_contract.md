# ISO QR V2 联调约定（编码端-解码端）

> 状态说明：本文档是当前主线联调契约，用于编码端与解码端统一执行口径。
>
> 协议主规范仍以 `docs/protocols/protocol_iso_qr_v2_course.md` 为准；本文件补充联调执行级约定。

## 1. 文档目标

- 固化 v2 主线中已稳定的接口、行为和验收口径。
- 降低“各自实现通过但联调失败”的沟通成本。
- 为后续可优化分支（如四色增强）提供主线基准。

## 2. v2 主线已锁定约定（继承）

### 2.1 参数语境

- `profile`: 当前首版固定为 `iso133`
- `ecc`: 当前首版固定为 `Q`
- `canvas`: `>= 720`
- 当前默认工作点：`iso133 + Q + 1440`

联调要求：编码与解码必须使用相同 `profile/ecc/canvas`，当前阶段不做自动猜档，也不支持切换到其他 ISO 档位。

### 2.2 单帧字节格式

- `header(8 bytes) + payload + crc32(4 bytes)`
- `header` 字段：
  - `protocol_id` (1 byte, fixed `0xA2`)
  - `protocol_version` (1 byte, fixed `0x01`)
  - `frame_seq` (2 bytes, big-endian)
  - `total_frames` (2 bytes, big-endian)
  - `payload_len` (2 bytes, big-endian)
- CRC32 覆盖范围：`header + payload`
- CRC32 参数：poly `0xEDB88320`，init `0xFFFFFFFF`，xorout `0xFFFFFFFF`

### 2.3 黑白主线解码流程

1. 若输入是视频，先用 `ffmpeg` 抽帧；若输入是 PNG/JPG，先转成内部 BMP。
2. 基于四角 marker 做归一化，裁剪中心 QR 区并回采模块网格。
3. 调用仓库内自研 `Version 29 / ECC Q` 解码器读取 QR 字节流。
4. 成功后执行 header 解析与 CRC 校验。
5. 按 `frame_seq` 去重收集并重组。

### 2.4 当前实现阈值

- marker 最小轮廓面积：`frame_area * 0.0015`
- 候选 marker 最小宽高：`> 20 px`
- 候选 marker 最大宽高比：`<= 1.35`
- 渲染最小模块尺度：`module_pixels >= 4.0`
- 当前首版只支持：`iso133 + Q + markers=on`

### 2.5 解码标准输出

- `output.bin`
- `decode_report.tsv`
- `decode_summary.txt`
- `missing_frames.txt`（仅缺帧时）
- `decode_debug/source/`
- `decode_debug/warped/`
- `decode_debug/qr_crop/`

## 3. 角色分工

### 3.1 编码端交付

- 提供可复现输入和完整 encode 命令。
- 提供 `frame_manifest.tsv` 与样例帧目录。
- 明确每批次参数语境：`profile/ecc/canvas/fps/repeat`。

### 3.2 解码端交付

- 提供完整 decode 输出目录与问题摘要。
- 提供 `decode_report.tsv` 失败类型统计。
- 对每个失败记录可复现 `source_index`。

## 4. 十项联调清单（推荐写法）

以下 10 项建议写入联调执行单并作为默认检查项。

### 4.1 固定解码输入参数

推荐约定：
- decode 调用必须显式传 `--profile --ecc --canvas`。
- 默认先跑：`iso133 + Q + 1440`。
- 对照可跑：`iso133 + Q + 2160`。

### 4.2 固定单帧格式

推荐约定：
- 一律 `header(8B)+payload+crc32(4B)`。
- 一律大端，禁止分支间混用大小端。
- CRC 一律覆盖 `header+payload`。

### 4.3 固定字段语义

推荐约定：
- `frame_seq` 从 `0` 开始，范围 `[0, total_frames-1]`。
- `total_frames` 在同一任务内恒定。
- `payload_len` 等于当前帧净荷字节数。

### 4.4 固定冲突处理

推荐约定：
- 同一 `frame_seq` 首次 CRC 通过的帧作为有效帧。
- 后续同序号帧标记为重复，不覆盖已收数据。
- 同序号若 CRC 都通过但 payload 不同，记录冲突并保留首帧。

### 4.5 固定失败规则

推荐约定：
- 软失败：单帧解码失败、CRC 失败、重复帧，继续处理。
- 硬失败：输入不可读、输出不可写、协议 id/version 不兼容。
- 任务失败判定：最终存在缺帧或关键硬失败。

### 4.6 固定输出文件

推荐约定：
- 每次 decode 必须输出 `decode_report.tsv` 与 `decode_summary.txt`。
- 缺帧时必须输出 `missing_frames.txt`。
- `decode_report.tsv` 必含列：
  - `source_index, success, profile, ecc, method, frame_seq, total_frames, payload_len, message`

### 4.7 固定最小样例集

推荐约定：
- 样例 A（全成功）：`output.bin` 与输入全字节一致。
- 样例 B（缺帧）：能正确输出缺失序号。
- 样例 C（CRC 错）：错帧被丢弃且报告可追踪。

### 4.8 固定 manifest 字段

推荐约定：
- `frame_manifest.tsv` 至少包含：
  - `file, frame_seq, total_frames, payload_len, profile, ecc, canvas_px, frame_bytes`
- 字段名固定，不做同义改名。

### 4.9 固定版本策略

推荐约定：
- `protocol_id != 0xA2` 或 `protocol_version != 0x01` 时按不兼容处理。
- 不兼容帧不参与重组，报告中明确原因。
- 升级协议版本必须附带向后兼容说明。

### 4.10 固定联调通过标准

推荐约定：
- 主标准：`output.bin` 与原输入逐字节一致。
- 辅标准：
  - 缺帧场景准确输出缺失序号；
  - CRC 错帧不污染重组结果；
  - `decode_report.tsv` 可追踪每个 `source_index`。

## 5. 推荐联调流程（可直接执行）

1. 编码端产出基线：
   - `Project1 encode input.bin out/encode/input --profile iso133 --ecc Q --canvas 1440`
2. 解码端同语境解码：
   - `Project1 decode out/encode/input/demo.mp4 out/decode/input --profile iso133 --ecc Q --canvas 1440`
3. 比对结果：
   - 校验 `output.bin` 字节一致性；
   - 检查 `decode_report.tsv` 是否出现异常集中点；
   - 检查 `decode_summary.txt` 与预期帧数是否一致。

## 6. 实施顺序建议

- 第 1 阶段：先打通黑白主线闭环，不引入彩色增强。
- 第 2 阶段：完善失败分类和报表可观测性。
- 第 3 阶段：在不破坏主线的前提下评估可优化分支。

## 7. 与可优化方案关系

- `docs/protocols/protocol_rgb_4color_option.md` 属于可优化选项。
- 彩色增强联调不得替代本文件定义的黑白主线验收标准。
- 建议先满足本文件全部 10 项，再进入彩色增强联调。
