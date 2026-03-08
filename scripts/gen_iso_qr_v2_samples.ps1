$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

$grid = 25
$quiet = 4
$module = 24
$size = ($grid + 2 * $quiet) * $module

$symbolPath = 'bin/samples/sample_iso_qr_v2_symbol.png'
$layoutPath = 'bin/samples/sample_iso_qr_v2_layout.png'

$matrix = New-Object 'bool[,]' $grid, $grid

function Set-Cell([int]$x, [int]$y, [bool]$v) {
    if ($x -ge 0 -and $x -lt $grid -and $y -ge 0 -and $y -lt $grid) {
        $script:matrix[$x, $y] = $v
    }
}

function Draw-Finder([int]$ox, [int]$oy) {
    for ($dy = 0; $dy -lt 7; $dy++) {
        for ($dx = 0; $dx -lt 7; $dx++) {
            $d = [Math]::Max([Math]::Abs($dx - 3), [Math]::Abs($dy - 3))
            $black = ($d -eq 3) -or ($d -le 1)
            Set-Cell ($ox + $dx) ($oy + $dy) $black
        }
    }
}

function Draw-Alignment([int]$cx, [int]$cy) {
    for ($dy = -2; $dy -le 2; $dy++) {
        for ($dx = -2; $dx -le 2; $dx++) {
            $d = [Math]::Max([Math]::Abs($dx), [Math]::Abs($dy))
            $black = ($d -eq 2) -or ($d -eq 0)
            Set-Cell ($cx + $dx) ($cy + $dy) $black
        }
    }
}

function Is-FormatCell([int]$x, [int]$y) {
    if ($y -eq 8 -and $x -ge 0 -and $x -le 8 -and $x -ne 6) { return $true }
    if ($x -eq 8 -and $y -ge 0 -and $y -le 8 -and $y -ne 6) { return $true }
    if ($y -eq 8 -and $x -ge 17 -and $x -le 24) { return $true }
    if ($x -eq 8 -and $y -ge 17 -and $y -le 24) { return $true }
    return $false
}

function Is-Reserved([int]$x, [int]$y) {
    $finderSep = (($x -le 7 -and $y -le 7) -or ($x -ge 17 -and $y -le 7) -or ($x -le 7 -and $y -ge 17))
    $timing = (($y -eq 6 -and $x -ge 8 -and $x -le 16) -or ($x -eq 6 -and $y -ge 8 -and $y -le 16))
    $alignment = ($x -ge 16 -and $x -le 20 -and $y -ge 16 -and $y -le 20)
    $darkModule = ($x -eq 8 -and $y -eq 17)
    return ($finderSep -or $timing -or $alignment -or $darkModule -or (Is-FormatCell $x $y))
}

function Cell-ToPixel([int]$cell) {
    return 20 + ($quiet + $cell) * $module
}

# Function patterns
Draw-Finder 0 0
Draw-Finder 18 0
Draw-Finder 0 18
Draw-Alignment 18 18

for ($x = 8; $x -le 16; $x++) {
    Set-Cell $x 6 ((($x - 8) % 2) -eq 0)
}
for ($y = 8; $y -le 16; $y++) {
    Set-Cell 6 $y ((($y - 8) % 2) -eq 0)
}
Set-Cell 8 17 $true

# Draw a deterministic format-bit preview (for visualizing reserved format zones)
$formatCoords = New-Object 'System.Collections.Generic.List[object]'
for ($x = 0; $x -le 8; $x++) { if ($x -ne 6) { $formatCoords.Add(@($x, 8)) } }
for ($y = 0; $y -le 8; $y++) { if ($y -ne 6) { $formatCoords.Add(@(8, $y)) } }
for ($x = 17; $x -le 24; $x++) { $formatCoords.Add(@($x, 8)) }
for ($y = 17; $y -le 24; $y++) { $formatCoords.Add(@(8, $y)) }
for ($i = 0; $i -lt $formatCoords.Count; $i++) {
    $pair = $formatCoords[$i]
    Set-Cell $pair[0] $pair[1] (($i % 2) -eq 0)
}

# Fill data modules with checker pattern to show data-bearing area.
for ($y = 0; $y -lt $grid; $y++) {
    for ($x = 0; $x -lt $grid; $x++) {
        if (-not (Is-Reserved $x $y)) {
            Set-Cell $x $y ((($x + $y) % 2) -eq 0)
        }
    }
}

# Render plain symbol
$symbol = New-Object System.Drawing.Bitmap $size, $size
$gs = [System.Drawing.Graphics]::FromImage($symbol)
$gs.Clear([System.Drawing.Color]::White)
$blackBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::Black)
for ($y = 0; $y -lt $grid; $y++) {
    for ($x = 0; $x -lt $grid; $x++) {
        if ($matrix[$x, $y]) {
            $px = ($quiet + $x) * $module
            $py = ($quiet + $y) * $module
            $gs.FillRectangle($blackBrush, $px, $py, $module, $module)
        }
    }
}
$symbol.Save($symbolPath, [System.Drawing.Imaging.ImageFormat]::Png)
$gs.Dispose()

# Build annotated layout panel
$canvas = New-Object System.Drawing.Bitmap 1820, 960
$gc = [System.Drawing.Graphics]::FromImage($canvas)
$gc.Clear([System.Drawing.Color]::FromArgb(246, 246, 246))
$gc.DrawImage($symbol, 20, 20, $size, $size)

$titleFont = New-Object System.Drawing.Font('Microsoft YaHei', 18, [System.Drawing.FontStyle]::Bold)
$textFont = New-Object System.Drawing.Font('Microsoft YaHei', 12, [System.Drawing.FontStyle]::Regular)
$miniFont = New-Object System.Drawing.Font('Microsoft YaHei', 10, [System.Drawing.FontStyle]::Regular)

$penQuiet = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(80, 80, 180), 3)
$penFinder = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(180, 30, 60), 3)
$penTiming = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 140, 0), 3)
$penAlign = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(0, 150, 110), 3)
$penFormat = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(120, 80, 180), 3)

# Borders for quiet zone and logical grid
$gc.DrawRectangle($penQuiet, 20, 20, $size - 1, $size - 1)
$gridOffset = 20 + $quiet * $module
$gc.DrawRectangle($penQuiet, $gridOffset, $gridOffset, $grid * $module, $grid * $module)

# Finder boxes
$gc.DrawRectangle($penFinder, (Cell-ToPixel 0), (Cell-ToPixel 0), 7 * $module, 7 * $module)
$gc.DrawRectangle($penFinder, (Cell-ToPixel 18), (Cell-ToPixel 0), 7 * $module, 7 * $module)
$gc.DrawRectangle($penFinder, (Cell-ToPixel 0), (Cell-ToPixel 18), 7 * $module, 7 * $module)

# Timing lines
$gc.DrawLine($penTiming,
    (Cell-ToPixel 8),
    (Cell-ToPixel 6) + [int]($module / 2),
    (Cell-ToPixel 16) + $module,
    (Cell-ToPixel 6) + [int]($module / 2))
$gc.DrawLine($penTiming,
    (Cell-ToPixel 6) + [int]($module / 2),
    (Cell-ToPixel 8),
    (Cell-ToPixel 6) + [int]($module / 2),
    (Cell-ToPixel 16) + $module)

# Alignment box (5x5 centered at 18,18 => x=16..20, y=16..20)
$gc.DrawRectangle($penAlign, (Cell-ToPixel 16), (Cell-ToPixel 16), 5 * $module, 5 * $module)

# Format-information strips (major visible strips)
$gc.DrawRectangle($penFormat, (Cell-ToPixel 0), (Cell-ToPixel 8), 9 * $module, 1 * $module)
$gc.DrawRectangle($penFormat, (Cell-ToPixel 8), (Cell-ToPixel 0), 1 * $module, 9 * $module)
$gc.DrawRectangle($penFormat, (Cell-ToPixel 17), (Cell-ToPixel 8), 8 * $module, 1 * $module)
$gc.DrawRectangle($penFormat, (Cell-ToPixel 8), (Cell-ToPixel 17), 1 * $module, 8 * $module)

$gc.DrawString('ISO 标准二维码 V2（25x25）结构标注图', $titleFont, [System.Drawing.Brushes]::Black, 860, 30)
$gc.DrawString('各区域作用与规格', $textFont, [System.Drawing.Brushes]::Black, 860, 75)

$y0 = 120
$step = 34
$gc.DrawString('1) 安全边界（Quiet Zone）：四边各 4 modules（ISO 强制）。', $textFont, [System.Drawing.Brushes]::Black, 860, $y0)
$gc.DrawString('2) Finder 定位块：3 个，单个 7x7，位置左上/右上/左下。', $textFont, [System.Drawing.Brushes]::Black, 860, $y0 + $step)
$gc.DrawString('3) 分隔带（Separator）：每个 Finder 外围 1 module 白环。', $textFont, [System.Drawing.Brushes]::Black, 860, $y0 + 2 * $step)
$gc.DrawString('4) Timing 时序线：y=6(x=8..16)，x=6(y=8..16)。', $textFont, [System.Drawing.Brushes]::Black, 860, $y0 + 3 * $step)
$gc.DrawString('5) Alignment 对齐块：5x5，中心(18,18)，区域 x=16..20,y=16..20。', $textFont, [System.Drawing.Brushes]::Black, 860, $y0 + 4 * $step)
$gc.DrawString('6) Format 信息区：靠近 Finder 的标准保留条带。', $textFont, [System.Drawing.Brushes]::Black, 860, $y0 + 5 * $step)
$gc.DrawString('7) 数据模块：承载 信息头 + 负载数据 + CRC32。', $textFont, [System.Drawing.Brushes]::Black, 860, $y0 + 6 * $step)
$gc.DrawString('8) 信息头规格：8 bytes（协议号/版本/帧序号/总帧数/长度）。', $textFont, [System.Drawing.Brushes]::Black, 860, $y0 + 7 * $step)
$gc.DrawString('9) CRC32 规格：4 bytes，大端序，覆盖 header+payload。', $textFont, [System.Drawing.Brushes]::Black, 860, $y0 + 8 * $step)
$gc.DrawString('10) 纠错机制：ISO Reed-Solomon（ECC L/M/Q/H）。', $textFont, [System.Drawing.Brushes]::Black, 860, $y0 + 9 * $step)

$gc.DrawString('说明：header/payload/CRC32 在图中不是固定矩形区域，', $miniFont, [System.Drawing.Brushes]::DimGray, 860, 530)
$gc.DrawString('而是按 ISO 二维码的数据放置与掩码规则映射到数据模块。', $miniFont, [System.Drawing.Brushes]::DimGray, 860, 550)

$gc.DrawString('颜色图例：安全边界=蓝，Finder=红，Timing=橙，Alignment=绿，Format=紫',
    $miniFont, [System.Drawing.Brushes]::DimGray, 860, 585)

$gc.DrawImage($symbol, 1120, 620, 300, 300)
$gc.DrawString('示例符号预览', $miniFont, [System.Drawing.Brushes]::Black, 1120, 930)

$canvas.Save($layoutPath, [System.Drawing.Imaging.ImageFormat]::Png)

$gc.Dispose()
$canvas.Dispose()
$symbol.Dispose()

Write-Host "Generated: $symbolPath"
Write-Host "Generated: $layoutPath"

