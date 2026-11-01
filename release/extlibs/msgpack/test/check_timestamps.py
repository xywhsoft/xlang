"""Independent integer/struct reference for timestamp payloads and all ext length headers."""

import argparse
from datetime import datetime
from pathlib import Path
import struct


def ticks():
    epoch = (datetime(1970, 1, 1) - datetime(1, 1, 1)).days * 86400000
    yield from (-(1 << 63), -(1 << 63) + 1,
                epoch-1001, epoch-1000, epoch-999, epoch-1, epoch, epoch+1,
                epoch+999, epoch+1000, epoch+1001, epoch+((1 << 32)-1)*1000,
                epoch+(1 << 32)*1000-1, epoch+(1 << 32)*1000,
                epoch+(1 << 34)*1000-1, epoch+(1 << 34)*1000, (1 << 63)-2, (1 << 63)-1)
    seed = 0x9E3779B97F4A7C15
    for _ in range(512):
        seed = (seed * 6364136223846793005 + 1) & ((1 << 64) - 1)
        yield seed if seed < (1 << 63) else seed - (1 << 64)


def expected():
    epoch = (datetime(1970, 1, 1) - datetime(1, 1, 1)).days * 86400000
    for index, time in enumerate(ticks()):
        seconds, millis = divmod(time - epoch, 1000)
        nanos = millis * 1000000
        if 0 <= seconds < (1 << 32) and nanos == 0:
            payload = struct.pack(">I", seconds)
            canonical = b"\xd6\xff" + payload
        elif 0 <= seconds < (1 << 34):
            payload = struct.pack(">Q", (nanos << 34) | seconds)
            canonical = b"\xd7\xff" + payload
        else:
            payload = struct.pack(">Iq", nanos, seconds)
            canonical = b"\xc7\x0c\xff" + payload
        for head, width in ((0xC7, 1), (0xC8, 2), (0xC9, 4)):
            wire = bytes((head,)) + len(payload).to_bytes(width, "big") + b"\xff" + payload
            yield index, time, head, wire, canonical
        if canonical[0] != 0xC7:
            yield index, time, canonical[0], canonical, canonical


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("records", type=Path)
    args = parser.parse_args()
    rows = args.records.read_text(encoding="ascii").splitlines()
    model = list(expected())
    if len(rows) != len(model):
        raise ValueError(f"timestamp records incomplete: {len(rows)}/{len(model)}")
    failures = 0
    for line, (index, time, head, wire, canonical) in zip(rows, model, strict=True):
        fields = line.split("\t")
        if (len(fields) != 6 or fields[:3] != [str(index), str(time), f"{head:02x}"] or
                bytes.fromhex(fields[3]) != wire or bytes.fromhex(fields[5]) != canonical):
            raise ValueError(f"timestamp corpus/encoding differs at {index}/{head:02x}")
        if fields[4] != f"time:{time}":
            failures += 1
            if failures <= 5:
                print(f"timestamp decode differs at {index}/{head:02x}: {fields[4]}")
    print(f"msgpack timestamp oracle: 530 times, {len(model)} frames, {failures} failures")
    if failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
