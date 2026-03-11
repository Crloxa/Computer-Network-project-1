`bin/samples/` 现在以 `V1.6-108-4F` 为默认样例口径，仓库内仍保留少量历史 `v2` 文件用于回看，但不再作为当前主线基准。

默认展示和汇报时，优先使用这里对应的 `samples / demo` 输出，不把 `decode` 产物当成第一叙事中心。

## 当前主线样例

通过 `Project1 samples out/samples` 动态生成的当前主线样例包括：

- `layout_guide.*`
- `sample_full_frame.*`
- `sample_short_frame.*`
- `sample_manifest.tsv`

这些样例主要用于：

- 确认 `108x108 / 4 finder / timing / alignment` 版式是否正确
- 校对 `16x10` header 保留区位置
- 作为仓库内自测 `decode` 时的版式参考

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

这些文件只保留作历史参考，不应再被当作 `V1.6-108-4F` 的默认验收基准。
