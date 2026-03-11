# ISO QR 主线设计与实现说明

## 1. 文档定位

本文档是当前主线实现的工程交接文档，覆盖：
- 设计目标与边界
- 协议与参数接口
- 编码、载体、视频、解码与重组流程
- 输出文件与调试产物
- 失败模式、排障路径与已知限制

本文档描述的对象是当前仓库中的 ISO QR 主线实现，不包含旧 `V1.6-108-4F` 自定义协议的扩展设计。旧协议仅保留在 `protocol_v1.md` 作为历史参考。
文件名中的 `v2` 是历史命名沿用，不表示当前默认二维码符号版本是 QR Version 2；当前默认工作点是 `iso133 / Version 29 / 133x133 + ECC Q`。
自 2026-03-11 起，仓库代码已经切到“自研 QR 本体 + 现有 isoqrv2 外部契约”的实现路线：不再调用 OpenCV 或现成 QR 编解码库，当前首版仅支持 `iso133 + Q + markers=on`。

## 2. 目标与边界

### 2.1 目标

- 使用 `Version 29 / 133x133 + ECC Q` 这套标准 QR 版式承载单帧数据，但二维码本体由仓库内 C++ 自行编码与解码。
- 保持 QR 本体标准不变，只在外围增加 carrier 与定位标记，以适配视频录制、拍屏和透视矫正。
- 提供统一的命令行入口，覆盖样例生成、编码打包、视频解码和文件重组。
- 让接手该仓库的开发者可以只靠本文档理解参数、流程、产物和调试方法。

### 2.2 非目标

- 不在当前主线中扩展到 `iso109/iso145/iso177` 或 `M/H`；这些参数仍保留在 CLI 兼容层，但首版代码只接受 `iso133/Q`。
- 不在当前主线中实现彩色 payload 或非标准二维码版式。
- 不在当前主线中实现跨帧 FEC / fountain code / 额外 RS 冗余。
- 不把旧阶段样例 PNG 当作当前主线协议基准。

## 3. 核心设计决策

### 3.1 为什么主线切到标准 ISO QR

- 与 `examples` 中的自定义版式相比，标准 ISO QR 更容易复用 OpenCV 编码与检测能力。
- 后续对鲁棒性的优化可以集中在外围 carrier、拍摄条件和参数选择，不必重复实现二维码本体规则。
- 课程展示可以保留“大尺寸、可拍摄、可透视拉正”的视觉效果，同时避免偏离标准。

### 3.2 为什么默认档位是 `Version 29 / 133x133 + ECC Q`

- `108x108` 不是合法 ISO QR 尺寸；标准尺寸必须满足 `21 + 4n`。
- 组内反馈表明 `Version 29 / 133x133` 的信息量与可识别性更平衡，适合作为默认工作点。
- `ECC Q` 在容量和拍摄容错之间更均衡，适合默认配置。
- 当录制链路支持 4K 时，优先通过增加 `canvas` 获得更高采样密度，而不是改造 QR 本体。

## 4. 术语

- `profile`：预设的 ISO QR 版本档位，例如 `iso133`。
- `logical grid`：二维码逻辑边长，单位是 module，例如 `133`。
- `module`：二维码最小黑白单元。
- `quiet zone`：二维码四周保留的标准静区。
- `carrier`：外围白底画布和四角定位标记，不属于 QR 本体。
- `canvas`：carrier 输出画布的像素边长，例如 `1440` 或 `2160`。

## 5. 模块划分

当前实现的职责分布如下：

### 5.1 `protocol_iso.*`

负责协议与参数模型：
- `ProfileId`
- `ErrorCorrection`
- `Profile`
- `FrameHeader`
- `EncoderOptions`
- header 打包与解析
- CRC32 计算
- profile/ecc 字符串解析

### 5.2 `encoder.*`

负责主流程实现：
- 标准 QR 编码
- carrier 布局生成
- 样例输出
- 输入文件切帧
- 视频封装
- carrier 透视拉正
- QR 解码与重组
- 调试产物和报告文件输出

### 5.3 `main.cpp`

负责 CLI：
- 参数解析
- 默认值使用
- `samples / encode / decode` 命令分发

## 6. 公共接口与默认值

### 6.1 Profile

协议文档保留完整 profile 概念，但当前代码只支持：

- `iso133`

| 名称 | Version | logical grid | 角色 |
| --- | --- | --- | --- |
| `iso109` | 23 | 109 | 接近 “108 左右” 的实验性档位 |
| `iso133` | 29 | 133 | 默认主线档位 |
| `iso145` | 32 | 145 | 更高容量对照档 |
| `iso177` | 40 | 177 | 极限压力档 |

默认 profile：
- `iso133`

### 6.2 ECC

协议文档保留完整 ECC 概念，但当前代码只支持：

- `Q`

默认值：
- `Q`

推荐选择：
- 常规录屏或稳定拍摄：`Q`
- 噪声更大或解码失败较多：`H`
- 容量优先对照实验：`M`

### 6.3 `FrameHeader`

固定 8 bytes，大端：

| 字段 | 大小 | 说明 |
| --- | --- | --- |
| `protocol_id` | 1 byte | 固定 `0xA2` |
| `protocol_version` | 1 byte | 固定 `0x01` |
| `frame_seq` | 2 bytes | 当前帧序号 |
| `total_frames` | 2 bytes | 总帧数 |
| `payload_len` | 2 bytes | 当前 payload 字节数 |

单帧字节布局：
1. `header`
2. `payload`
3. `crc32`

固定开销：
- `kHeaderBytes = 8`
- `kCrcBytes = 4`
- `kFrameOverheadBytes = 12`

CRC32 规则：
- 覆盖 `header + payload`
- 多项式 `0xEDB88320`
- 初始值 `0xFFFFFFFF`
- 最终异或 `0xFFFFFFFF`

### 6.4 `EncoderOptions`

默认值如下：

| 字段 | 默认值 | 说明 |
| --- | --- | --- |
| `profile_id` | `iso133` | 主线默认档位 |
| `error_correction` | `Q` | 默认纠错级别 |
| `canvas_pixels` | `1440` | 默认 carrier 边长 |
| `fps` | `60` | 视频输出帧率 |
| `repeat` | `3` | 每个逻辑帧重复写入次数 |
| `enable_carrier_markers` | `true` | 是否绘制四角定位标记 |
| `write_protocol_samples` | `true` | `encode` 时是否额外生成 `protocol_samples/` |
| `write_decode_debug` | `true` | `decode` 时是否写出 `decode_debug/*` |

参数校验规则：
- `--canvas >= 720`
- `--fps > 0`
- `--repeat > 0`
- `--markers` 只接受 `on/off`

## 7. CLI 契约

### 7.1 `samples`

```bash
Project1 samples <output_dir> [--profile iso109|iso133|iso145|iso177] [--ecc M|Q|H] [--canvas px]
```

作用：
- 生成每个 profile 的样例 QR 图、carrier 图和布局图
- 输出容量矩阵 `sample_capacity.tsv`

### 7.2 `encode`

```bash
Project1 encode <input_file> <output_dir> [--profile iso109|iso133|iso145|iso177] [--ecc M|Q|H] [--canvas px] [--fps n] [--repeat n] [--protocol-samples on|off]
```

作用：
- 读取单个输入文件
- 切帧并生成标准 QR
- 输出 QR 帧、carrier 帧、manifest、样例和视频

### 7.3 `decode`

```bash
Project1 decode <input_video_or_frame_dir> <output_dir> [--profile iso109|iso133|iso145|iso177] [--ecc M|Q|H] [--canvas px] [--decode-debug on|off]
```

作用：
- 从视频或帧目录读取图像
- 执行 QR 解码、透视拉正、header 校验和重组
- 输出 `output.bin` 和调试报告

## 8. 编码设计

### 8.1 容量探测

当前首版容量是固定值：
- `max_frame_bytes = 908`
- `max_payload_bytes = 896`
- 该值对应当前自研实现固定支持的 `Version 29 / ECC Q / Byte mode`

### 8.2 标准 QR 生成

QR 本体由仓库内自研 C++ 实现：
- 固定 `version = 29`
- 固定 `correction_level = Q`
- 固定 `mode = Byte mode`
- 内部自行完成 data codewords、RS block、mask 选择、format/version info 写入与解码

本体规则：
- 保持标准 quiet zone
- 不叠加额外花纹
- 不在 QR 本体内部放 carrier 标记

### 8.3 Carrier 布局

carrier 是在标准 QR 外层额外构造的白底画布。布局参数由 `canvas_pixels` 推导：

- `marker_margin = max(24, canvas / 28)`
- `marker_size = max(72, canvas / 9)`
- `qr_size = max(720, canvas * 0.78)`
- `qr_x = qr_y = (canvas - qr_size) / 2`

四角规则：
- 左上、右上、左下使用相同 marker
- 右下使用反相中心 marker，用于方向区分

### 8.4 渲染尺度约束

编码后会检查：

```text
module_pixels = qr_size / qr_frame.cols
```

当前阈值：
- `module_pixels >= 4.0`

若阈值不足，则直接报错，避免生成在拍摄场景下几乎不可用的码图。

### 8.5 视频输出

视频封装流程：
1. 先把每个逻辑帧写成内部 BMP 序列
2. 每个逻辑帧按 `repeat` 次数重复写入
3. 使用 `ffmpeg` 把 BMP 序列封装为 `yuv420p` 的 `demo.mp4`
4. 若 `ffmpeg` 不可用，则视频链路直接报错，但帧图链路仍可独立联调

## 9. 解码设计

### 9.1 输入源

支持两种解码输入：
- 帧目录
- 视频文件

视频路径下会先用 `ffmpeg` 抽帧到临时 BMP 目录，再进入自研解码流程。

### 9.2 两段式检测

当前检测链是固定裁切链路：

1. 先把输入帧转成 BMP
2. 再按当前 carrier 固定布局裁出中心 QR 区
3. 对 QR 区做阈值化与模块采样
4. 交给仓库内自研 `Version 29 / Q` 解码器读取

### 9.3 Carrier 拉正

首版不做复杂透视拉正。当前做法是：
- 若输入已经接近 QR 模块图，则直接按模块网格采样
- 否则先把整帧缩放到 `canvas`
- 再按固定 `carrier` 布局裁出中心 QR 区
- 对 QR 区做 Otsu 阈值化和模块投票采样

### 9.4 Header 解析与重组

解码成功后执行：
- 解析 `FrameHeader`
- 校验协议 id / version
- 校验 `payload_len`
- 校验 CRC32
- 读取 `frame_seq / total_frames`

重组规则：
- 首次成功帧确定 `expected_total_frames`
- 若后续帧 `total_frames` 不一致，则记为失败
- 同一 `frame_seq` 仅收第一份 payload，后续重复帧跳过并在 `decode_report.tsv` 中记为 `duplicate_frame`
- 最终按 `frame_seq` 从小到大拼接得到 `output.bin`

缺帧行为：
- 若存在缺失序号，写 `missing_frames.txt`
- 同时返回错误，不输出“完整解码成功”的结果
- 即使失败，也必须稳定输出 `decode_report.tsv` 与 `decode_summary.txt`

## 10. 输出产物

### 10.1 `samples`

输出：
- `sample_<profile>_symbol.png`
- `sample_<profile>_carrier.png`
- `sample_<profile>_layout.png`
- `sample_manifest.tsv`
- `sample_capacity.tsv`

`sample_manifest.tsv` 列：

| 列名 | 说明 |
| --- | --- |
| `file` | 样例 QR 文件名 |
| `profile` | 当前 profile |
| `version` | ISO version |
| `logical_grid` | 逻辑边长 |
| `ecc` | 当前样例使用的 ECC |
| `canvas_px` | carrier 画布边长 |
| `max_frame_bytes` | 单帧最大总字节数 |
| `max_payload_bytes` | 单帧最大净荷字节数 |

`sample_capacity.tsv` 列：

| 列名 | 说明 |
| --- | --- |
| `profile` | profile 名称 |
| `version` | ISO version |
| `logical_grid` | 逻辑边长 |
| `ecc` | `M/Q/H` |
| `max_frame_bytes` | 单帧最大总字节数 |
| `max_payload_bytes` | 单帧最大净荷字节数 |
| `recommended` | 是否为推荐工作点 |

### 10.2 `encode`

输出目录结构：
- `frames/qr/frame_XXXXX.png`
- `frames/carrier/frame_XXXXX.png`
- `protocol_samples/`（默认生成，可通过 `--protocol-samples off` 关闭）
- `frame_manifest.tsv`
- `input_info.txt`
- `demo.mp4`
- `video_status.txt`

`frame_manifest.tsv` 列：

| 列名 | 说明 |
| --- | --- |
| `file` | carrier/QR 帧文件名 |
| `frame_seq` | 帧序号 |
| `total_frames` | 总帧数 |
| `payload_len` | 当前帧净荷长度 |
| `profile` | 当前 profile |
| `ecc` | 当前 ECC |
| `canvas_px` | carrier 画布边长 |
| `frame_bytes` | 当前帧总字节数 |

`input_info.txt` 字段：
- `protocol`
- `profile`
- `version`
- `logical_grid`
- `ecc`
- `canvas_px`
- `input_path`
- `input_bytes`
- `frame_count`
- `max_frame_bytes`
- `max_payload_bytes`
- `fps`
- `repeat`
- `carrier_markers`
- `protocol_samples`

### 10.3 `decode`

输出：
- `output.bin`
- `decode_report.tsv`
- `decode_summary.txt`
- `missing_frames.txt`（仅缺帧时）
- `decode_debug/source/`（默认生成，可通过 `--decode-debug off` 关闭）
- `decode_debug/warped/`（默认生成，可通过 `--decode-debug off` 关闭）
- `decode_debug/qr_crop/`（默认生成，可通过 `--decode-debug off` 关闭）

`decode_report.tsv` 列：

| 列名 | 说明 |
| --- | --- |
| `source_index` | 输入帧索引 |
| `success` | 当前帧是否成功进入有效重组 |
| `profile` | 当前解码 profile |
| `ecc` | 当前解码 ECC |
| `method` | `direct`、`aruco`、`warped-direct` 等 |
| `frame_seq` | 解析出的帧序号 |
| `total_frames` | 解析出的总帧数 |
| `payload_len` | 解析出的净荷长度 |
| `message` | 解码或校验信息 |

`decode_summary.txt` 字段：
- `status`
- `decoded_frames`
- `total_frames`
- `output_bytes`
- `missing_frames`

`missing_frames.txt`：
- 第一行固定为 `missing_count=<n>`
- 后续每行一个缺失的 `frame_seq`

### 10.4 固定联调样例

仓库内固定保留 3 套当前 ISO 主线基准：
- `bin/samples/v2_success/`
- `bin/samples/v2_missing_frame/`
- `bin/samples/v2_crc_error/`

统一参数：
- `profile=iso133`
- `ecc=Q`
- `canvas_px=1440`

用途：
- `v2_success`：验证 `output.bin == input.bin`
- `v2_missing_frame`：验证缺帧失败路径和 `missing_frames.txt`
- `v2_crc_error`：验证 `crc_mismatch` 报告与 CRC 错帧跳过行为

## 11. 推荐参数与调优建议

### 11.1 推荐工作点

常规默认：
- `--profile iso133 --ecc Q --canvas 1440`

4K 录制：
- `--profile iso133 --ecc Q --canvas 2160`

拍摄噪声更大：
- `--profile iso133 --ecc H --canvas 2160`

### 11.2 参数取舍

`profile` 越大：
- 容量更高
- 对拍摄采样密度越敏感

`ecc` 越高：
- 容量更低
- 解码容错更高

`canvas` 越大：
- 单 module 的像素更大
- 更适合录屏和屏摄
- 输出文件也更大

### 11.3 关于 `108`

如果讨论“108 左右是否安全”，需要分清两件事：
- 逻辑尺寸：标准 ISO 下不能是 `108`，最近合法值是 `109`
- 渲染像素：真正影响识别的是每个 module 有多少像素，而不是只看逻辑边长

因此当前主线用 `133x133`，并通过更大的 `canvas` 去提高可拍摄性。

## 12. 失败模式与排障

### 12.1 编码阶段

常见失败：
- 当前参数组合不是 `iso133 + Q`
- 当前参数下连 header 都塞不进去
- 渲染尺度低于 `4 px/module`

排障顺序：
1. 确认当前参数就是 `--profile iso133 --ecc Q --markers on`
2. 提高 `--canvas`
3. 检查输入文件大小是否导致分帧数异常增大

### 12.2 视频阶段

常见失败：
- `ffmpeg` 不存在，无法完成 PNG/BMP 转换或视频封装
- 输出目录不可写

排障顺序：
1. 先看 `video_status.txt`
2. 确认 `ffmpeg/bin/ffmpeg(.exe)` 是否存在
3. 若只需联调，可直接用 `frames/carrier/` 目录跑 `decode`

### 12.3 解码阶段

常见失败：
- 输入视频打不开
- 直接解码失败
- carrier marker 没定位到四个角
- CRC32 不通过
- `total_frames` 不一致
- 缺帧

排障顺序：
1. 看 `decode_report.tsv`
2. 检查 `decode_debug/source/`、`warped/`、`qr_crop/`
3. 若 `warped/` 空缺，优先排查 marker 可见性
4. 若 `qr_crop/` 有图但仍失败，优先排查 profile/ecc/canvas 是否过激进

## 13. 已知限制

- 当前鲁棒性仍高度依赖实际拍摄条件，尚未形成大规模实测统计。
- 当前没有跨帧冗余，缺帧会直接导致整体解码失败。
- 当前 decode 依赖调用时提供正确的 `profile/ecc/canvas` 语境，不做自动档位猜测。
- 当前代码只支持 `iso133 / Q / markers=on`，其余 ISO 参数仍保留在 CLI 兼容层，但不会真正执行。
- 当前主线不依赖 OpenCV；视频桥接和图片格式转换依赖 `ffmpeg` 可执行程序。

## 14. 后续工作

建议后续按下面顺序演进：

1. 做 `iso133 Q/H` 的实拍成功率统计
2. 对 marker 检测和透视拉正做更多抗噪优化
3. 视需要增加应用层跨帧冗余
4. 若要扩到彩色协议，必须新开协议分支，不要污染当前 ISO 主线
