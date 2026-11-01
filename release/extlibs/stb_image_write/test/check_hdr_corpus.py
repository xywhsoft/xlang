"""Small independent RGBE decoder for the encoder's flat/RLE value corpus."""
import argparse
import math
from pathlib import Path
import re
import struct
import sys


def decode(data):
    header, rest = data.split(b"\n\n", 1)
    if b"FORMAT=32-bit_rle_rgbe\n" not in header + b"\n":
        raise ValueError("RGBE format header")
    dimensions, body = rest.split(b"\n", 1)
    match = re.fullmatch(rb"-Y (\d+) \+X (\d+)", dimensions)
    if not match:
        raise ValueError("RGBE dimensions")
    height, width = map(int, match.groups())
    offset, pixels = 0, []
    for _ in range(height):
        if 8 <= width <= 32767:
            if body[offset:offset + 4] != bytes((2, 2, width >> 8, width & 255)):
                raise ValueError("RGBE scanline header")
            offset += 4
            channels = []
            for _ in range(4):
                values = bytearray()
                while len(values) < width:
                    count = body[offset]
                    offset += 1
                    if count > 128:
                        values.extend(bytes((body[offset],)) * (count - 128))
                        offset += 1
                    elif count:
                        values.extend(body[offset:offset + count])
                        offset += count
                    else:
                        raise ValueError("RGBE empty packet")
                if len(values) != width:
                    raise ValueError("RGBE packet exceeds row")
                channels.append(values)
            row = zip(*channels)
        else:
            flat = body[offset:offset + 4 * width]
            if len(flat) != width * 4:
                raise ValueError("RGBE truncated flat row")
            row = zip(*(iter(flat),) * 4)
            offset += 4 * width
        for red, green, blue, exponent in row:
            pixels.append(tuple(math.ldexp(v / 256.0, exponent - 128) if exponent else 0.0
                                for v in (red, green, blue)))
    if offset != len(body):
        raise ValueError("RGBE trailing bytes")
    return width, height, pixels


def check(path, data):
    match = re.fullmatch(r"w(\d+)-c[1-4]-b([a-f0-9]{8})\.hdr", path.name)
    if not match:
        raise ValueError("Unexpected corpus file")
    source = struct.unpack("=f", int(match[2], 16).to_bytes(4, sys.byteorder))[0]
    width, height, pixels = decode(data)
    if width != int(match[1]) or height != 2 or len(pixels) != width * height:
        raise ValueError("RGBE dimensions differ from fixture")
    for pixel in pixels:
        for value in pixel:
            if source < 1e-32:
                if value != 0:
                    raise ValueError("RGBE black threshold")
            elif not math.isfinite(value) or value <= 0 or abs(value - source) > source / 128:
                raise ValueError("RGBE value exceeds quantization tolerance")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("corpus", type=Path)
    args = parser.parse_args()
    files = sorted(args.corpus.glob("*.hdr"))
    if len(files) != 96:
        raise AssertionError("Expected all 96 valid value cases")
    for path in files:
        check(path, path.read_bytes())
    control = args.corpus / "w8-c3-b3f800000.hdr"
    original = control.read_bytes()
    variants = (original[:-1], original + b"x", original.replace(b"+X 8\n", b"+X 7\n"))
    for changed in variants:
        try:
            check(control, changed)
        except (ValueError, IndexError):
            pass
        else:
            raise AssertionError("Damaged corpus control was accepted")
    print("HDR independent RGBE values: 96 files, 3 rejection controls PASS")


if __name__ == "__main__":
    main()
