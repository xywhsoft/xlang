"""用任意精度整数核对公开装箱结果，不调用被测 checked 实现。"""
import argparse
from fractions import Fraction
import math
from pathlib import Path
import sys

MIN, MAX = -(1 << 63), (1 << 63) - 1


def inputs():
    triples = [(7, 8, 2), (MIN, 0, 1), (MAX, 1, 1), (MIN, -1, -1),
               (MAX, 2, 2), (MIN, 1, -1), (MIN, MIN, MIN), (MAX, MAX, MAX),
               (1, 1, 0), (1, 1, 2), (-1, 1, 2), (1, -1, 2), (3, 1, -2),
               (0, 0, MIN), (MIN, MIN, 0), (1, MAX, MIN)]
    state = 0x9E3779B97F4A7C15
    for i in range(144):
        if i < 16:
            a, b, d = triples[i]
        else:
            values = []
            for _ in range(3):
                state = (state * 6364136223846793005 + 1) & ((1 << 64) - 1)
                values.append(-(state >> 1) if state & 4 else state >> 1)
            a, b, d = values
            if i % 17 == 0:
                d = 0
        for op in range(4):
            yield op, a, b, d, 0
    rates = [-2, -1.5, -1, -0.5, 0, 0.5, 1, 1.5, 2, math.nan, math.inf, -math.inf, sys.float_info.max]
    for a in [-125, -1, 0, 1, 125]:
        for rate in rates:
            yield 4, a, 0, 0, rate
    for a in [MIN, MIN + 1, MAX - 1, MAX]:
        for rate in [-1, 0, 1, 2]:
            yield 4, a, 0, 0, rate


def reference(op, a, b, d, rate):
    if op == 0:
        value = a + b
    elif op == 1:
        value = a - b
    elif op == 2:
        value = a * b
    else:
        if (op == 3 and d == 0) or (op == 4 and not math.isfinite(rate)):
            return "value", "-"
        ratio = Fraction(a * b, d) if op == 3 else a * Fraction(rate)
        # 半值远离零；这里只对 rate 的精确二进制倍率/明确越界做参照。
        q, r = divmod(abs(ratio.numerator), ratio.denominator)
        value = (q + (2 * r >= ratio.denominator)) * (-1 if ratio < 0 else 1)
    return ("ok", str(value)) if MIN <= value <= MAX else ("range", "-")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("records", type=Path)
    lines = parser.parse_args().records.read_text(encoding="ascii").splitlines()
    corpus = list(inputs())
    if len(corpus) != 657 or len(lines) != len(corpus):
        raise ValueError(f"incomplete boxed corpus: {len(lines)}/657")
    for index, row in enumerate(corpus):
        expected = [str(index), str(row[0]), *reference(*row)]
        if lines[index].split("\t") != expected:
            raise ValueError(f"boxed result differs at {index}: {lines[index]} != {expected}")
    print("money integer oracle: 657 records, PASS")


if __name__ == "__main__":
    main()
