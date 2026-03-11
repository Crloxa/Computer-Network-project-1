# 文档索引

`docs/` 目录按“当前主线、历史参考、变更记录”组织，避免协议描述与实现脱节。

## 阅读入口

- 当前有效的工程设计、协议与编解码主线：看 `docs/protocols/protocol_v1.md`
  - 说明：当前代码已经切回 `V1.6-108-4F`
  - 说明：头部语义采用最简控制头，`16x10` 保留区仅使用前 3 行
  - 说明：默认交付叙事优先强调 `encoder + samples/demo`，`decode` 只保留作仓库内自测辅助
- 历史 `v2` 主线参考：看 `docs/protocols/protocol_iso_qr_v2_course.md`
- 历史 `v2` 联调约定：看 `docs/protocols/protocol_iso_v2_integration_contract.md`
- 可优化选项（非默认主线）：看 `docs/protocols/protocol_rgb_4color_option.md`
- 最近发生的有效改动：看 `docs/CHANGELOG.md`
- 样例和运行产物说明：看 `bin/samples/README.txt`

## 当前结构

- `docs/README.md`：文档入口与阅读顺序
- `docs/CHANGELOG.md`：集中式改动记录，最新在前
- `docs/protocols/protocol_v1.md`：当前有效的 `V1.6-108-4F` 主线协议说明
- `docs/protocols/protocol_iso_qr_v2_course.md`：历史 `v2` 方案说明，代码仍保留但不默认暴露
- `docs/protocols/protocol_iso_v2_integration_contract.md`：历史 `v2` 联调约定
- `docs/protocols/protocol_rgb_4color_option.md`：四色增强承载方案（非默认主线）

## 协作约定

- 修改协议、代码行为、构建方式、样例资产或文档结构时，必须同步更新 `docs/CHANGELOG.md`
- 当前有效协议文档只保留真实默认行为；被替代方案移入历史参考，不再继续扩写
- 历史 `v2` 资料可以保留，但必须明确标注为“非默认主线”
- 若样例图片不是由当前代码生成，必须明确标注为历史样例，不能冒充当前主线基准

## 推荐阅读顺序

1. 先读 `docs/protocols/protocol_v1.md`，理解当前主线的几何布局、头部、编码和样例输出口径
2. 再读 `bin/samples/README.txt`，确认样例和运行产物的来源
3. 最后读 `docs/CHANGELOG.md`，查看近期变更

## 常用联调路径

- 运行输出：`out/`
- 当前样例生成命令：
  - `Project1 samples out/samples`
- 当前编码命令：
  - `Project1 encode input.bin out/encode/input`
- 当前图像演示命令：
  - `Project1 demo input.jpg out/demo/input`
- 当前自测解码命令：
  - `Project1 decode out/encode/input/frames/physical out/decode/input`
