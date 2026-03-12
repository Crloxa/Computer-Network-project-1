param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ProjectArgs
)

if (-not $ProjectArgs -or $ProjectArgs.Count -eq 0) {
    Write-Host "用法:"
    Write-Host "  .\scripts\run_project1.ps1 encode input.jpg out\encode\input"
    Write-Host "  .\scripts\run_project1.ps1 -Config Release samples out\samples"
    Write-Host "  .\scripts\run_project1.ps1 encode input.jpg out\encode\input --profile iso133 --ecc Q --canvas 1440"
    exit 1
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$exePath = Join-Path $repoRoot ("x64\" + $Config + "\Project1.exe")
$bundledFfmpeg = Join-Path $repoRoot "ffmpeg\bin\ffmpeg.exe"

if (-not (Test-Path $exePath)) {
    Write-Error ("未找到可执行文件: " + $exePath + "`n请先在 Visual Studio 中使用 " + $Config + "|x64 构建 Project1.sln。")
    exit 1
}

Write-Host ("[run] repo=" + $repoRoot)
Write-Host ("[run] exe=" + $exePath)
if (Test-Path $bundledFfmpeg) {
    Write-Host ("[run] ffmpeg=" + $bundledFfmpeg)
} else {
    Write-Warning "未在仓库内找到 ffmpeg\bin\ffmpeg.exe。BMP 帧流程仍可运行，但 demo.mp4、PNG/JPG 解码等能力会受限。"
}

Push-Location $repoRoot
try {
    & $exePath @ProjectArgs
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
