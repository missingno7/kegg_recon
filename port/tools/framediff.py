#!/usr/bin/env python3
"""Compare a smoke.py indexed BMP against a DOSBox-X reference PNG.

    python port/tools/framediff.py --reference docs/port/reference-captures/main-menu.png \
        --port build/port/menu.bmp --diff build/port/menu-diff.png

Inputs are normalized to the game's 320x200 VGA raster with nearest-neighbor
sampling. Auto mode accepts raw 320x200 captures, 4:3 captures where VGA's 200
scan lines were stretched to 240 display lines, and port scanouts with 400
vertical lines. The whole image is sampled so aspect correction and doubled
scanout heights map back to all 200 game rows without cropping.
"""
from __future__ import annotations

import argparse
import struct
import sys
import zlib
from dataclasses import dataclass
from pathlib import Path

VGA_W, VGA_H = 320, 200


@dataclass
class Image:
    width: int
    height: int
    rgb: bytes


def _read_bmp(data: bytes, path: Path) -> Image:
    if len(data) < 54 or data[:2] != b"BM":
        raise ValueError(f"{path}: not a supported BMP")
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    dib_size = struct.unpack_from("<I", data, 14)[0]
    if dib_size < 40 or len(data) < 14 + dib_size:
        raise ValueError(f"{path}: unsupported BMP DIB header")
    width, signed_height, planes, depth, compression = struct.unpack_from("<iiHHI", data, 18)
    if width <= 0 or signed_height == 0 or planes != 1 or depth not in (8, 24, 32) or compression != 0:
        raise ValueError(f"{path}: expected uncompressed 8/24/32-bit BMP")
    height = abs(signed_height)
    row_bytes = ((width * depth + 31) // 32) * 4
    if pixel_offset + row_bytes * height > len(data):
        raise ValueError(f"{path}: truncated BMP pixels")
    palette: list[tuple[int, int, int]] = []
    if depth == 8:
        colors_used = struct.unpack_from("<I", data, 46)[0]
        count = colors_used or 256
        palette_start = 14 + dib_size
        if palette_start + count * 4 > pixel_offset:
            raise ValueError(f"{path}: invalid BMP palette")
        palette = [tuple(data[i + j] for j in (2, 1, 0)) for i in
                   range(palette_start, palette_start + count * 4, 4)]
    rgb = bytearray(width * height * 3)
    for y in range(height):
        sy = y if signed_height < 0 else height - 1 - y
        row = pixel_offset + sy * row_bytes
        for x in range(width):
            dst = (y * width + x) * 3
            if depth == 8:
                index = data[row + x]
                if index >= len(palette):
                    raise ValueError(f"{path}: palette index outside BMP palette")
                rgb[dst:dst + 3] = bytes(palette[index])
            else:
                src = row + x * (depth // 8)
                rgb[dst:dst + 3] = data[src + 2], data[src + 1], data[src]
    return Image(width, height, bytes(rgb))


def _paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    return a if pa <= pb and pa <= pc else b if pb <= pc else c


def _read_png(data: bytes, path: Path) -> Image:
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError(f"{path}: not PNG data")
    offset, chunks = 8, []
    width = height = depth = color = interlace = None
    palette: bytes | None = None
    transparency = b""
    while offset + 12 <= len(data):
        size = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        end = offset + 8 + size
        if end + 4 > len(data):
            raise ValueError(f"{path}: truncated PNG chunk")
        block = data[offset + 8:end]
        if kind == b"IHDR":
            width, height, depth, color, comp, filt, interlace = struct.unpack(">IIBBBBB", block)
            if comp != 0 or filt != 0:
                raise ValueError(f"{path}: unsupported PNG compression/filter method")
        elif kind == b"PLTE":
            palette = block
        elif kind == b"tRNS":
            transparency = block
        elif kind == b"IDAT":
            chunks.append(block)
        elif kind == b"IEND":
            break
        offset = end + 4
    if width is None or not width or not height or depth != 8 or interlace != 0:
        raise ValueError(f"{path}: expected noninterlaced 8-bit PNG")
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(color)
    if channels is None or (color == 3 and (palette is None or len(palette) % 3)):
        raise ValueError(f"{path}: unsupported PNG color type/palette")
    stride = width * channels
    raw = zlib.decompress(b"".join(chunks))
    if len(raw) != (stride + 1) * height:
        raise ValueError(f"{path}: PNG decompressed size mismatch")
    scan = bytearray(stride * height)
    pos = 0
    for y in range(height):
        filt = raw[pos]
        pos += 1
        row_start = y * stride
        for x in range(stride):
            value = raw[pos + x]
            left = scan[row_start + x - channels] if x >= channels else 0
            above = scan[row_start - stride + x] if y else 0
            upper_left = scan[row_start - stride + x - channels] if y and x >= channels else 0
            if filt == 1:
                value += left
            elif filt == 2:
                value += above
            elif filt == 3:
                value += (left + above) // 2
            elif filt == 4:
                value += _paeth(left, above, upper_left)
            elif filt != 0:
                raise ValueError(f"{path}: unsupported PNG row filter {filt}")
            scan[row_start + x] = value & 0xff
        pos += stride
    rgb = bytearray(width * height * 3)
    for i in range(width * height):
        src, dst = i * channels, i * 3
        if color == 0:
            rgb[dst:dst + 3] = bytes((scan[src],) * 3)
        elif color == 2:
            rgb[dst:dst + 3] = scan[src:src + 3]
        elif color == 3:
            index = scan[src]
            if index * 3 + 2 >= len(palette):
                raise ValueError(f"{path}: palette index outside PNG palette")
            rgb[dst:dst + 3] = palette[index * 3:index * 3 + 3]
        elif color == 4:
            rgb[dst:dst + 3] = bytes((scan[src],) * 3)
        else:
            rgb[dst:dst + 3] = scan[src:src + 3]
    return Image(width, height, bytes(rgb))


def read_image(path: Path) -> Image:
    data = path.read_bytes()
    if data.startswith(b"BM"):
        return _read_bmp(data, path)
    if data.startswith(b"\x89PNG"):
        return _read_png(data, path)
    raise ValueError(f"{path}: only BMP and PNG inputs are supported")


def normalize(image: Image, aspect: str, label: str) -> bytes:
    ratio = image.width / image.height
    raw_vga = abs(ratio - VGA_W / VGA_H) <= 0.015
    corrected_vga = abs(ratio - 4 / 3) <= 0.015
    doubled_vga = abs(ratio - VGA_W / 400) <= 0.01
    if aspect == "auto":
        if not raw_vga and not corrected_vga and not doubled_vga:
            raise ValueError(f"{label}: {image.width}x{image.height} is not a supported VGA capture ratio")
    elif aspect == "raw" and not raw_vga:
        raise ValueError(f"{label}: {image.width}x{image.height} is not raw 320x200 VGA")
    elif aspect == "4:3" and not corrected_vga:
        raise ValueError(f"{label}: {image.width}x{image.height} is not 4:3 VGA")
    rgb = image.rgb
    out = bytearray(VGA_W * VGA_H * 3)
    for y in range(VGA_H):
        sy = min(image.height - 1, int((y + 0.5) * image.height / VGA_H))
        for x in range(VGA_W):
            sx = min(image.width - 1, int((x + 0.5) * image.width / VGA_W))
            src = (sy * image.width + sx) * 3
            dst = (y * VGA_W + x) * 3
            out[dst:dst + 3] = rgb[src:src + 3]
    return bytes(out)


def write_png(path: Path, width: int, height: int, rgb: bytes) -> None:
    if len(rgb) != width * height * 3:
        raise ValueError("incorrect RGB byte count for PNG output")

    def chunk(kind: bytes, payload: bytes) -> bytes:
        body = kind + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body) & 0xffffffff)

    raw = b"".join(b"\0" + rgb[y * width * 3:(y + 1) * width * 3] for y in range(height))
    data = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def compare(reference: bytes, port: bytes, threshold: int) -> tuple[float, bytes, int]:
    diff = bytearray(len(reference))
    bad = 0
    changed = 0
    for i in range(0, len(reference), 3):
        dr = abs(reference[i] - port[i])
        dg = abs(reference[i + 1] - port[i + 1])
        db = abs(reference[i + 2] - port[i + 2])
        delta = max(dr, dg, db)
        if delta > threshold:
            bad += 1
            diff[i:i + 3] = 255, min(255, delta * 2), 0
        changed += dr + dg + db
    pixels = VGA_W * VGA_H
    return bad * 100.0 / pixels, bytes(diff), changed // (pixels * 3)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--reference", required=True, type=Path, help="DOSBox-X PNG capture")
    ap.add_argument("--port", required=True, type=Path, help="smoke.py --shot BMP")
    ap.add_argument("--diff", type=Path, help="write a 320x200 red/yellow mismatch image")
    ap.add_argument("--aspect", choices=("auto", "raw", "4:3"), default="auto",
                    help="accept raw VGA, 4:3 corrected, or infer each input independently")
    ap.add_argument("--threshold", type=int, default=0,
                    help="ignore channel differences up to this value (0 means exact)")
    args = ap.parse_args()
    try:
        if not 0 <= args.threshold <= 255:
            raise ValueError("--threshold must be 0..255")
        reference_image = read_image(args.reference)
        port_image = read_image(args.port)
        reference = normalize(reference_image, args.aspect, "reference")
        port = normalize(port_image, args.aspect, "port")
        percent, diff, mae = compare(reference, port, args.threshold)
        diff_path = args.diff or args.port.with_name(args.port.stem + "-diff.png")
        write_png(diff_path, VGA_W, VGA_H, diff)
        print(f"reference {reference_image.width}x{reference_image.height}; "
              f"port {port_image.width}x{port_image.height}; normalized {VGA_W}x{VGA_H}")
        print(f"mismatch: {percent:.2f}% of pixels (threshold {args.threshold}); "
              f"mean absolute channel error: {mae}")
        print(f"diff image: {diff_path}")
        return 0
    except (OSError, ValueError, zlib.error) as exc:
        print(f"framediff: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
