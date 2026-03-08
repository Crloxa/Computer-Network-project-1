# Changelog

本文件记录影响协议、代码行为、构建方式、样例资产或文档结构的有效改动。

维护规则：
- 采用集中式记录，统一维护在本文件
- 最新记录放在最前面
- 每条记录至少写明：日期、类型、范围、摘要、兼容性
- 纯格式化、空白调整、错别字修正可不记录

## 2026-03-08

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
