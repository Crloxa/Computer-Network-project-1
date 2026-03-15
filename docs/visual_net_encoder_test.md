# Visual-Net Encoder 独立测试程序

这个程序只测试 encoder 模块，不接 ffmpeg，不接 decode。

它会：

- 读取输入文件
- 调用 `Code::Main(...)` 生成编码图片序列
- 检查输出图片是否连续命名、是否能被 OpenCV 读出、尺寸是否为 `1330x1330`、通道数是否为 `3`
- 输出控制台摘要
- 在输出目录写出 `encoder_test_manifest.tsv`

## 工程文件

- 项目文件：`Visual-Net/encoder_test.vcxproj`
- 测试入口：`Visual-Net/src/encoder_test_main.cpp`

## 构建环境

- Windows
- Visual Studio 2022
- x64 配置

工程默认使用仓库内自带的依赖：

- 头文件：`Visual-Net/src/include`
- 库文件：`Visual-Net/src/lib`
- 运行时 DLL：`Visual-Net/bin/*.dll`

## 用法

```bat
encoder_test <input_file> <output_dir> [output_format] [frame_limit]
```

示例：

```bat
encoder_test sample.bin out_frames
encoder_test sample.bin out_frames jpg
encoder_test sample.bin out_frames png 10
```

## 输出

输出目录中会包含：

- `00000.png` 这类按五位数字命名的编码帧
- 可选的 `_layout` 预览图
- `encoder_test_manifest.tsv`

`encoder_test_manifest.tsv` 字段：

- `frame_index`
- `file_name`
- `width`
- `height`
- `channels`
- `file_size`
- `status`
- `detail`

## 返回码

- `0`：所有基础检查通过
- `1`：参数错误、文件错误、生成失败或基础检查失败
