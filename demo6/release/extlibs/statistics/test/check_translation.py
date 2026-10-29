"""用精确有理数和高精度开方核对输入/输出记录，特别覆盖平移与量纲变化。"""
from decimal import Decimal, localcontext
from fractions import Fraction as F
import math
from pathlib import Path
import struct
import sys

NAMES = ("variance", "varianceSample", "stdev", "stdevSample", "mad",
         "covariance", "covarianceSample", "correlation", "slope", "intercept")


def number(text):
    if len(text) != 16:
        raise ValueError("expected exact binary64 representation")
    return struct.unpack(">d", int(text, 16).to_bytes(8, "big"))[0]


def decimal(value):
    return Decimal(value.numerator)/Decimal(value.denominator)


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: check_translation.py RECORDS.tsv")
    failures = total = 0
    maximum = [0.0]*len(NAMES)
    with localcontext() as context, Path(sys.argv[1]).open(encoding="ascii") as source:
        context.prec = 120
        for line in source:
            fields = line.rstrip("\n").split("\t")
            if len(fields) != 13 or int(fields[0]) != total:
                raise ValueError(f"missing or malformed record at {total}")
            x, y = ([F(number(item)) for item in field.split(",")] for field in fields[1:3])
            n = len(x)
            if n != len(y) or n < 3:
                raise ValueError("paired input dimensions differ")
            mx, my = sum(x)/n, sum(y)/n
            vx = sum((a-mx)**2 for a in x)
            vy = sum((a-my)**2 for a in y)
            cov = sum((a-mx)*(b-my) for a, b in zip(x,y))
            slope = cov/vx
            intercept = my-slope*mx
            expected = [float(vx/n), float(vx/(n-1)), float(decimal(vx/n).sqrt()),
                        float(decimal(vx/(n-1)).sqrt()), float(sum(abs(a-mx) for a in x)/n),
                        float(cov/n), float(cov/(n-1)), float(decimal(cov)/decimal(vx*vy).sqrt()),
                        float(slope), float(intercept)]
            # 截距的条件数受两个大均值相减影响，绝对误差按它们的量级约束。
            scales = [abs(value) for value in expected]
            # 零协方差/斜率也按数据量纲核对，不能对极小量纲套固定绝对容差。
            if not scales[5]:
                scales[5] = float(decimal(vx*vy).sqrt()/n)
            if not scales[6]:
                scales[6] = float(decimal(vx*vy).sqrt()/(n-1))
            if not scales[7]:
                scales[7] = 1.0
            if not scales[8]:
                scales[8] = float(decimal(vy/vx).sqrt())
            scales[-1] = float(abs(my)+abs(slope*mx))
            for i, (actual_bits, want, scale) in enumerate(zip(fields[3:], expected, scales)):
                actual = number(actual_bits)
                error = abs(actual-want)/scale if scale else abs(actual-want)
                maximum[i] = max(maximum[i], error)
                if not math.isfinite(actual) or error > 2e-12:
                    if failures < 12:
                        print(f"record {total} {NAMES[i]}: {actual:.17g} != {want:.17g}", file=sys.stderr)
                    failures += 1
            total += 1
    if total != 1024:
        raise ValueError(f"expected 1024 cases, got {total}")
    print(f"statistics Fraction oracle: {total} cases, {total*len(NAMES)} results, {failures} failures")
    print("max scaled error: "+", ".join(f"{name}={error:.3g}" for name,error in zip(NAMES,maximum)))
    if failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
