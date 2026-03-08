# Repository Guidelines

## 项目结构与模块组织
- `src/` 存放当前有效的 C++ 源码，现阶段协议相关实现主要在 `protocol_v1.*`、`encoder.*` 和 `main.cpp`。
- `docs/` 存放协议说明和对接文档，例如 `docs/protocol_v1.md`。
- `bin/samples/` 存放小体积、可复现的协议基准样例，用于解码联调和版式确认。
- `include/`、`lib/`、`bin/` 保存已入库的 OpenCV 头文件、库文件和 DLL，供 Visual Studio 工程直接使用。
- `ffmpeg/` 预留给本地 FFmpeg 可执行文件。
- 所有运行时产物统一输出到 `out/`；该目录已被忽略，不应提交。

## 构建、测试与开发命令
- 在 Visual Studio 中打开 `Project1.sln`，使用 `Debug|x64` 或 `Release|x64` 构建。
- 编码器命令行入口：
  - `Project1 samples out/samples`
  - `Project1 demo input.jpg out/demo/input`
  - `Project1 encode input.bin out/encode/input`
- macOS / Linux 下可用轻量语法检查：
  - `clang++ -std=c++17 -Iinclude -Isrc -fsyntax-only src/main.cpp src/encoder.cpp src/protocol_v1.cpp`
- 手工验证时，所有生成文件都放在 `out/`，不要提交批量帧图或 demo 视频。

## 代码风格与命名规范
- 使用 C++17，统一 4 空格缩进。
- 优先拆分为小而明确的辅助函数，避免把协议、绘制、IO 混在一个大函数里。
- 类型和结构体使用 `PascalCase`，函数使用 `camelCase`，常量优先使用清晰的 `kCamelCase`。
- 协议字段名在代码和文档中保持一致，例如 `frame_seq`、`payload_len_bytes`、`crc32_payload`。
- 不要随意修改 `include/`、`lib/`、`bin/` 下的第三方依赖内容。

## 测试要求
- 当前仓库没有独立单元测试框架，提交前至少完成以下验证：
  - 工程可成功构建；
  - 能生成样例帧或 demo 视频；
  - Header 解析、payload 长度和 CRC 行为与 `docs/protocol_v1.md` 一致。
- 如果新增协议逻辑，只有在样例具有稳定参考价值时，才向 `bin/samples/` 增加少量基准文件。

## 提交与合并请求规范
- 历史提交较混杂，后续请使用清晰、直接的祈使句式提交信息，例如：`Add CRC32 header packing for protocol v1`。
- 尽量保持提交粒度单一：协议变更、工程配置变更、样例资产更新应尽量分开提交。
- Pull Request 至少应包含：
  - 变更摘要；
  - 影响的目录或文件；
  - 若涉及版式或渲染修改，附样例输出或截图；
  - 若涉及协议兼容性，明确说明是否破坏现有解码逻辑。
