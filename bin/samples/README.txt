`bin/samples/` 下当前已提交的 PNG/TSV 来自旧阶段试验，不再代表现行主线协议。

当前有效的 ISO 主线样例请使用以下命令实时生成：
- `Project1 samples out/samples`

运行后会生成：
- `sample_iso109_symbol.png`
- `sample_iso109_carrier.png`
- `sample_iso109_layout.png`
- `sample_iso145_symbol.png`
- `sample_iso145_carrier.png`
- `sample_iso145_layout.png`
- `sample_iso177_symbol.png`
- `sample_iso177_carrier.png`
- `sample_iso177_layout.png`
- `sample_manifest.tsv`

说明：
- 中央 QR 为真实标准 ISO QR。
- 外围 carrier 仅用于视频定位与透视矫正，不属于 QR 本体标准。
- 旧 `layout_guide.png`、`sample_full_frame.png`、`sample_iso_qr_v2_*`、`sample_manifest_qrx_25_3f1a.tsv` 仅作历史参考。
