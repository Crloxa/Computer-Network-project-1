[CmdletBinding()]
param(
    [string]$InputPath,
    [string]$OutputRoot,
    [int]$TimeLimitMs = 30000,
    [int[]]$FpsValues = @(15, 10)
)

$ErrorActionPreference = "Stop"

function Assert-FileExists {
    param(
        [string]$Path,
        [string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Label not found: $Path"
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir
$visualNetBin = Join-Path $repoRoot "Visual-Net/bin"
$encoderExe = Join-Path $visualNetBin "encoder.exe"
$decoderExe = Join-Path $visualNetBin "decoder.exe"
$ffmpegExe = Join-Path $visualNetBin "ffmpeg/bin/ffmpeg.exe"

$isWindowsHost = ($env:OS -eq "Windows_NT")
if (-not $isWindowsHost) {
    throw "This script must be run on Windows because Visual-Net/bin contains Windows executables."
}

Assert-FileExists -Path $encoderExe -Label "encoder.exe"
Assert-FileExists -Path $decoderExe -Label "decoder.exe"
Assert-FileExists -Path $ffmpegExe -Label "ffmpeg.exe"

if (-not $InputPath) {
    $InputPath = Join-Path $repoRoot "Visual-Net/miku.jpg"
}

$resolvedInput = (Resolve-Path -LiteralPath $InputPath).Path
if (-not $OutputRoot) {
    $OutputRoot = Join-Path $repoRoot "artifacts/visual-net-roundtrip"
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$runRoot = Join-Path $OutputRoot $timestamp
New-Item -ItemType Directory -Force -Path $runRoot | Out-Null

$results = @()
$originalLocation = Get-Location

try {
    Set-Location $visualNetBin

    foreach ($fps in $FpsValues) {
        $caseDir = Join-Path $runRoot ("fps{0}" -f $fps)
        New-Item -ItemType Directory -Force -Path $caseDir | Out-Null

        $videoPath = Join-Path $caseDir "encoded.mp4"
        $restoredPath = Join-Path $caseDir ("restored" + [IO.Path]::GetExtension($resolvedInput))
        $encoderLog = Join-Path $caseDir "encoder.log"
        $decoderLog = Join-Path $caseDir "decoder.log"

        if (Test-Path -LiteralPath $videoPath) {
            Remove-Item -LiteralPath $videoPath -Force
        }
        if (Test-Path -LiteralPath $restoredPath) {
            Remove-Item -LiteralPath $restoredPath -Force
        }

        Write-Host "==> fps=$fps"
        & .\encoder.exe $resolvedInput $videoPath $TimeLimitMs $fps 2>&1 | Tee-Object -FilePath $encoderLog
        $encoderExit = $LASTEXITCODE

        $videoExists = Test-Path -LiteralPath $videoPath
        $videoSize = if ($videoExists) { (Get-Item -LiteralPath $videoPath).Length } else { 0 }

        if ($videoExists -and $videoSize -gt 0) {
            & .\decoder.exe $videoPath $restoredPath 2>&1 | Tee-Object -FilePath $decoderLog
            $decoderExit = $LASTEXITCODE
        }
        else {
            "decoder skipped because encoded video was not generated" | Set-Content -LiteralPath $decoderLog
            $decoderExit = -1
        }

        $restoredExists = Test-Path -LiteralPath $restoredPath
        $match = $false
        if ($restoredExists) {
            $inputHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedInput).Hash
            $outputHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $restoredPath).Hash
            $match = ($inputHash -eq $outputHash)
        }

        $result = [pscustomobject]@{
            fps = $fps
            encoder_exit = $encoderExit
            decoder_exit = $decoderExit
            video_exists = $videoExists
            video_size = $videoSize
            restored_exists = $restoredExists
            byte_match = $match
            encoder_log = $encoderLog
            decoder_log = $decoderLog
            video_path = $videoPath
            restored_path = $restoredPath
        }

        $results += $result
        $result | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $caseDir "result.json")
    }
}
finally {
    Set-Location $originalLocation
}

$results | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $runRoot "summary.json")
$results | Format-Table -AutoSize

if (-not ($results | Where-Object { $_.byte_match })) {
    exit 1
}
