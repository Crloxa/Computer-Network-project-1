#!/usr/bin/env python3

import binascii
import shutil
import struct
from pathlib import Path

import qrcode
from PIL import Image
from qrcode import constants
from qrcode.exceptions import DataOverflowError
from qrcode.util import MODE_8BIT_BYTE, QRData


ROOT = Path(__file__).resolve().parents[1]
SAMPLES_DIR = ROOT / "bin" / "samples"
PROFILE = "iso133"
ECC_NAME = "Q"
CANVAS_PIXELS = 1440
VERSION = 29
LOGICAL_GRID = 133
HEADER_BYTES = 8
CRC_BYTES = 4
FRAME_OVERHEAD_BYTES = HEADER_BYTES + CRC_BYTES
PROTOCOL_ID = 0xA2
PROTOCOL_VERSION = 0x01


def compute_crc32(data: bytes) -> int:
    return binascii.crc32(data) & 0xFFFFFFFF


def pack_frame(frame_seq: int, total_frames: int, payload: bytes) -> bytes:
    header = struct.pack(">BBHHH", PROTOCOL_ID, PROTOCOL_VERSION, frame_seq, total_frames, len(payload))
    body = header + payload
    return body + struct.pack(">I", compute_crc32(body))


def render_qr(frame_bytes: bytes) -> Image.Image:
    qr = qrcode.QRCode(
        version=VERSION,
        error_correction=constants.ERROR_CORRECT_Q,
        box_size=1,
        border=4,
    )
    qr.add_data(QRData(frame_bytes, mode=MODE_8BIT_BYTE))
    qr.make(fit=False)
    return qr.make_image(fill_color="black", back_color="white").convert("RGB")


def render_marker(size_pixels: int, invert_center: bool) -> Image.Image:
    marker = Image.new("RGB", (7, 7), "white")
    pixels = marker.load()
    for y in range(7):
        for x in range(7):
            distance = max(abs(x - 3), abs(y - 3))
            black = distance == 3 or distance <= 1
            if invert_center and 2 <= x <= 4 and 2 <= y <= 4:
                black = not black
            pixels[x, y] = (0, 0, 0) if black else (255, 255, 255)
    return marker.resize((size_pixels, size_pixels), Image.Resampling.NEAREST)


def build_carrier_layout() -> dict:
    marker_margin = max(24, CANVAS_PIXELS // 28)
    marker_size = max(72, CANVAS_PIXELS // 9)
    qr_size = max(720, int(CANVAS_PIXELS * 0.78))
    qr_x = (CANVAS_PIXELS - qr_size) // 2
    return {
        "marker_margin": marker_margin,
        "marker_size": marker_size,
        "qr_size": qr_size,
        "qr_x": qr_x,
        "qr_y": qr_x,
    }


def render_carrier(frame_bytes: bytes) -> Image.Image:
    layout = build_carrier_layout()
    carrier = Image.new("RGB", (CANVAS_PIXELS, CANVAS_PIXELS), "white")
    qr = render_qr(frame_bytes).resize((layout["qr_size"], layout["qr_size"]), Image.Resampling.NEAREST)
    carrier.paste(qr, (layout["qr_x"], layout["qr_y"]))

    marker = render_marker(layout["marker_size"], False)
    marker_br = render_marker(layout["marker_size"], True)
    margin = layout["marker_margin"]
    size = layout["marker_size"]
    carrier.paste(marker, (margin, margin))
    carrier.paste(marker, (CANVAS_PIXELS - margin - size, margin))
    carrier.paste(marker, (margin, CANVAS_PIXELS - margin - size))
    carrier.paste(marker_br, (CANVAS_PIXELS - margin - size, CANVAS_PIXELS - margin - size))
    return carrier


def can_encode(frame_bytes: bytes) -> bool:
    try:
        render_qr(frame_bytes)
        return True
    except DataOverflowError:
        return False


def determine_max_frame_bytes() -> int:
    low = FRAME_OVERHEAD_BYTES
    high = 4096
    best = 0
    while low <= high:
        candidate = low + (high - low) // 2
        test_bytes = bytes((index * 73 + 19) & 0xFF for index in range(candidate))
        if can_encode(test_bytes):
            best = candidate
            low = candidate + 1
        else:
            high = candidate - 1
    if best < FRAME_OVERHEAD_BYTES:
        raise RuntimeError("Unable to encode the ISO transport header.")
    return best


def deterministic_input_bytes(payload_size: int) -> bytes:
    target_length = payload_size * 3 - 17
    return bytes(((0x30 + index * 17) & 0xFF) for index in range(target_length))


def encode_input(input_bytes: bytes) -> list[tuple[int, bytes, Image.Image]]:
    max_payload_bytes = determine_max_frame_bytes() - FRAME_OVERHEAD_BYTES
    chunks = [
        input_bytes[offset:offset + max_payload_bytes]
        for offset in range(0, len(input_bytes), max_payload_bytes)
    ]
    frames = []
    for frame_seq, payload in enumerate(chunks):
        frame_bytes = pack_frame(frame_seq, len(chunks), payload)
        frames.append((frame_seq, frame_bytes, render_carrier(frame_bytes)))
    return frames


def write_manifest(path: Path, frame_infos: list[tuple[int, bytes]]) -> None:
    lines = ["file\tframe_seq\ttotal_frames\tpayload_len\tprofile\tecc\tcanvas_px\tframe_bytes"]
    for frame_seq, frame_bytes in frame_infos:
        _, _, _, total_frames, payload_len = struct.unpack(">BBHHH", frame_bytes[:HEADER_BYTES])
        lines.append(
            f"frame_{frame_seq:05d}.png\t{frame_seq}\t{total_frames}\t{payload_len}\t"
            f"{PROFILE}\t{ECC_NAME}\t{CANVAS_PIXELS}\t{len(frame_bytes)}"
        )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_case(case_dir: Path,
               input_bytes: bytes,
               frame_infos: list[tuple[int, bytes, Image.Image]],
               expected_status: str,
               missing_sequences: list[int]) -> None:
    if case_dir.exists():
        shutil.rmtree(case_dir)
    frames_dir = case_dir / "frames"
    frames_dir.mkdir(parents=True, exist_ok=True)

    (case_dir / "input.bin").write_bytes(input_bytes)
    (case_dir / "decode_args.txt").write_text("--profile iso133 --ecc Q --canvas 1440\n", encoding="utf-8")
    (case_dir / "expected_status.txt").write_text(expected_status + "\n", encoding="utf-8")
    if missing_sequences:
        (case_dir / "expected_missing_frames.txt").write_text(
            "\n".join(str(value) for value in missing_sequences) + "\n",
            encoding="utf-8",
        )

    manifest_rows = []
    for frame_seq, frame_bytes, image in frame_infos:
        file_name = f"frame_{frame_seq:05d}.png"
        image.save(frames_dir / file_name)
        manifest_rows.append((frame_seq, frame_bytes))
    write_manifest(case_dir / "frame_manifest.tsv", manifest_rows)


def main() -> None:
    input_bytes = deterministic_input_bytes(determine_max_frame_bytes() - FRAME_OVERHEAD_BYTES)
    success_frames = encode_input(input_bytes)
    if len(success_frames) < 3:
        raise RuntimeError("Fixture generation expected at least 3 frames.")

    success_dir = SAMPLES_DIR / "v2_success"
    missing_dir = SAMPLES_DIR / "v2_missing_frame"
    crc_dir = SAMPLES_DIR / "v2_crc_error"

    write_case(success_dir, input_bytes, success_frames, "success", [])

    missing_seq = success_frames[1][0]
    missing_frames = [frame for frame in success_frames if frame[0] != missing_seq]
    write_case(missing_dir, input_bytes, missing_frames, "missing_frames", [missing_seq])

    crc_frame_seq, crc_frame_bytes, _ = success_frames[1]
    corrupted = bytearray(crc_frame_bytes)
    corrupted[HEADER_BYTES] ^= 0x5A
    crc_frames = []
    for frame_seq, frame_bytes, image in success_frames:
        if frame_seq == crc_frame_seq:
            mutated_bytes = bytes(corrupted)
            crc_frames.append((frame_seq, mutated_bytes, render_carrier(mutated_bytes)))
        else:
            crc_frames.append((frame_seq, frame_bytes, image))
    write_case(crc_dir, input_bytes, crc_frames, "missing_frames", [crc_frame_seq])

    fixture_index = [
        "case\texpected_status\tprofile\tecc\tcanvas_px\tnotes",
        "v2_success\tsuccess\tiso133\tQ\t1440\tfull frame set; output.bin must match input.bin",
        f"v2_missing_frame\tmissing_frames\tiso133\tQ\t1440\tframe_{missing_seq:05d}.png removed from input set",
        f"v2_crc_error\tmissing_frames\tiso133\tQ\t1440\tframe_{crc_frame_seq:05d}.png carries payload/CRC mismatch",
    ]
    (SAMPLES_DIR / "v2_fixture_index.tsv").write_text("\n".join(fixture_index) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
