`bin/samples/` 现在以 `protocol_v1` 当前主线为默认样例口径，仓库内仍保留少量历史 `v2` 文件用于回看，但不再作为当前主线基准。

默认展示和汇报时，优先使用这里对应的 `samples / demo` 输出，不把 `decode` 产物当成第一叙事中心。

## 当前主线样例

通过 `Project1 samples out/samples` 动态生成的当前主线样例包括：

- `layout_guide.*`
- `sample_full_frame.*`
- `sample_short_frame.*`
- `sample_manifest.tsv`
- `protocol_v1_v1_7_133_layout_cn.svg`（仓库内静态中文标注示意图，用于汇报和组内对齐）

这些样例主要用于：

- 确认当前 `protocol_v1` 的三主定位器 / 右下辅助定位 / timing 版式是否正确
- 校对当前 header 保留区位置和字段含义
- 作为仓库内自测 `decode` 时的版式参考
- 在组内展示时快速解释“定位区 / 帧头 / 数据区”的分工

## 当前自测回环基准

当前 `decode` 只作为自测辅助。它的默认回环输入不是仓库内静态 PNG，而是由当前编码器生成的运行产物：

- `out/<case>/encode/frames/physical/`
- `out/<case>/encode/demo.mp4`

当前首版只保证这两类输入可回环，不保证拍屏、透视畸变或历史外部样例。

## 历史参考

以下内容仍可能出现在仓库中，但都不代表当前默认主线：

- `v2_success/`
- `v2_missing_frame/`
- `v2_crc_error/`
- `v2_fixture_index.tsv`
- 旧阶段的 ISO / QR / layout 对照图

这些文件只保留作历史参考，不应再被当作当前 `protocol_v1` 主线的默认验收基准。
