# Visual-Net ffmpeg 回环测试

这个测试固定以 `Visual-Net/bin` 为执行目录，复用现成的 `encoder.exe`、`decoder.exe` 和 `ffmpeg/bin/ffmpeg.exe`，验证：

`输入文件 -> 编码图片序列 -> ffmpeg 合成视频 -> ffmpeg 拆帧 -> decode 还原文件 -> 输入输出字节一致`

## 环境要求

- Windows
- Windows PowerShell 5.1+ 或 PowerShell 7+
- 仓库内 `Visual-Net/bin` 目录完整存在

当前仓库附带的是 Windows `PE32+` 可执行文件，macOS / Linux 机器不能直接运行，除非额外提供兼容层。

## 一键执行

在仓库根目录运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_visual_net_roundtrip.ps1
```

默认输入样本是 `Visual-Net/miku.jpg`，默认测试两组帧率：

- `15fps`
- `10fps`

也可以显式指定输入和参数：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_visual_net_roundtrip.ps1 `
  -InputPath .\Visual-Net\jnsj.jpg `
  -TimeLimitMs 30000 `
  -FpsValues 15,20
```

## 产物与判定

脚本会在 `artifacts/visual-net-roundtrip/<timestamp>/` 下为每组帧率生成：

- `encoded.mp4`
- `restored.*`
- `encoder.log`
- `decoder.log`
- `result.json`

并在运行根目录生成 `summary.json`。

成功判定：

- `encoded.mp4` 存在且大小大于 0
- `decoder.exe` 成功输出还原文件
- 还原文件与输入文件的 `SHA256` 一致

失败定位优先顺序：

- `encoded.mp4` 未生成：先检查执行目录和 `ffmpeg/bin/ffmpeg.exe` 相对路径
- 视频已生成但未还原：检查 `decoder.log` 中是否出现 `failed to open the video` 或 `skipped frame`
- 还原文件存在但哈希不一致：视为 encode/decode 与生成视频的兼容性问题
