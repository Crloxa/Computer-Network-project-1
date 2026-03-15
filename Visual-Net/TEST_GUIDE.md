# Visual-Net 测试指南

## 1. 文档目的

本文档用于说明 `Visual-Net` 模块当前的测试方法，并明确本次修改范围。

本次针对 `Visual-Net` 的编解码逻辑进行了修改，主要涉及以下文件：

- `src/code.cpp`
- `src/ImgDecode.cpp`
- `src/pic.cpp`

其中：

- `code.cpp` 负责编码侧二维码帧生成与数据写入逻辑
- `ImgDecode.cpp` 负责解码侧图像信息解析与还原逻辑
- `pic.cpp` 负责图像定位、裁剪、预处理等相关处理

`ffmpeg` 仍然使用项目原有版本，未在本次修改中替换或重构。测试时继续使用 `Visual-Net/bin/ffmpeg/` 下已有程序即可。

## 2. 测试前说明

测试请先进入 `Visual-Net` 目录，再进行后续操作：

```powershell
cd d:\计算机网络实验\Lab1\Computer-Network-project-1\Visual-Net
```

说明：

- 源码查看、修改、编译入口在 `Visual-Net`
- 实际运行编码器和解码器建议在 `Visual-Net/bin` 下进行
- 原因是程序调用 `ffmpeg` 时使用的是相对路径，运行目录不对时可能找不到 `ffmpeg.exe`

进入运行目录：

```powershell
cd .\bin
```

## 3. 修改范围说明

本次测试重点验证的是 `Visual-Net` 内部编解码部分是否工作正常，重点不是 `ffmpeg` 本身。

测试关注点如下：

1. 编码阶段能否正确把输入文件转换为二维码图像序列或视频
2. 解码阶段能否正确从视频中提取图像并恢复原始数据
3. 修改后的 `code`、`ImgDecode`、`pic` 三部分是否能够协同工作
4. 原有 `ffmpeg` 调用链是否仍可正常使用

## 4. 测试环境要求

建议确认以下内容：

- 当前目录为 `Visual-Net/bin`
- `encoder.exe`、`decoder.exe` 存在
- `ffmpeg\bin\ffmpeg.exe` 存在
- 相关 `dll` 文件存在于 `bin` 目录下

当前项目内已有如下测试程序：

- `Visual-Net/bin/encoder.exe`
- `Visual-Net/bin/decoder.exe`

## 5. 测试步骤

### 5.1 编码测试

目的：验证修改后的编码逻辑是否能够正常工作。

操作方法：

```powershell
.\encoder.exe <输入文件路径> <输出视频路径> <最长视频时长ms> [传输帧率]
```

示例：

```powershell
.\encoder.exe ..\README.md output.mp4 10000 15
```

参数说明：

- `<输入文件路径>`：待编码的原始文件
- `<输出视频路径>`：编码后生成的视频文件，建议使用 `.mp4`
- `<最长视频时长ms>`：最大视频时长，单位为毫秒
- `[传输帧率]`：可选参数，建议不超过 `15`

预期结果：

- 成功生成输出视频
- 过程中会临时生成图像帧
- 程序可正常调用原有 `ffmpeg` 完成图像转视频

### 5.2 解码测试

目的：验证修改后的解码逻辑是否能够从视频恢复原始文件。

操作方法：

```powershell
.\decoder.exe <输入视频路径> <输出文件路径>
```

示例：

```powershell
.\decoder.exe output.mp4 decoded_readme.md
```

预期结果：

- 程序可正常抽取视频帧
- 程序能调用原有 `ffmpeg` 将视频拆分为图片
- 修改后的 `pic.cpp` 与 `ImgDecode.cpp` 能完成图像定位、解析和数据恢复
- 最终生成解码后的输出文件

### 5.3 编解码一致性验证

目的：验证编码结果经过解码后是否与原文件一致。

操作方法：

1. 先执行编码测试，生成 `output.mp4`
2. 再执行解码测试，生成恢复后的文件
3. 对比原文件与恢复文件是否一致

在 PowerShell 中可使用：

```powershell
fc /b ..\README.md .\decoded_readme.md
```

预期结果：

- 若输出为空或提示两个文件一致，说明编解码测试通过
- 若存在字节差异，则说明编码、解码或图像处理链路仍需继续排查

## 6. 推荐测试流程

建议按以下顺序执行：

1. 进入 `Visual-Net`
2. 进入 `Visual-Net/bin`
3. 使用一个较小文件先做编码测试
4. 使用生成的视频做解码测试
5. 比较原文件与恢复文件
6. 若通过，再更换更大的文件重复测试

推荐命令示例：

```powershell
cd d:\计算机网络实验\Lab1\Computer-Network-project-1\Visual-Net
cd .\bin
.\encoder.exe ..\README.md output.mp4 10000 15
.\decoder.exe output.mp4 decoded_readme.md
fc /b ..\README.md .\decoded_readme.md
```

## 7. 注意事项

- 测试前请确认当前运行目录正确，建议在 `Visual-Net/bin` 下执行
- `ffmpeg` 使用项目原有版本，不需要额外替换
- 若程序提示找不到 `ffmpeg.exe`，通常是运行目录不正确
- 若解码失败，可先检查生成的视频是否完整
- 若使用手机拍摄视频进行解码测试，应尽量保证二维码区域完整、清晰、无遮挡

## 8. 结论说明模板

测试文档中可使用如下表述：

> 本次对 `Visual-Net` 的编解码模块进行了修改，主要涉及 `code`、`ImgDecode` 和 `pic` 相关实现；`ffmpeg` 部分继续沿用原项目版本。测试时需先进入 `Visual-Net` 目录，并在 `Visual-Net/bin` 下执行编码与解码程序，按“编码 -> 解码 -> 文件一致性比对”的流程完成验证。

## 9. FFmpeg 功能模块说明

### 9.1 功能概述

本次开发中，对 `ffmpeg` 模块进行了封装和扩展，提供了更灵活的音视频处理功能，主要包括：

- 视频转图片（支持自定义输出分辨率）
- 图片转视频（支持自定义帧率和码率）
- 图片缩放（支持任意尺寸调整）

### 9.2 函数接口

#### 9.2.1 视频转图片

```cpp
int VideotoImage(const char* videoPath,     // 输入视频路径
                 const char* imagePath,     // 输出图片目录
                 const char* imageFormat,   // 图片格式（如 "jpg", "png"）
                 int width = -1,            // 输出宽度（-1 表示保持原始宽度）
                 int height = -1);          // 输出高度（-1 表示保持原始高度）
```

**功能**：将视频文件拆分为一系列图片，可选择调整输出图片的分辨率。

**参数说明**：
- `videoPath`：输入视频文件的路径（支持相对路径和绝对路径）
- `imagePath`：输出图片的目录路径，若不存在会自动创建
- `imageFormat`：输出图片的格式，如 "jpg"、"png" 等
- `width`：输出图片的宽度，-1 表示保持原始宽度
- `height`：输出图片的高度，-1 表示保持原始高度

**返回值**：0 表示成功，非 0 表示失败。

#### 9.2.2 图片转视频

```cpp
int ImagetoVideo(const char* imagePath,     // 输入图片目录
                 const char* imageFormat,   // 图片格式（如 "jpg", "png"）
                 const char* videoPath,     // 输出视频路径
                 unsigned rawFrameRates = 30,       // 输入图片序列的帧率
                 unsigned outputFrameRates = 30,    // 输出视频的帧率
                 unsigned kbps = 0);                // 输出视频码率（0 表示自动）
```

**功能**：将一系列按序号命名的图片合成为视频文件。

**参数说明**：
- `imagePath`：包含按序号命名图片的目录路径
- `imageFormat`：图片的格式，如 "jpg"、"png" 等
- `videoPath`：输出视频文件的路径
- `rawFrameRates`：输入图片序列的帧率，默认为 30
- `outputFrameRates`：输出视频的帧率，默认为 30
- `kbps`：输出视频的码率（单位：kbps），0 表示自动

**返回值**：0 表示成功，非 0 表示失败。

#### 9.2.3 图片缩放

```cpp
int ScaleImage(const char* inputPath,   // 输入图片路径
               const char* outputPath,  // 输出图片路径
               int width,               // 目标宽度
               int height);             // 目标高度
```

**功能**：调整图片的尺寸，保持原始宽高比并进行适当填充。

**参数说明**：
- `inputPath`：输入图片的路径
- `outputPath`：输出图片的路径
- `width`：目标宽度
- `height`：目标高度

**返回值**：0 表示成功，非 0 表示失败。

### 9.3 调用流程

#### 9.3.1 基本调用示例

```cpp
// 1. 视频转图片（保持原始分辨率）
FFMPEG::VideotoImage("input.mp4", "output_frames", "jpg");

// 2. 视频转图片（调整为 1064x1064）
FFMPEG::VideotoImage("input.mp4", "output_frames_1064x1064", "jpg", 1064, 1064);

// 3. 图片转视频
FFMPEG::ImagetoVideo("input_frames", "jpg", "output.mp4", 30, 30, 2000);

// 4. 图片缩放
FFMPEG::ScaleImage("input.jpg", "output_800x600.jpg", 800, 600);
```

#### 9.3.2 路径设置

FFmpeg 可执行文件的路径可通过以下函数设置：

```cpp
// 设置 ffmpeg 可执行文件的路径
FFMPEG::SetFfmpegPath("path/to/ffmpeg/bin/");

// 获取当前 ffmpeg 路径
const char* path = FFMPEG::GetFfmpegPath();
```

默认路径为 `ffmpeg\bin\`，与程序运行目录相对。

### 9.4 完成的工作

1. **函数封装**：将 FFmpeg 命令行操作封装为简单易用的 C++ 函数接口

2. **功能扩展**：
   - 为 `VideotoImage` 函数添加了分辨率调整功能
   - 保持了原有函数的向后兼容性（默认参数）

3. **路径灵活性**：
   - 支持相对路径和绝对路径
   - 自动创建输出目录
   - 可通过 `SetFfmpegPath` 函数自定义 FFmpeg 可执行文件的路径

4. **测试验证**：
   - 验证了视频转图片功能（支持分辨率调整）
   - 验证了图片转视频功能
   - 验证了图片缩放功能
   - 确保与学长的 FFmpeg 可执行文件完全兼容

5. **代码质量**：
   - 代码结构清晰，易于理解和维护
   - 提供了详细的函数文档和参数说明
   - 错误处理机制完善

### 9.5 测试建议

在使用 FFmpeg 功能模块时，建议：

1. **运行目录**：在 `Visual-Net/bin` 目录下执行程序，确保 FFmpeg 路径正确

2. **路径设置**：
   - 使用相对路径时，相对于程序运行目录
   - 使用绝对路径时，确保路径格式正确（Windows 下使用反斜杠 `\`）

3. **性能考虑**：
   - 调整分辨率会增加处理时间
   - 处理大量图片或高分辨率视频时，建议适当调整参数以平衡质量和速度

4. **错误处理**：
   - 检查函数返回值以判断操作是否成功
   - 若失败，可查看控制台输出的 FFmpeg 错误信息
