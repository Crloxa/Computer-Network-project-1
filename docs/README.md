# 文档索引

`docs/` 目录按“当前主线、历史参考、变更记录”组织，避免协议描述与实现脱节。

## 阅读入口
- 当前有效的工程设计、协议与编解码主线：看 `docs/protocols/protocol_iso_qr_v2_course.md`
  - 说明：当前代码已经切到“自研 QR 本体 + 现有 isoqrv2 外部契约”实现，首版只支持 `iso133 / Q / markers=on`
  - 说明：仓库仍保留 `include/`、`lib/`、`bin/` 下的历史第三方资产用于回溯旧版本，但当前主链不再直接依赖它们
- 编码端-解码端联调约定（当前执行口径）：看 `docs/protocols/protocol_iso_v2_integration_contract.md`
- 可优化选项（非默认主线）：看 `docs/protocols/protocol_rgb_4color_option.md`
- 历史自定义方案参考：看 `docs/protocols/protocol_v1.md`
- 最近发生的有效改动：看 `docs/CHANGELOG.md`
- 样例和运行产物说明：看 `bin/samples/README.txt`
- 说明：`docs/protocols/protocol_iso_qr_v2_course.md` 文件名中的 `v2` 是历史命名沿用，不表示当前默认二维码符号版本是 QR Version 2。

## 当前结构
- `docs/README.md`：文档入口与阅读顺序
- `docs/CHANGELOG.md`：集中式改动记录，最新在前
- `docs/protocols/protocol_iso_qr_v2_course.md`：当前有效的 ISO QR v2 主线设计与实现说明（现代码为自研实现）
- `docs/protocols/protocol_iso_v2_integration_contract.md`：v2 主线联调约定与执行清单
- `docs/protocols/protocol_rgb_4color_option.md`：四色增强承载方案（可优化选项，非默认主线）
- `docs/protocols/protocol_v1.md`：旧的 `V1.6-108-4F` 自定义方案，仅作历史参考
- 当前默认工作点为 `iso133 / Version 29 / 133x133 + ECC Q`，并且当前代码仅支持这一组工作点。

## 协作约定
- 修改协议、代码行为、构建方式、样例资产或文档结构时，必须同步更新 `docs/CHANGELOG.md`
- 协议正文只保留当前有效规则；被替代方案移入历史参考，不再继续扩写
- 可优化方案必须明确标注“非默认主线”，并与当前有效协议文档分开维护
- 若样例图片不是由当前代码生成，必须明确标注为历史样例，不能冒充当前主线基准

## 推荐阅读顺序
1. 先读 `docs/protocols/protocol_iso_qr_v2_course.md`，理解当前主线的接口、流程、产物和限制
2. 再读 `docs/protocols/protocol_iso_v2_integration_contract.md`，按联调口径执行编码端与解码端对齐
3. 再读 `bin/samples/README.txt`，确认样例和运行产物的来源
4. 最后读 `docs/CHANGELOG.md`，查看近期变更

## 常用联调路径
- 运行输出：`out/`
- 当前样例生成命令：
  - `Project1 samples out/samples`
- 当前编码命令：
  - `Project1 encode input.bin out/encode/input --profile iso133 --ecc Q --canvas 1440`
- 当前解码命令：
  - `Project1 decode out/encode/input/demo.mp4 out/decode/input --profile iso133 --ecc Q --canvas 1440`
- 4K 录制推荐命令：
  - `Project1 encode input.bin out/encode/input_4k --profile iso133 --ecc Q --canvas 2160`
