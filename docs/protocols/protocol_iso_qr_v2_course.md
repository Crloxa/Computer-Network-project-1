# ISO 标准二维码课程方案（Version 2, 25x25）

## 1. 目标与约束
- 面向 optical 课程项目，使用 ISO/IEC 18004 标准二维码作为载体。
- 满足你提出的结构要求：
  - 安全边界（quiet zone）
  - 三个 Finder
  - 一个 Alignment
  - 信息头
  - CRC32 位码
- 兼顾课程评分指标：有效传输量、误码率、丢失率、可复现实验流程。

## 2. 标准版本选择
- 选型：`QR Code Model 2, Version 2`
- 网格尺寸：`25 x 25` modules
- 原因：
  - Version 2 天然具有 `3 Finder + 1 Alignment`（位于右下侧区域）。
  - 结构与课程展示需求匹配，复杂度低于高版本。

## 3. 几何结构（ISO 标准）
1. Quiet Zone（安全边界）
- 四边至少 `4 modules` 白边。
- 采集时若边界被裁切，会显著降低定位成功率。

2. Finder Patterns（3 个定位块）
- 位置：左上、右上、左下。
- 每个 Finder 为标准 `7 x 7` 结构，外围带分隔带（separator）。

3. Timing Patterns
- 一条水平、一条垂直交替黑白线，用于网格采样相位校正。

4. Alignment Pattern（1 个）
- Version 2 中存在 1 个标准 alignment 图案（靠近右下区域）。
- 用于透视或局部畸变下的局部对齐。

5. Format Information
- 含纠错等级与掩码编号信息，属于标准保留区。

## 4. 码内数据组织（信息头 + 数据区 + CRC32）
说明：以下“信息头”放在二维码数据 payload 内部，不改变 ISO QR 保留区定义。

### 4.1 二进制打包格式（Byte Mode）
- 编码模式：`Byte mode`
- 数据段结构（按字节顺序）：
  1. `header`（8 bytes）
  2. `payload`（N bytes）
  3. `crc32`（4 bytes, big-endian）

### 4.2 Header 建议字段（8 bytes）
- `protocol_id`：1 byte（固定值，如 `0xA2`）
- `protocol_ver`：1 byte（例如 `0x01`）
- `frame_seq`：2 bytes（大端）
- `total_frames`：2 bytes（大端）
- `payload_len`：2 bytes（大端）

### 4.3 CRC32 规则
- 算法：`CRC32 IEEE`
- 多项式：`0xEDB88320`
- 初始值：`0xFFFFFFFF`
- 最终异或：`0xFFFFFFFF`
- 覆盖范围：`header + payload`（不包含 crc32 字段本身）

## 5. 纠错设计说明（关键）
- ISO 二维码的“纠错”由 Reed-Solomon（RS）完成，属于标准机制。
- `CRC32` 是检错机制，不负责纠错。
- 因此推荐“双层可靠性”：
  - 第一层：QR 标准 RS 纠错（抗局部损坏）
  - 第二层：应用层 CRC32 完整性校验（发现残余错误）

## 6. 纠错等级选择建议
- 推荐默认：`ECC = M`
- 备选：
  - `L`：容量更大，抗损伤更弱
  - `Q/H`：抗损伤更强，容量更低
- 课程实验建议做对照：同样时长下比较 `L/M/Q` 的有效传输率与 BER。

## 7. 容量预算（Version 2, Byte mode）
以下为常见可用字节容量（不含你自定义拆包逻辑）：
- V2-L: 32 bytes
- V2-M: 26 bytes
- V2-Q: 20 bytes
- V2-H: 14 bytes

若采用 `header(8) + crc32(4)` 固定开销：
- V2-L: payload 最大约 20 bytes
- V2-M: payload 最大约 14 bytes
- V2-Q: payload 最大约 8 bytes
- V2-H: payload 最大约 2 bytes

说明：为避免边界条件问题，工程上建议预留 1-2 bytes 裕量。

## 8. 编码流程（发送端）
1. 输入二进制流按帧切片。
2. 组装 `header + payload`。
3. 计算 CRC32 并追加到尾部。
4. 以 Byte mode 写入 ISO QR V2 数据段。
5. 设置 ECC 等级与掩码（由库自动或按评分策略选择）。
6. 输出帧图像并合成为视频。

## 9. 解码流程（接收端）
1. 从拍摄视频提取帧并检测 QR。
2. 使用标准解码器恢复字节流（已包含 RS 纠错过程）。
3. 解析 header，检查长度一致性。
4. 对 `header + payload` 重新计算 CRC32 并比对。
5. 通过 `frame_seq` 和 `total_frames` 重组原始文件。
6. 输出 `out.bin` 与有效性标记（`vout.bin`）。

## 10. 与课程题目对齐
- 发送端/接收端 CLI 仍按课程接口实现。
- 不依赖手机扫码 App，采用你们自有解码流程。
- 能清晰解释 Nyquist/Shannon 语境下的权衡：
  - 提高 ECC 等级提升鲁棒性但降低单帧净荷。
  - 降低净荷换取更低 BER，可能提升有效传输量。

## 11. 交付建议
- 在报告中至少给出三组实验：
  1. `ECC=L`
  2. `ECC=M`
  3. `ECC=Q`
- 对比指标：有效传输量、有效传输率、BER、丢失率。
- 保留一组“你们自定义码”结果作为对照，可展示方案选择依据。

## 12. 示例图片
- `bin/samples/sample_iso_qr_v2_symbol.png`：ISO QR V2 结构化示例码图
- `bin/samples/sample_iso_qr_v2_layout.png`：带区域标注、作用和规格大小说明的布局图
