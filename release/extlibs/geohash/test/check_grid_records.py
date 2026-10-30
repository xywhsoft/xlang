"""独立网格参照：Fraction 量化编码，逐字符整数拆位恢复范围与邻格。"""
import argparse
from fractions import Fraction
from pathlib import Path
import struct

ALPHABET = "0123456789bcdefghjkmnpqrstuvwxyz"


def bits(value):
    return struct.pack(">d", value).hex()


def next_state(state):
    return (state * 1664525 + 1013904223) & 0xFFFFFFFF


def text(index, size):
    return "".join(ALPHABET[(index >> (5 * at)) & 31] for at in reversed(range(size)))


def grid_text(x, y, precision):
    nx, ny = (5 * precision + 1) // 2, 5 * precision // 2
    # 分别把经纬索引的每一位放到最终整数的偶/奇槽，不复用 C 二分法。
    value = sum(((x >> at) & 1) << (2 * at + (5 * precision % 2 == 0)) for at in range(nx))
    value |= sum(((y >> at) & 1) << (2 * at + (5 * precision % 2 != 0)) for at in range(ny))
    return text(value, precision)


def encode(lat, lon, precision):
    nx, ny = (5 * precision + 1) // 2, 5 * precision // 2
    x = min((Fraction(lon) + 180) * (1 << nx) // 360, (1 << nx) - 1)
    y = min((Fraction(lat) + 90) * (1 << ny) // 180, (1 << ny) - 1)
    return grid_text(x, y, precision)


def cells():
    for size in [1, 2]:
        for index in range(1 << (5 * size)):
            yield text(index, size)
    state = 0x1A2B3C4D
    for size in range(3, 13):
        for corner in ["000000000000", "zzzzzzzzzzzz", "bpbpbpbpbpbp", "pbpbpbpbpbpb"]:
            yield corner[:size]
        for _ in range(64):
            result = ""
            for _ in range(size):
                state = next_state(state)
                result += ALPHABET[state >> 27]
            yield result


def pairs():
    raw = [(0, 0), (0x8000000000000000, 0), (1, 0x8000000000000001), (0x8000000000000001, 1),
           (0x4056800000000000, 0x4066800000000000), (0xC056800000000000, 0xC066800000000000),
           (0x4056800000000000, 0xC066800000000000), (0xC056800000000000, 0x4066800000000000),
           (0x40567FFFFFFFFFFF, 0x40667FFFFFFFFFFF), (0xC0567FFFFFFFFFFF, 0xC0667FFFFFFFFFFF),
           (0x4046800000000000, 0x4056800000000000), (0x40467FFFFFFFFFFF, 0x40567FFFFFFFFFFF),
           (0x4046800000000001, 0x4056800000000001), (0xC046800000000000, 0xC056800000000000),
           (0xC046800000000001, 0xC056800000000001), (0xC0467FFFFFFFFFFF, 0xC0567FFFFFFFFFFF)]
    for row in raw:
        yield tuple(struct.unpack(">d", n.to_bytes(8, "big"))[0] for n in row)
    state = 0x6927C31D
    for _ in range(64):
        state = next_state(state)
        lat = ((state & 65535) - 32768) * (90 / 32768)
        state = next_state(state)
        lon = ((state & 65535) - 32768) * (180 / 32768)
        yield lat, lon


def decode(hash_text):
    stream = "".join(f"{ALPHABET.index(c):05b}" for c in hash_text)
    xs, ys = stream[::2], stream[1::2]
    x, y, width, height = int(xs, 2), int(ys, 2), 1 << len(xs), 1 << len(ys)
    left, bottom = Fraction(360 * x, width) - 180, Fraction(180 * y, height) - 90
    right, top = left + Fraction(360, width), bottom + Fraction(180, height)
    coordinates = [float(v) for v in [bottom, left, top, right, (bottom + top) / 2, (left + right) / 2]]
    neighbors = [grid_text(x, y + 1, len(hash_text)) if y + 1 < height else "-",
                 grid_text(x, y - 1, len(hash_text)) if y else "-",
                 grid_text((x + 1) % width, y, len(hash_text)), grid_text((x - 1) % width, y, len(hash_text))]
    return coordinates, neighbors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("records", type=Path)
    rows = [line.split("\t") for line in parser.parse_args().records.read_text(encoding="ascii").splitlines()]
    if len(rows) != 2696:
        raise ValueError(f"incomplete grid corpus: {len(rows)}/2696")
    index = 0
    for lat, lon in pairs():
        for precision in range(1, 13):
            expected = ["E", str(index), str(precision), bits(lat), bits(lon), encode(lat, lon, precision)]
            if rows[index] != expected:
                raise ValueError(f"encoding differs at {index}: {rows[index]} != {expected}")
            index += 1
    for hash_text in cells():
        coordinates, neighbors = decode(hash_text)
        row = rows[index]
        expected = ["C", str(index), hash_text, str(len(hash_text)), *map(bits, coordinates)]
        if len(row) != 15 or row[:10] != expected or row[11:] != neighbors:
            raise ValueError(f"cell differs at {index}: {row}")
        bbox = row[10].split(",")
        if len(bbox) != 4 or list(map(bits, map(float, bbox))) != list(map(bits, coordinates[:4])):
            raise ValueError(f"bbox not roundtrippable at {index}: {bbox}")
        index += 1
    if index != 2696:
        raise ValueError(f"incomplete reference: {index}")
    print("geohash rational grid oracle: 2696 records, PASS")


if __name__ == "__main__":
    main()
