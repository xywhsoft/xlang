"""独立 Bellman-Ford 按 float64 边加法核对，不复用 Dijkstra 或其索引堆。"""
import argparse
import math
from pathlib import Path
import struct


def weights():
    bits = (0, 0x8000000000000000, 1, 0x0010000000000000, 0x0010000000000001,
            0x3CB0000000000000, 0x3FF0000000000000, 0x4000000000000000,
            0x4340000000000000, 0x7FDFFFFFFFFFFFFF, 0x7FEFFFFFFFFFFFFE, 0x7FEFFFFFFFFFFFFF)
    return [struct.unpack(">d", n.to_bytes(8, "big"))[0] for n in bits] + [
        float((1 << 63) - 1), float((1 << 53) + 1), float((1 << 64) - 1), float(1 << 63)]


def edges(sample):
    if sample == 0:
        return []
    if sample in (1, 2):
        return [(0, 1, 11 if sample == 1 else 9), (1, 2, 11)] + ([(0, 3, 10), (3, 2, 0)] if sample == 2 else [])
    if sample == 3:
        return [(0, 1, 11), (0, 1, 2), (1, 2, 3), (2, 3, 4)]
    if sample == 4:
        return [(i, (i + 1) % 6, i % 2) for i in range(6)]
    if sample == 5:
        return [(0, 1, 12), (1, 2, 14), (0, 3, 13), (3, 2, 15)]
    state = (0x6902A531 ^ (sample * 2654435761)) & 0xFFFFFFFF
    result = []
    for source in range(6):
        for target in range(6):
            state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
            if (state >> 24) % 3:
                continue
            result.append((source, target, (state >> 16) % 16))
            if not state & 15:
                result.append((source, target, (state >> 8) % 16))
    return result


def reference(graph, start, values):
    distance = [math.inf] * 6
    reached = [False] * 6
    distance[start], reached[start] = 0.0, True
    for _ in range(5):
        changed = False
        for source, target, index in graph:
            reached[target] |= reached[source]
            if math.isfinite(distance[source]):
                candidate = distance[source] + values[index]
                if candidate < distance[target]:
                    distance[target], changed = candidate, True
        # 可达性与数值松弛分别收敛，不因没有有限更新漏掉溢出后的节点。
        if not changed:
            for _ in range(5):
                for source, target, _ in graph:
                    reached[target] |= reached[source]
            break
    if any(found and not math.isfinite(d) for found, d in zip(reached, distance)):
        return ["range"] + ["-"] * 6
    return ["ok"] + [struct.pack(">d", d).hex() if math.isfinite(d) else "-" for d in distance]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("records", type=Path)
    lines = parser.parse_args().records.read_text(encoding="ascii").splitlines()
    if len(lines) != 3072:
        raise ValueError(f"incomplete weighted corpus: {len(lines)}/3072")
    values = weights()
    overflow = 0
    for sample in range(512):
        graph = edges(sample)
        for start in range(6):
            expected = [str(sample), str(start)] + reference(graph, start, values)
            if lines[sample * 6 + start].split("\t") != expected:
                raise ValueError(f"weighted result differs at sample={sample}, start={start}")
            overflow += expected[2] == "range"
    if not overflow:
        raise ValueError("no overflow case exercised")
    print(f"graph Bellman-Ford oracle: 3072 records, {overflow} range cases, PASS")


if __name__ == "__main__":
    main()
