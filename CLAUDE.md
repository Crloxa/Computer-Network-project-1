# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a **data-to-video encoding/decoding system** for a Computer Networks course project. Arbitrary binary data is encoded into a sequence of custom QR-code-like image frames (protocol `V1.6-133-4F`) assembled into an MP4 video, which can then be decoded back to the original binary. The current branch implements **4-color (4色) encoding** — each cell in the 133×133 logical grid uses one of 4 colors (Blue/Green/Red/White) to carry 2 bits, yielding 3756 bytes/frame.

## Build & Run

### macOS (CMake)
```bash
brew install opencv ffmpeg   # prerequisites
cmake -S . -B build
cmake --build build -j
```
Produces `bin/encoder` and `bin/decoder`.

### Windows (Visual Studio)
Open `Project1.sln` in VS 2022+, build `x64|Debug` or `x64|Release`. The `.vcxproj` links against pre-built `lib/opencv_world4120[d].lib` and bundles `ffmpeg\bin\ffmpeg.exe`.

### CLI Usage
```bash
# Encoder
./bin/encoder <input.bin> <output.mp4> <timeLimitMs> [fps]
# e.g.: ./bin/encoder e1.bin e1.mp4 10000

# Decoder
./bin/decoder <input.mp4> <output.bin> [vout.bin]
# e.g.: ./bin/decoder e1.mp4 1.bin v1.bin
```

The encoder and decoder are the **same source files** compiled separately via `-DBUILD_ENCODER=1` / `-DBUILD_DECODER=1` (see `CMakeLists.txt`). The `main()` in `src/main.cpp` uses `#if defined(BUILD_ENCODER)` to select the pipeline.

## Testing with the Official Checker

Full end-to-end self-test loop (from `requirements.md`):
```bash
gcc checker.c -o checker          # compile the course checker
./checker msg 1 102400            # generate e1.bin (100KB test file)
./bin/encoder e1.bin e1.mp4 10000 # encode
./bin/decoder e1.mp4 1.bin v1.bin # decode (1.bin = output, v1.bin = validity)
./checker bench 1                 # score: check Lost% and Val.b
```

**Critical**: `vout.bin` uses **bit-level** validity — output `0xFF` per valid byte (all 8 bits valid) and `0x00` per lost byte. Outputting `0x01` causes the checker to treat 7 of 8 bits as lost.

All runtime output goes to `out/`; do not commit bulk frame images or demo videos.

## Architecture

### Encoding Pipeline (`FileToVideo` in `src/main.cpp:21`)
```
Binary file
  → Code::Main()         (src/code.cpp)   — splits data into 133×133 frame Mats
      → CodeFrame()                        — builds safe area, QR finder patterns, header, 4-color data cells
      → ScaleToDisSize()                   — upscales 133→1330 (10×)
      → imwrite() PNG frames
  → FFMPEG::ImagetoVideo() (src/ffmpeg.cpp) — assembles PNGs into MP4 via ffmpeg CLI
```

### Decoding Pipeline (`VideoToFile` in `src/main.cpp:53`)
```
MP4 video
  → FFMPEG::VideotoImage() (src/ffmpeg.cpp) — extracts JPG frames via ffmpeg CLI
  → ImgParse::Main()        (src/pic.cpp)   — perspective-corrects each frame to 133×133
  → ImageDecode::Main()     (src/ImgDecode.cpp) — reads header + 4-color cells, verifies checksum
  → Reassemble bytes, write output.bin + vout.bin
```

### Key Source Files

| File | Namespace | Role |
|---|---|---|
| `src/main.cpp` | — | Entry point; `FileToVideo` / `VideoToFile` pipelines |
| `src/code.cpp/.h` | `Code` | Frame encoding: grid layout, finder patterns, 4-color cell writing |
| `src/pic.cpp/.h` | `ImgParse` | Image parsing: multi-strategy perspective correction |
| `src/ImgDecode.cpp/.h` | `ImageDecode` | Frame decoding: header + payload reading, checksum verification |
| `src/ffmpeg.cpp/.h` | `FFMPEG` | FFmpeg CLI wrapper for video encoding/decoding |

### Image Parsing Strategies (`src/pic.cpp`)
`ImgParse::Main()` tries strategies in order, falling back if each fails:
1. **Direct downscale** — if input is near-square (no distortion)
2. **V5 outer contour** — for first 3 frames (camera startup artifacts)
3. **V15 nested contour** — primary strategy; detects 3-level nested QR finder contours
4. **Temporal fallback** — reuses the last valid perspective transform matrix (`lastValidTransform`)

## Frame Protocol (V1.6-133-4F)

| Constant | Value | Meaning |
|---|---|---|
| `FrameSize` | 133 | Logical grid dimension |
| `FrameOutputRate` | 10 | Upscale factor → 1330×1330 output |
| `BytesPerFrame` | 3756 | Data payload per frame |
| `BitsPerCell` | 2 | 4-color encoding: 2 bits/cell |
| `SafeAreaWidth` | 2 | White border thickness |
| `QrPointSize` | 21 | Large finder pattern size (3 corners) |
| `SmallQrPointRadius` | 3 | Small alignment point (bottom-right) |
| `HeaderHeight` / `HeaderWidth` | 3 / 16 | Header: 3 rows of 16-bit fields |

**Frame layout**: 2px safe border → three 21×21 QR finder patterns (TL, TR, BL corners) → one small alignment point (BR) → 3×16 header at (row=3, col=21) → data cells fill remaining space across 5 rectangular regions.

**Header rows** (black/white encoding, more resilient):
- Row 0: frame type flags (Start/End/Normal/StartAndEnd) + tail length
- Row 1: 16-bit XOR checksum
- Row 2: 16-bit frame sequence number

**4-color palette** (BGR): Blue=0, Green=1, Red=2, White=3. Each byte splits into 4 cells (2 bits each, low-order first).

**Decoding color classification** (`ImgDecode.cpp:readCellValue`): if B+G+R ≥ 500 → White (3); else the dominant channel (B/G/R) determines value (0/1/2).

## Platform Notes

- **FFmpeg path**: `ffmpeg\bin\ffmpeg.exe` (Windows bundled) vs `ffmpeg` in PATH (macOS/Linux). Set in `src/ffmpeg.cpp`.
- **OpenCV**: Pre-built DLLs/libs in `lib/` for Windows; `find_package(OpenCV)` from Homebrew for macOS.
- **Git LFS**: Tracks `*.lib`, `*.dll`, `*.exe`, `*.mp4` — ensure Git LFS is installed before cloning.
