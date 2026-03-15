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
