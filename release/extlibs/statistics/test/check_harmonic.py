"""Use exact binary64 inputs and rational reciprocal sums, not the C algorithm."""
import math
from pathlib import Path
import struct
import sys
from fractions import Fraction


def number(text):
    if len(text) != 16:
        raise ValueError("invalid binary64 record")
    result = struct.unpack(">d", bytes.fromhex(text))[0]
    if not math.isfinite(result):
        raise ValueError("nonfinite input/result")
    return result


def check(lines):
    finite = rejected = 0
    for index, line in enumerate(lines):
        fields = line.strip().split("\t")
        if len(fields) != 3 or fields[0] != str(index):
            raise ValueError("record framing/sequence differs")
        values = [Fraction.from_float(number(value)) for value in fields[1].split(",")]
        if not values:
            raise ValueError("missing input")
        reciprocal = sum((1 / value for value in values), Fraction()) if all(values) else Fraction()
        expected = None
        if reciprocal:
            try:
                expected = float(len(values) / reciprocal)
            except OverflowError:
                pass
        if expected is None or not math.isfinite(expected):
            if fields[2] != "error:value":
                raise ValueError(f"{index}: undefined/overflow not rejected")
            rejected += 1
        else:
            if fields[2].startswith("error:"):
                raise ValueError(f"{index}: finite rational {expected.hex()} was rejected")
            actual = number(fields[2])
            if actual != expected and abs(actual - expected) > abs(expected) * 2e-13:
                raise ValueError(f"{index}: {actual.hex()} != rational {expected.hex()}")
            finite += 1
    if len(lines) != 3043:
        raise ValueError("incomplete harmonic corpus")
    return finite, rejected


def main():
    lines = Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
    finite, rejected = check(lines)
    # 负控确保序号、数值与错误路径被实际核对，而不是仅数记录条数。
    for replacement in ("1\t" + lines[0].split("\t", 1)[1],
                        lines[0].rsplit("\t", 1)[0] + "\t0000000000000000",
                        lines[0].rsplit("\t", 1)[0] + "\terror:value"):
        broken = [replacement, *lines[1:]]
        try:
            check(broken)
        except ValueError:
            continue
        raise ValueError("oracle negative control was accepted")
    print(f"statistics harmonic oracle: {len(lines)} records, {finite} finite, {rejected} rejected, 3 controls, PASS")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, IndexError, OSError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
