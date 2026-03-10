`bin/samples/` 现在同时承载两类内容：当前 v2 固定联调基准，以及旧阶段保留的历史参考文件。

## 当前可直接联调的 v2 基准

固定入口：
- `v2_fixture_index.tsv`

固定基准目录：
- `v2_success/`
- `v2_missing_frame/`
- `v2_crc_error/`

每套目录固定包含：
- `input.bin`
- `frames/`
- `frame_manifest.tsv`
- `decode_args.txt`
- `expected_status.txt`

其中：
- `v2_success/`：完整成功样例，`output.bin` 必须与 `input.bin` 字节完全一致
- `v2_missing_frame/`：缺一帧样例，必须输出 `missing_frames.txt`
- `v2_crc_error/`：单帧 CRC 错误样例，必须在 `decode_report.tsv` 中出现 `crc_mismatch`

基准的默认参数固定为：
- `--profile iso133 --ecc Q --canvas 1440`

重新生成这三套基准：
- 在具备 `qrcode` 和 `Pillow` 的 Python 环境中运行：`python3 scripts/gen_v2_fixtures.py`

## 运行时样例产物

以下仍然是运行 `Project1 samples out/samples` 时动态生成的产物，不作为仓库内固定回归基准：
- `sample_<profile>_symbol.png`
- `sample_<profile>_carrier.png`
- `sample_<profile>_layout.png`
- `sample_manifest.tsv`
- `sample_capacity.tsv`

## 历史参考

以下文件不代表当前 v2 主线，只保留作历史对照：
- `layout_guide.png`
- `sample_full_frame.png`
- `sample_iso_qr_v2_symbol.png`
- `sample_iso_qr_v2_layout.png`
- `sample_manifest_qrx_25_3f1a.tsv`
- `sample_manifest_v1_legacy.tsv`
