# Computer-Network-project-1

## 当前主线

当前仓库推荐使用的实现是 `V1.6-133-4F` 自定义黑白传输主线：固定 `133x133` 逻辑网格、三个大角点 Finder + 一个小 Alignment、`16x3` header 保留区、2 像素安全边框。

当前默认对外口径优先强调两件事：

- 自定义编码方案的 `encoder`
- 用 `samples / demo` 直接生成效果图和测试输出

- 主线文档：`docs/protocols/protocol_v1.md`
- 文档入口：`docs/README.md`
- Windows 运行指引：`docs/windows_quickstart.md`
- 变更记录：`docs/CHANGELOG.md`
- 样例说明：`bin/samples/README.txt`
- 默认输出尺寸：`1330x1330`（逻辑帧 133×133，放大 10 倍）
- 默认模块尺寸：`module_px = 10`
- 默认单帧载荷上限：`1878 bytes/frame`
- 推荐命令：`Project1 encode input.bin out/encode/input`

## 重要提醒

- 当前默认 CLI、README、样例和联调口径都以 `V1.6-133-4F` 为准。
- 现代码保留了 `protocol_iso.*`、`qr_iso.*` 及对应文档，便于回看 `v2` 实现，但它们不再是默认入口。
- 当前 `decode` 只作为仓库内自测与联调辅助，不是默认交付中心。
- 当前 `decode` 的首版验收范围只覆盖仓库自生成的帧目录和自生成 `demo.mp4`，不覆盖拍屏、透视畸变和历史外部样例。
- 视频封装和视频拆帧仍依赖 `ffmpeg` 可执行程序。
- 若本机没有 `ffmpeg`，`samples` / `encode` 仍会生成 BMP 与 manifest，但不会产出 `demo.mp4`，也可能跳过 `png_mirror/`。
- `decode` 对 BMP 帧目录不依赖 `ffmpeg`；对视频、PNG、JPG 输入依赖 `ffmpeg`。
- 所有运行产物统一写到 `out/`，不要提交批量帧图或 demo 视频。

## 阅读入口

- 仓库文档索引：`docs/README.md`
- Windows 专项说明：`docs/windows_quickstart.md`
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

## Windows PowerShell 速记

- 必须先在 Visual Studio 中构建 `x64|Debug` 或 `x64|Release`
- 推荐从仓库根目录运行，不要直接假设 `Project1.exe` 已在 `PATH`
- 直接运行可执行文件：
  - `.\x64\Debug\Project1.exe --version`
  - `.\x64\Debug\Project1.exe encode input.jpg out\encode\input`
- 使用仓库脚本：
  - `.\scripts\run_project1.ps1 encode input.jpg out\encode\input`
  - `.\scripts\run_project1.ps1 decode out\encode\input\frames\physical out\decode\input`
- 组员若仍复制旧命令里的 `--profile/--ecc/--canvas`，当前程序会接受并警告“这些参数已被忽略”，实际仍按 `V1.6-108-4F` 执行

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
