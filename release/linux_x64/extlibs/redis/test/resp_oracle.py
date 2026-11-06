"""Independent RESP2 grammar/value oracle; no package encoder or parser is imported."""
import argparse
from pathlib import Path
import random
import re
import subprocess


class Incomplete(Exception):
    pass


class Invalid(Exception):
    pass


def blob(value):
    try:
        value.decode("utf8", errors="strict")
        kind = "S"
    except UnicodeDecodeError:
        kind = "B"
    return kind + value.hex()


def parse(data):
    nodes = 1

    def value(at, depth, following):
        nonlocal nodes
        if at == len(data):
            raise Incomplete
        if depth >= 64:
            raise Invalid
        kind, start = data[at:at + 1], at + 1
        if kind not in (b"+", b"-", b":", b"$", b"*"):
            raise Invalid
        end = start
        while end < len(data) and data[end] not in (10, 13):
            end += 1
        if end == len(data) or data[end:] == b"\r":
            raise Incomplete
        if data[end:end + 2] != b"\r\n":
            raise Invalid
        header, at = data[start:end], end + 2
        if at + 3 * following > 64 * 1024 * 1024:
            raise Invalid
        if kind in (b"+", b"-"):
            return kind.decode("ascii") + blob(header), at
        # 整数可带正负号；bulk/array长度只能是十进制数字或精确的null标记-1。
        grammar = rb"[+-]?[0-9]+" if kind == b":" else rb"(?:[0-9]+|-1)"
        if not re.fullmatch(grammar, header):
            raise Invalid
        number = int(header)
        if not -(2**63) <= number < 2**63:
            raise Invalid
        if kind == b":":
            return "I" + str(number), at
        if number == -1:
            return "N", at
        if number < 0 or number > 64 * 1024 * 1024:
            raise Invalid
        if kind == b"$":
            if at + number + 2 + 3 * following > 64 * 1024 * 1024:
                raise Invalid
            if at + number + 2 > len(data):
                raise Incomplete
            if data[at + number:at + number + 2] != b"\r\n":
                raise Invalid
            return "$" + blob(data[at:at + number]), at + number + 2
        nodes += number
        if nodes > 100000 or number and depth == 63 or at + 3 * (number + following) > 64 * 1024 * 1024:
            raise Invalid
        children = []
        for index in range(number):
            child, at = value(at, depth + 1, following + number - index - 1)
            children.append(child + ";")
        return "[" + "".join(children) + "]", at

    try:
        result, end = value(0, 0, 0)
        return "C", result + "|" + blob(data[end:])
    except Incomplete:
        return "I", "-"
    except Invalid:
        return "E", "-"


def cases():
    rng = random.Random(0x5245535032)
    payloads = (b"", b"OK", b"a\0b", b"\x00\xff\x80", "你好".encode(), b"\r\n", b"\xc0\x80", b"\xf4\x90\x80\x80")

    def generate(depth):
        choice = rng.randrange(6 if depth < 4 else 5)
        raw = rng.choice(payloads)
        if choice == 0:
            return b":" + str(rng.choice((0, -1, 2**63 - 1, -(2**63), rng.randrange(-100000, 100000)))).encode() + b"\r\n"
        if choice == 1:
            return rng.choice((b"$-1\r\n", b"*-1\r\n"))
        if choice == 2:
            return b"$" + str(len(raw)).encode() + b"\r\n" + raw + b"\r\n"
        if choice in (3, 4):
            return (b"+" if choice == 3 else b"-") + raw.replace(b"\r", b"R").replace(b"\n", b"N") + b"\r\n"
        children = [generate(depth + 1) for _ in range(rng.randrange(5))]
        return b"*" + str(len(children)).encode() + b"\r\n" + b"".join(children)

    result = [b"", b"+OK\r\n", b"*0\r\n", b"$0\r\n\r\n"]
    for number in (b"0", b"+0", b"-0", b"01", b"+01", b"-01", b"-1", b"-001", b"+1", b"-2", b"9223372036854775808"):
        for kind in (b":", b"$", b"*"):
            frame = kind + number + b"\r\n" + (b"a\r\n" if kind == b"$" else b":1\r\n")
            result += [frame, b"*2\r\n:7\r\n" + frame]
    for depth in (1, 2, 31, 62, 63, 64):
        result.append(b"*1\r\n" * depth + b"*0\r\n")
    for _ in range(320):
        frame = generate(0)
        result += [frame, frame + generate(0), frame + b"\x00\xff"]
        if frame:
            at = rng.randrange(len(frame))
            result.append(frame[:at] + bytes([rng.choice((0, 10, 13, 43, 45, 48, 57, 255))]) + frame[at + 1:])
            result.append(frame[:rng.randrange(len(frame))])
    return list(dict.fromkeys(result))


def check(versions, output):
    lines = output.splitlines()
    if len(lines) != len(versions):
        raise AssertionError("Incomplete independent RESP output")
    mismatches = []
    checks = 0
    for frame, line in zip(versions, lines):
        expected = [parse(frame[:end])[0] for end in range(len(frame) + 1)]
        state = "".join(expected)
        wanted = [state, state, parse(frame)[1]]
        if line.split("\t") != wanted and len(mismatches) < 8:
            mismatches.append((frame.hex(), line, wanted))
        checks += len(expected)
    if mismatches:
        raise AssertionError(mismatches)
    return checks


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cli", type=Path)
    parser.add_argument("--xl", type=Path)
    parser.add_argument("--source", type=Path)
    args = parser.parse_args()
    command = [str(args.cli.resolve())] if args.cli else [str(args.xl.resolve()), "run", str(args.source.resolve())]
    frames = cases()
    process = subprocess.run(command, input="".join(f.hex() + "\n" for f in frames),
                             capture_output=True, text=True, encoding="ascii", timeout=120)
    if process.returncode or process.stderr:
        raise AssertionError((process.returncode, process.stderr))
    checks = check(frames, process.stdout)
    # 正文、状态和流水线余量任一被篡改都必须被判据拒绝。
    for changed in (process.stdout.replace("I", "C", 1), process.stdout.replace("|S", "|B", 1), process.stdout.replace("+S4f4b", "+S4f4a", 1)):
        if changed == process.stdout:
            raise AssertionError("Negative-control target not covered")
        try:
            check(frames, changed)
        except AssertionError:
            continue
        raise AssertionError("Corrupted oracle output accepted")
    print(f"redis independent RESP: {len(frames)} frames, {checks} prefixes, 3 negative controls PASS")


if __name__ == "__main__":
    main()
