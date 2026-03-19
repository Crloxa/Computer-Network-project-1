# 文档索引

`docs/` 目录按“当前主线、历史参考、变更记录”组织，避免协议描述与实现脱节。

## 项目概述

本项目是一个**计算机网络课程设计**，实现了一套自定义的**视觉二维码视频传输协议**（V1.6-108-4F），用于将任意二进制文件编码为一段视频，解码端通过摄像头或视频文件还原原始数据。

**编码端工作流程：**
1. 将输入文件按 `BytesPerFrame = 1878` 字节分帧
2. 每帧在 `133×133` 逻辑坐标上绘制：四角 Finder 图案、帧控制头（帧类型 + 校验码 + 帧序号）、数据 payload
3. 将逻辑帧以 `10×` 放大率渲染为 `1330×1330` PNG，再经 FFmpeg 合成为 MP4 视频

**解码端工作流程：**
1. 读取帧图像，可选经 `ImgParse`（pic.cpp）做透视矫正，还原为 `133×133` 逻辑帧
2. 从头部读取帧类型与 16-bit 校验码，从 payload 区读取数据字节
3. 集齐所有帧后按帧序号拼合，恢复原始二进制文件

**主要源文件：**

| 文件 | 职责 |
|------|------|
| `src/frame_constants.h` | 协议几何常量、公共类型与单元格工具（编解码共享）|
| `src/code.h` / `src/code.cpp` | 编码器：绘制帧、写入 payload、输出 PNG 序列 |
| `src/ImgDecode.h` / `src/ImgDecode.cpp` | 解码器：读取帧、解析头部、提取 payload |
| `src/pic.h` / `src/pic.cpp` | 图像预处理：Finder 检测与透视矫正 |
| `src/ffmpeg.h` / `src/ffmpeg.cpp` | 调用本地 FFmpeg 将帧序列合成/分解视频 |
| `src/main.cpp` | CLI 入口：`samples` / `demo` / `encode` / `decode` 命令 |

---

## 阅读入口

- 当前有效的工程设计、协议与编解码主线：看 `docs/protocols/protocol_v1.md`
  - 说明：当前代码已经切回 `V1.6-108-4F`
  - 说明：头部语义采用最简控制头，`16x10` 保留区仅使用前 3 行
  - 说明：默认交付叙事优先强调 `encoder + samples/demo`，`decode` 只保留作仓库内自测辅助
- Windows 运行与排障：看 `docs/windows_quickstart.md`
  - 说明：给 PowerShell、Visual Studio、`ffmpeg.exe` 路径和常见报错提供可直接照抄的保姆级指引
- 历史 `v2` 主线参考：看 `docs/protocols/protocol_iso_qr_v2_course.md`
- 历史 `v2` 联调约定：看 `docs/protocols/protocol_iso_v2_integration_contract.md`
- 可优化选项（非默认主线）：看 `docs/protocols/protocol_rgb_4color_option.md`
- 最近发生的有效改动：看 `docs/CHANGELOG.md`
- 样例和运行产物说明：看 `bin/samples/README.txt`

## 当前结构

- `docs/README.md`：文档入口与阅读顺序
- `docs/CHANGELOG.md`：集中式改动记录，最新在前
- `docs/protocols/protocol_v1.md`：当前有效的 `V1.6-108-4F` 主线协议说明
- `docs/windows_quickstart.md`：Windows 构建、运行与排障手册
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
2. 若你在 Windows 上运行，马上读 `docs/windows_quickstart.md`
3. 再读 `bin/samples/README.txt`，确认样例和运行产物的来源
4. 最后读 `docs/CHANGELOG.md`，查看近期变更

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
