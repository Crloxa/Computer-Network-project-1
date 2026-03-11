# Computer-Network-project-1

## 当前主线

当前仓库推荐使用的实现是自研 `ISO QR v2` 视频传输主线：二维码本体保持 `Version 29 / 133x133 + ECC Q` 版式，但编码解码不再依赖 OpenCV 或现成 QR 库。

- 主线文档：`docs/protocols/protocol_iso_qr_v2_course.md`
- 联调约定：`docs/protocols/protocol_iso_v2_integration_contract.md`
- 可优化选项：`docs/protocols/protocol_rgb_4color_option.md`（非默认主线）
- 默认工作点：`Version 29 / 133x133 + ECC Q`
- 当前代码限制：`encode/decode` 首版仅支持 `--profile iso133 --ecc Q --markers on`
- 推荐命令：`Project1 encode input.bin out/encode/input --profile iso133 --ecc Q --canvas 1440`
- 说明：`protocol_iso_qr_v2_course.md` 中的 `v2` 是历史命名沿用，不表示当前默认二维码符号版本是 QR Version 2。

## 重要提醒

- 当前主线已经切到自研的 `ISO QR v2` 实现，二维码本体仍遵循 `Version 29 / 133x133 + ECC Q` 这套版式。
- 当前默认实现不再调用 OpenCV 或现成 QR 编解码库；图像桥接与视频封装/拆帧依赖 `ffmpeg` 可执行程序。
- 仓库中的 `include/`、`lib/`、`bin/` 仍保留历史第三方资产用于回溯旧版本，但当前主执行链路不再直接依赖这些目录。
- 后续开发、联调、验收、截图和汇报，默认都应基于这条 ISO 主线进行。
- 旧的 `V1.6-108-4F` 自定义方案仅保留作历史参考，不推荐继续作为日常运行版本。
- 四色增强承载方案属于可优化选项，当前默认交付与验收仍以黑白 ISO 主线为准。
- 编码端与解码端联调请按 `docs/protocols/protocol_iso_v2_integration_contract.md` 执行，避免口径不一致。
- 若其他 agent 或组员需要了解当前实现，应优先阅读 `docs/README.md` 与 `docs/protocols/protocol_iso_qr_v2_course.md`，不要默认沿用老版本逻辑。

## 阅读入口

- 仓库文档索引：`docs/README.md`
- 当前主线设计文档：`docs/protocols/protocol_iso_qr_v2_course.md`
- v2 联调约定文档：`docs/protocols/protocol_iso_v2_integration_contract.md`
- 可优化方案文档：`docs/protocols/protocol_rgb_4color_option.md`
- 历史方案参考：`docs/protocols/protocol_v1.md`
- 变更记录：`docs/CHANGELOG.md`
- 样例说明：`bin/samples/README.txt`

## 常用命令

- 生成样例：`Project1 samples out/samples`
- 默认编码：`Project1 encode input.bin out/encode/input --profile iso133 --ecc Q --canvas 1440`
- 默认解码：`Project1 decode out/encode/input/demo.mp4 out/decode/input --profile iso133 --ecc Q --canvas 1440`
- 4K 推荐：`Project1 encode input.bin out/encode/input_4k --profile iso133 --ecc Q --canvas 2160`
