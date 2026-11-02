"""Independently verify complete shared-memory frames using struct.pack('<q')."""
import argparse
from pathlib import Path
import struct

SIZES = (1, 2, 7, 8, 9, 15, 16, 31, 32, 63, 64, 65, 127, 128)
VALUES = (0, 1, -1, -(1 << 63), (1 << 63) - 1, 0x0102030405060708, -0x0102030405060708)


def verify(text):
    lines = text.splitlines()
    expected = [(size, offset, value) for size in SIZES for offset in range(max(0, size-7)) for value in VALUES]
    if len(lines) != len(expected)+1 or not lines[-1].endswith(', 0 failures'):
        raise ValueError('Incomplete or failing shared-memory record corpus')
    for line, key in zip(lines[:-1], expected):
        marker, size, offset, value, encoded = line.split('\t')
        if marker != 'record' or tuple(map(int, (size, offset, value))) != key:
            raise ValueError('Record ordering/key differs')
        size, offset, value = key
        frame = bytearray((i*29+size) & 255 for i in range(size))
        frame[offset:offset+8] = struct.pack('<q', value)
        if encoded != frame.hex():
            raise ValueError('Wire frame differs from independent struct oracle')
    return len(expected)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('records', type=Path)
    args = parser.parse_args()
    text = args.records.read_text(encoding='utf8')
    count = verify(text)
    # 验证器自身必须能检出正文、顺序及遗漏三类损坏。
    lines = text.splitlines()
    broken = ['\n'.join([lines[0][:-1]+('1' if lines[0][-1] != '1' else '2'), *lines[1:]]),
              '\n'.join([lines[1], lines[0], *lines[2:]]), '\n'.join(lines[1:])]
    for item in broken:
        try:
            verify(item)
        except ValueError:
            continue
        raise RuntimeError('Independent oracle failed a negative control')
    print(f'sharedmem independent layout: {count} frames, 3 rejected controls')
