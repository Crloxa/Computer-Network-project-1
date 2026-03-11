# Changelog

本文件记录影响协议、代码行为、构建方式、样例资产或文档结构的有效改动。

维护规则：
- 采用集中式记录，统一维护在本文件
- 最新记录放在最前面
- 每条记录至少写明：日期、类型、范围、摘要、兼容性
- 纯格式化、空白调整、错别字修正可不记录

## 2026-03-11

- Type: `Changed`
  - Scope: `code`
  - Summary: 将 ISO QR v2 主链从 OpenCV / 现成 QR 编解码切换为仓库内自研 `Version 29 / 133x133 + ECC Q` 实现，并新增 `simple_image.*` 与 `qr_iso_v29.*` 支撑样例、编码、视频桥接和解码流程
  - Compatibility: `breaking`

- Type: `Changed`
  - Scope: `protocol`
  - Summary: 统一 `Version 29 / ECC Q` 的真实容量口径为 `max_frame_bytes=908`、`max_payload_bytes=896`，并移除仓库中残留的旧 `698/686` 口径
  - Compatibility: `breaking`

- Type: `Changed`
  - Scope: `layout`
  - Summary: 调整 carrier 布局，要求四角 marker 与 QR 本体完全分离，避免 marker 覆盖 quiet zone、finder 或数据区，并恢复 `encode -> decode` 的自举闭环
  - Compatibility: `breaking`

- Type: `Changed`
  - Scope: `build`
  - Summary: 更新 Visual Studio 工程编译项，移除主执行链路对 OpenCV 头库的直接依赖，改为编译自研图像与 QR 模块
  - Compatibility: `breaking`

- Type: `Changed`
  - Scope: `docs`
  - Summary: 更新根 `README.md`、`docs/README.md` 与 `docs/protocols/protocol_iso_qr_v2_course.md`，明确当前实现是“自研 QR 本体 + 现有 isoqrv2 外部契约”，首版仅支持 `iso133 / Q / markers=on`
  - Compatibility: `non-breaking`

- Type: `Changed`
  - Scope: `docs`
  - Summary: 清理联调文档中残留的 `QRCodeDetector/OpenCV` 旧表述，并补充“历史第三方资产仍保留在仓库中但不属于当前主执行链路”的说明
  - Compatibility: `non-breaking`

- Type: `Added`
  - Scope: `docs`
  - Summary: 新增 `docs/protocols/protocol_iso_v2_integration_contract.md`，结合 v2 主线固化联调输入契约、输出口径、失败语义与 10 项联调清单推荐内容
  - Compatibility: `non-breaking`

- Type: `Changed`
  - Scope: `docs`
  - Summary: 更新 `docs/README.md` 与根 `README.md` 的阅读入口，新增 v2 联调约定文档索引
  - Compatibility: `non-breaking`

- Type: `Changed`
  - Scope: `performance`
  - Summary: `DecodeIsoPackage` 改为流式处理视频/帧目录，复用 QR detector，并新增 `--protocol-samples on|off` 与 `--decode-debug on|off` 开关以支持快速路径
  - Compatibility: `non-breaking`

- Type: `Changed`
  - Scope: `decode`
  - Summary: `DecodeIsoPackage` 改为在成功、缺帧、CRC 错误、无有效帧和读入失败场景下都稳定输出 `decode_report.tsv` 与 `decode_summary.txt`，并将重复帧显式标记为 `duplicate_frame`
  - Compatibility: `non-breaking`

- Type: `Added`
  - Scope: `samples`
  - Summary: 在 `bin/samples/` 新增 `v2_success`、`v2_missing_frame`、`v2_crc_error` 三套当前 ISO 主线联调基准，以及 `v2_fixture_index.tsv` 与再生成脚本 `scripts/gen_v2_fixtures.py`
  - Compatibility: `non-breaking`

- Type: `Changed`
  - Scope: `samples`
  - Summary: 将旧阶段的 `bin/samples/sample_manifest.tsv` 改名为 `sample_manifest_v1_legacy.tsv`，避免与当前 ISO 主线样例产物的 `sample_manifest.tsv` 命名冲突
  - Compatibility: `non-breaking`

## 2026-03-10

- Type: `Added`
  - Scope: `docs`
  - Summary: 新增 `docs/protocols/protocol_rgb_4color_option.md`，记录“基础 ISO QR + 四色副载层”的可优化方案，并明确其为非默认主线
  - Compatibility: `non-breaking`

- Type: `Changed`
  - Scope: `docs`
  - Summary: 更新 `docs/README.md` 与根 `README.md` 的阅读入口，新增四色可优化文档索引并标注“可优化选项”
  - Compatibility: `non-breaking`

## 2026-03-09

- Type: `Changed`
  - Scope: `docs`
  - Summary: 将 `protocol_iso_qr_v2_course.md` 从方案摘要补全为工程交接文档，覆盖模块职责、公共接口、输出文件字段、失败模式、排障路径和已知限制
  - Compatibility: `non-breaking`

- Type: `Changed`
  - Scope: `protocol`
  - Summary: ISO 主线默认 profile 从 `iso145` 调整为 `iso133`（`Version 29 / 133x133`），并新增 `ECC=H` 支持以对齐组内实测档位与更高拍摄容错需求
  - Compatibility: `breaking`

- Type: `Added`
  - Scope: `samples`
  - Summary: `Project1 samples` 新增 `sample_capacity.tsv`，输出 `iso109/iso133/iso145/iso177` 在 `M/Q/H` 下的容量矩阵，并将 `iso133 + Q` 标记为推荐工作点
  - Compatibility: `non-breaking`

- Type: `Changed`
  - Scope: `decode`
  - Summary: `decode_report.tsv` 增加 `profile` 与 `ecc` 列，便于比较 `Version 29` 在 `Q/H` 下的解码表现
  - Compatibility: `non-breaking`

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
  - Summary: 新增 `docs/protocols/protocol_iso_qr_v2_course.md`，给出符合课程要求的标准 ISO QR 主线课程方案文档（quiet zone、3 finder、1 alignment、信息头与 CRC32）
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
