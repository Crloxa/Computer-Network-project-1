# 文档索引

`docs/` 目录按“当前主线、历史参考、变更记录”组织，避免协议描述与实现脱节。

## 阅读入口
- 当前有效协议与编解码主线：看 `docs/protocols/protocol_iso_qr_v2_course.md`
- 历史自定义方案参考：看 `docs/protocols/protocol_v1.md`
- 最近发生的有效改动：看 `docs/CHANGELOG.md`
- 样例和运行产物说明：看 `bin/samples/README.txt`

## 当前结构
- `docs/README.md`：文档入口与阅读顺序
- `docs/CHANGELOG.md`：集中式改动记录，最新在前
- `docs/protocols/protocol_iso_qr_v2_course.md`：当前有效的 ISO QR 视频传输主线
- `docs/protocols/protocol_v1.md`：旧的 `V1.6-108-4F` 自定义方案，仅作历史参考

## 协作约定
- 修改协议、代码行为、构建方式、样例资产或文档结构时，必须同步更新 `docs/CHANGELOG.md`
- 协议正文只保留当前有效规则；被替代方案移入历史参考，不再继续扩写
- 若样例图片不是由当前代码生成，必须明确标注为历史样例，不能冒充当前主线基准

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
