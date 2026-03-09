# Changelog

本文件记录影响协议、代码行为、构建方式、样例资产或文档结构的有效改动。

维护规则：
- 采用集中式记录，统一维护在本文件
- 最新记录放在最前面
- 每条记录至少写明：日期、类型、范围、摘要、兼容性
- 纯格式化、空白调整、错别字修正可不记录

## 2026-03-09

- Type: `Changed`
  - Scope: `protocol`
  - Summary: 将当前有效主线从 `V1.6-108-4F` 自定义编码切换为真实标准 ISO QR 视频传输方案，支持 `iso109/iso145/iso177` 三档 profile 和 `M/Q` 纠错等级
  - Compatibility: `breaking`

- Type: `Added`
  - Scope: `code`
  - Summary: 新增 `protocol_iso.*`，实现 ISO 应用层 header、CRC32、profile 解析，以及围绕 OpenCV QRCodeEncoder/QRCodeDetector 的标准编码与解码链
  - Compatibility: `breaking`

- Type: `Added`
  - Scope: `cli`
  - Summary: CLI 新增 `decode` 命令，并为 `samples/encode/decode` 增加 `--profile`、`--ecc`、`--canvas`、`--fps`、`--repeat`、`--markers` 参数
  - Compatibility: `breaking`

- Type: `Changed`
  - Scope: `docs`
  - Summary: 将 ISO 协议文档升级为当前有效主线文档，文档索引仅保留真实存在且仍有效的条目；旧 `protocol_v1.md` 改为历史参考
  - Compatibility: `non-breaking`

- Type: `Changed`
  - Scope: `samples`
  - Summary: `bin/samples/README.txt` 改为说明当前样例应通过 `Project1 samples` 实时生成，仓库内旧 PNG 视为历史样例而非当前协议基准
  - Compatibility: `non-breaking`

## 2026-03-08

- Type: `Added`
  - Scope: `samples`
  - Summary: 在 `bin/samples/` 新增 ISO QR V2 示例 `sample_iso_qr_v2_symbol.png` 与标注图 `sample_iso_qr_v2_layout.png`
  - Compatibility: `non-breaking`

- Type: `Added`
  - Scope: `docs`
  - Summary: 新增 `docs/protocols/protocol_iso_qr_v2_course.md`，给出符合课程要求的 ISO QR Version 2 方案（quiet zone、3 finder、1 alignment、信息头与 CRC32）
  - Compatibility: `non-breaking`

- Type: `Added`
  - Scope: `samples`
  - Summary: 在 `bin/samples/` 新增 `QRX-25-3F1A` 示例图片 `sample_qrx_25_3f1a.png`、布局图 `sample_qrx_25_3f1a_layout.png` 和样例清单 `sample_manifest_qrx_25_3f1a.tsv`
  - Compatibility: `non-breaking`

- Type: `Added`
  - Scope: `docs`
  - Summary: 新增 `docs/protocols/protocol_qrx_25_3f1a.md`，定义 `3 Finder + 1 Alignment` 方案的静区、Header、Payload 与 CRC32 规则
  - Compatibility: `non-breaking`

- Type: `Docs`
  - Scope: `docs`
  - Summary: 将 `protocol_v1_optimization_journey.md` 和 `protocol_v1_robust_108x108.md` 归档到 `docs/protocols/`，统一纳入协议与优化过程文档目录
  - Compatibility: `non-breaking`

- Type: `Changed`
  - Scope: `protocol`
  - Summary: 将当前编码基线切换为 `V1.6-108-4F`，启用 `108x108` 四 Finder、`160-bit` header、timing 线、固定单 alignment，以及 `module_px=9` 的渲染方式
  - Compatibility: `breaking`

- Type: `Changed`
  - Scope: `samples`
  - Summary: 样例输出和 manifest 改为声明 `protocol_id=V1.6-108-4F`，并同步记录 `crc32_header`
  - Compatibility: `breaking`

- Type: `Docs`
  - Scope: `docs`
  - Summary: 更新主协议文档和文档索引，使其与新的 `V1.6-108-4F` 编码布局保持一致
  - Compatibility: `non-breaking`

- Type: `Docs`
  - Scope: `docs`
  - Summary: 重组 `docs/` 目录，新增 `docs/README.md` 作为入口索引，将协议文档迁移到 `docs/protocols/protocol_v1.md`
  - Compatibility: `non-breaking`

- Type: `Docs`
  - Scope: `collaboration`
  - Summary: 新增集中式 `docs/CHANGELOG.md`，并在 `AGENTS.md` 中要求每次有效改动同步留存变更记录
  - Compatibility: `non-breaking`
