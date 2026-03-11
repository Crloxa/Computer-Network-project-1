# Computer-Network-project-1

## 当前主线

当前仓库推荐使用的实现是 `V1.6-108-4F` 自定义黑白传输主线：固定 `108x108` 逻辑网格、四角四个 finder、`16x10` header 保留区、两条 timing 线和一个固定 alignment。

当前默认对外口径优先强调两件事：

- 自定义编码方案的 `encoder`
- 用 `samples / demo` 直接生成效果图和测试输出

- 主线文档：`docs/protocols/protocol_v1.md`
- 文档入口：`docs/README.md`
- 变更记录：`docs/CHANGELOG.md`
- 样例说明：`bin/samples/README.txt`
- 默认输出尺寸：`1080x1080`
- 默认模块尺寸：`module_px = 9`
- 默认单帧载荷上限：`1380 bytes/frame`
- 推荐命令：`Project1 encode input.bin out/encode/input`

## 重要提醒

- 当前默认 CLI、README、样例和联调口径都以 `V1.6-108-4F` 为准。
- 现代码保留了 `protocol_iso.*`、`qr_iso.*` 及对应文档，便于回看 `v2` 实现，但它们不再是默认入口。
- 当前 `decode` 只作为仓库内自测与联调辅助，不是默认交付中心。
- 当前 `decode` 的首版验收范围只覆盖仓库自生成的帧目录和自生成 `demo.mp4`，不覆盖拍屏、透视畸变和历史外部样例。
- 视频封装和拆帧仍依赖 `ffmpeg` 可执行程序；若本机没有 `ffmpeg`，编码会保留帧目录和 manifest，但不会产出 `demo.mp4`。
- 所有运行产物统一写到 `out/`，不要提交批量帧图或 demo 视频。

## 阅读入口

- 仓库文档索引：`docs/README.md`
- 当前主线协议：`docs/protocols/protocol_v1.md`
- 历史 `v2` 参考：`docs/protocols/protocol_iso_qr_v2_course.md`
- 历史 `v2` 联调约定：`docs/protocols/protocol_iso_v2_integration_contract.md`
- 变更记录：`docs/CHANGELOG.md`
- 样例说明：`bin/samples/README.txt`

## 常用命令

- 生成样例：`Project1 samples out/samples`
- 图像演示：`Project1 demo input.jpg out/demo/input`
- 默认编码：`Project1 encode input.bin out/encode/input`
- 自测解码：`Project1 decode out/encode/input/frames/physical out/decode/input`

## 当前输出结构

- `samples` 会生成 `layout_guide.*`、`sample_full_frame.*`、`sample_short_frame.*` 和 `sample_manifest.tsv`
- `encode` / `demo` 会生成：
  - `frames/logical/`
  - `frames/physical/`
  - `frame_manifest.tsv`
  - `input_info.txt`
  - `protocol_samples/`
  - `demo.mp4`（若本机有 `ffmpeg`）
  - `video_status.txt`
- `decode` 作为自测辅助会生成：
  - `output.bin`
  - `decode_report.tsv`
  - `decode_summary.txt`
  - `missing_frames.txt`（仅缺帧时）
