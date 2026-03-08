# 文档索引

`docs/` 目录按“索引、规范、历史”组织，减少多人协作和 agent 读取时的定位成本。

## 阅读入口
- 新同学或首次接手：先看 `docs/README.md`
- 协议/编解码实现：看 `docs/protocols/protocol_v1.md`
- 最近发生了什么变化：看 `docs/CHANGELOG.md`
- 协议基准样例：看 `bin/samples/`

## 当前结构
- `docs/README.md`：文档入口与阅读顺序
- `docs/CHANGELOG.md`：集中式改动记录，最新在前
- `docs/protocols/protocol_v1.md`：当前有效的 108x108 协议规范

## 协作约定
- 修改协议、代码行为、构建方式、样例资产或文档结构时，必须同步更新 `docs/CHANGELOG.md`
- 新增或移动文档时，必须同步维护本文件
- 协议正文只保留当前有效规则，历史变化统一记录到 changelog，不在正文堆叠旧版本说明

## 常用联调路径
- 样例图片：`bin/samples/`
- 运行输出：`out/`
- 编码命令：
  - `Project1 samples out/samples`
  - `Project1 demo input.jpg out/demo/input`
  - `Project1 encode input.bin out/encode/input`
