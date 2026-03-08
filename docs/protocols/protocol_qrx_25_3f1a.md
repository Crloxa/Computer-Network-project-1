# QRX-25-3F1A 设计方案（3 Finder + 1 Alignment）

## 目标
- 面向课程 optical 项目提供一个二维码风格的基线方案。
- 使用 3 个 Finder 和 1 个 Alignment，保持结构接近标准二维码的视觉习惯。
- 在码图中显式放入信息头和 `CRC32`，便于帧级调试与错误定位。

## 设计标识
- 协议 ID：`QRX-25-3F1A`
- 逻辑网格：`25 x 25`
- 静区（quiet zone）：四边各 `4 modules`
- 示例模块像素：`24 px`
- 示例图片总尺寸：`(25 + 2*4) * 24 = 792 px`

## 几何构成
1. Finder（3 个）
- 左上：`x=0..6, y=0..6`
- 右上：`x=18..24, y=0..6`
- 左下：`x=0..6, y=18..24`
- 每个 Finder 外围保留 1 模块分隔带，按 `8 x 8` 保留块处理。

2. Alignment（1 个）
- 位置：`x=18..22, y=18..22`
- 尺寸：`5 x 5`

3. Timing 线
- 水平：`y=6, x=8..16`
- 垂直：`x=6, y=8..16`
- 序列：交替 `1010...`，起始为黑。

4. 信息头 Header
- 位置：`x=17..24, y=8..15`
- 尺寸：`8 x 8 = 64 bits`
- 字段定义：
  - `frame_seq`: 16 bits
  - `payload_len_bytes`: 8 bits
  - `header_version`: 8 bits
  - `crc32_payload`: 32 bits
- 位序：大端写入，按 row-major 扫描填充。

5. 数据区 Payload
- 扫描顺序：对 `25 x 25` 全图按 row-major 扫描。
- 若模块属于保留区（Finder/Header/Timing/Alignment），则跳过。
- 其余模块按 `MSB-first` 填入 payload bits。

## 保留区与容量
- 总模块：`25 * 25 = 625 bits`
- Finder 保留：`3 * 8 * 8 = 192 bits`
- Header：`64 bits`
- Timing：`9 + 9 = 18 bits`
- Alignment：`5 * 5 = 25 bits`
- 理论 payload：`625 - 192 - 64 - 18 - 25 = 326 bits`
- 由于保留区交叉边界处理，当前实现实测可写位约为 `327 bits`（约 `40 bytes` 上限）。

## CRC32 规则
- 覆盖范围：payload 原始字节，不含 Header。
- 算法：`CRC32 IEEE`
- 多项式：`0xEDB88320`
- 初始值：`0xFFFFFFFF`
- 最终异或：`0xFFFFFFFF`

## 样例文件
- `bin/samples/sample_qrx_25_3f1a.png`：实际渲染样例
- `bin/samples/sample_qrx_25_3f1a_layout.png`：结构标注图
- `bin/samples/sample_manifest_qrx_25_3f1a.tsv`：样例清单

## 解码建议
1. 先检测 3 个 Finder 并估计几何变换。
2. 识别 Alignment 做局部校正。
3. 按 timing 线微调采样相位。
4. 读取 Header 后按 `payload_len_bytes` 截断 payload。
5. 校验 `crc32_payload`，输出有效性标记。

## 适用说明
- 该方案是“二维码风格教学协议”，不是 ISO 标准 QR 的完整实现。
- 优点是结构清晰、易改造，适合作业实验与可解释性展示。
- 若追求手机原生扫码兼容，需改用标准 QR 编码链路。
