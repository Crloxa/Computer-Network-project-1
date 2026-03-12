# Windows 运行与排障速查

## 1. 先记住四件事

- 当前默认主线是 `V1.6-108-4F`，不是旧 ISO QR v2。
- 在 Windows PowerShell 里不要直接敲 `Project1 ...`，而是用 `.\x64\Debug\Project1.exe ...`、`.\x64\Release\Project1.exe ...`，或者 `.\scripts\run_project1.ps1 ...`。
- 必须从仓库根目录运行，或者让脚本帮你切回仓库根目录。
- `ffmpeg.exe` 默认放在 `ffmpeg\bin\ffmpeg.exe`。

## 2. 最短可用路径

### 2.1 构建

1. 打开 `Project1.sln`
2. 选择 `x64`
3. 选择 `Debug` 或 `Release`
4. 执行 Build

不要选 `Win32`。

### 2.2 验证你跑的是新二进制

在仓库根目录执行：

```powershell
.\x64\Debug\Project1.exe --version
```

期望看到：

```text
Project1 protocol=V1.6-108-4F
```

如果你看到旧 ISO、OpenCV 或别的版本信息，说明你跑的不是当前仓库默认主线。

## 3. 推荐运行方式

### 3.1 直接运行可执行文件

```powershell
.\x64\Debug\Project1.exe samples out\samples
.\x64\Debug\Project1.exe encode input.jpg out\encode\input
.\x64\Debug\Project1.exe decode out\encode\input\frames\physical out\decode\input
```

### 3.2 使用仓库脚本

```powershell
.\scripts\run_project1.ps1 encode input.jpg out\encode\input
.\scripts\run_project1.ps1 decode out\encode\input\frames\physical out\decode\input
.\scripts\run_project1.ps1 -Config Release samples out\samples_release
```

脚本会：

- 自动回到仓库根目录
- 定位 `x64\Debug\Project1.exe` 或 `x64\Release\Project1.exe`
- 打印实际使用的 exe 路径
- 检查仓库内的 `ffmpeg\bin\ffmpeg.exe`

## 4. 关于旧 ISO 参数

组里以前常用的命令例如：

```powershell
.\x64\Debug\Project1.exe encode input.jpg out\encode\input --profile iso133 --ecc Q --canvas 1440
```

当前版本会接受这些参数，但只把它们当兼容层输入：

- `--profile`
- `--ecc`
- `--canvas`
- `--markers`
- `--protocol-samples`
- `--decode-debug`

程序会明确警告“这些历史 ISO 参数已被忽略”，实际仍按 `V1.6-108-4F` 执行。

## 5. ffmpeg 依赖口径

### 5.1 不装 ffmpeg 也能跑的场景

- `samples` 生成 BMP 与 manifest
- `encode` 生成 BMP 帧目录与 manifest
- `decode` 读取 BMP 帧目录

### 5.2 需要 ffmpeg 的场景

- 生成 `demo.mp4`
- 读取视频输入做 `decode`
- 读取 PNG/JPG/JPEG 输入做 `decode`
- 生成 `png_mirror/`（当前会尽量生成，缺依赖时会跳过）

仓库内推荐路径：

```text
ffmpeg\bin\ffmpeg.exe
```

## 6. 常见报错对照

### 6.1 `Project1 : 无法将“Project1”项识别为 cmdlet`

原因：你在 PowerShell 里直接敲了 `Project1`，但 exe 不在 `PATH`。

处理：

```powershell
.\x64\Debug\Project1.exe --version
```

或者：

```powershell
.\scripts\run_project1.ps1 --version
```

### 6.2 `OpenCV ... QRCodeEncoderImpl::generateQR`

原因：你运行的不是当前仓库默认的 `V1.6-108-4F` 二进制，而是旧 ISO/OpenCV 构建产物或旧工作区。

处理：

1. 重新拉到当前仓库主线
2. 在 Visual Studio 里重新构建 `x64|Debug`
3. 先跑 `.\x64\Debug\Project1.exe --version`
4. 确认输出 `Project1 protocol=V1.6-108-4F`

### 6.3 `ffmpeg executable was not found`

原因：仓库里没有找到 `ffmpeg\bin\ffmpeg.exe`，并且系统 `PATH` 里也没有 `ffmpeg`。

处理：

1. 把 `ffmpeg.exe` 放到 `ffmpeg\bin\ffmpeg.exe`
2. 或者先只跑 BMP 路径：
   - `samples`
   - `encode`
   - `decode` 对 BMP 帧目录

### 6.4 `Unknown option`

原因：要么参数确实不存在，要么你跑到的是更旧的二进制。

先做两步：

```powershell
.\x64\Debug\Project1.exe --version
.\x64\Debug\Project1.exe --help
```

如果 `--version` 不是 `V1.6-108-4F`，先重建再说。
