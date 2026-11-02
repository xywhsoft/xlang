"""Generate exact full-table LCS results, independently of the C suffix planner."""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import random
import struct


def split(text):
    if not text:
        return []
    lines = text.split(b'\n')
    if not lines[-1]:
        lines.pop()
    return [line[:-1] if line.endswith(b'\r') else line for line in lines]


def brute(left, right):
    # 小序列直接穷举子序列，不依赖下面的动态规划递推。
    a = {tuple(left[i] for i in range(len(left)) if mask >> i & 1)
         for mask in range(1 << len(left))}
    b = {tuple(right[i] for i in range(len(right)) if mask >> i & 1)
         for mask in range(1 << len(right))}
    return max(map(len, a & b))


def reference(old, new):
    left, right = split(old), split(new)
    table = [[0] * (len(right) + 1) for _ in range(len(left) + 1)]
    for i, a in enumerate(left, 1):
        for j, b in enumerate(right, 1):
            table[i][j] = table[i-1][j-1] + 1 if a == b else max(table[i-1][j], table[i][j-1])
    if len(left) <= 4 and len(right) <= 4:
        assert table[-1][-1] == brute(left, right)
    i, j = len(left), len(right)
    reversed_steps = []
    counts = [0, 0, 0]
    while i or j:
        if i and j and left[i-1] == right[j-1]:
            i -= 1
            j -= 1
            reversed_steps.append(b' ' + left[i] + b'\n')
            counts[2] += 1
        elif j and (not i or table[i][j-1] >= table[i-1][j]):
            j -= 1
            reversed_steps.append(b'+' + right[j] + b'\n')
            counts[0] += 1
        else:
            i -= 1
            reversed_steps.append(b'-' + left[i] + b'\n')
            counts[1] += 1
    result = b''.join(reversed(reversed_steps))
    return result, ','.join(map(str, counts)).encode('ascii')


def corpus(directory):
    directory.mkdir(parents=True, exist_ok=False)
    pairs = {}

    def add(old, new):
        a, b = old.encode('utf8'), new.encode('utf8')
        assert b'\0' not in a + b
        pairs[a, b] = None

    words = ['\n'.join(word) for size in range(5) for word in itertools.product(('a', 'b'), repeat=size)]
    for a in words:
        for b in words:
            add(a, b)
    exhaustive_pairs = len(pairs)
    special = ['', '\n', '\r', '\r\n', '\r\r\n', 'a', 'a\n', '\n\n', 'a\r\n', '\rA',
               '你好', '🙂\n', 'é', 'e\u0301']
    for a in special:
        for b in special:
            add(a, b)
    rng = random.Random(0xD1FF)
    tokens = ['', 'a', 'b', '你好', 'é', 'e\u0301', '🙂', '\r', 'x\r', ' \t', '"quoted"']
    for _ in range(512):
        texts = ['\n'.join(rng.choice(tokens) for _ in range(rng.randrange(10))) + rng.choice(('', '\n')) for _ in range(2)]
        add(*texts)
    for length in range(1, 33):
        suffix = '\n'.join('tail-' + str(i % 3) for i in range(length))
        add('a\nb\n' + suffix, 'b\na\n' + suffix)
    unique = [f'line-{i:04d}' for i in range(2048)]
    add('\n'.join(unique), '\n'.join(unique))
    add('\n'.join(unique), '\n'.join(['changed', *unique[1:]]))
    add('\n'.join(unique[1:]), '\n'.join(unique))
    add('', '\n'.join(unique))
    add('x' * (8 * 1024 * 1024), 'x' * (8 * 1024 * 1024))
    binary = bytearray(b'XDIFF01\0' + struct.pack('<I', len(pairs)))
    for old, new in pairs:
        output, stats = reference(old, new)
        fields = (old, new, output, stats)
        binary.extend(struct.pack('<4I', *map(len, fields)))
        binary.extend(b''.join(fields))
    path = directory / 'corpus.bin'
    path.write_bytes(binary)
    # 两个负控保留合法帧、只改期望；第三个另检查截断。
    first_lengths = struct.unpack_from('<4I', binary, 12)
    control = bytearray(b'XDIFF01\0' + struct.pack('<I', 1))
    control.extend(binary[12:28 + sum(first_lengths)])
    for label, field in (('wrong-output', 2), ('wrong-stats', 3)):
        lengths = struct.unpack_from('<4I', control, 12)
        changed = bytearray(control)
        if lengths[field]:
            offset = 28 + sum(lengths[:field])
            changed[offset] ^= 1
        else:
            # 第一个输入为空，输出为空；换成一个期望字节并更新帧长度。
            offset = 28 + sum(lengths[:field])
            changed[offset:offset] = b'!'
            struct.pack_into('<I', changed, 12 + field * 4, 1)
        (directory / (label + '.bin')).write_bytes(changed)
    truncated = control[:-1]
    (directory / 'truncated.bin').write_bytes(truncated)
    report = {'cases': len(pairs), 'exhaustiveBinaryPairs': exhaustive_pairs,
              'maxLines': 2048, 'maxTextBytes': 8 * 1024 * 1024,
              'bytes': len(binary), 'sha256': hashlib.sha256(binary).hexdigest(),
              'seed': '0xD1FF', 'reference': 'full matrix, bottom-right exact match first, insert on tie',
              'smallSequenceIndependentBruteForce': True, 'format': '8-byte magic, u32 count, four u32 sizes, four byte strings'}
    (directory / 'corpus.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps(report, ensure_ascii=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=Path)
    corpus(parser.parse_args().directory)
