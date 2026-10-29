"""用标准库有理数作为独立求逆 oracle，不复用被测浮点实现。"""

import ctypes
import json
import math
import random
import sys
from fractions import Fraction

library = ctypes.CDLL(sys.argv[1])
inverse = library.xlin_inverse2_into
inverse.argtypes = [ctypes.c_double] * 4 + [ctypes.POINTER(ctypes.c_double)] * 4
inverse.restype = ctypes.c_int
rng = random.Random(0x6C696E)
cases = 0
failures = 0
max_relative = 0.0
examples = []


def verify(matrix, tolerance):
    global cases, failures, max_relative
    a, b, c, d = map(Fraction.from_float, matrix)
    determinant = a * d - b * c
    expected = [float(value / determinant) for value in (d, -b, -c, a)]
    outputs = [ctypes.c_double() for _ in range(4)]
    ok = inverse(*matrix, *map(ctypes.byref, outputs)) != 0
    actual = [value.value for value in outputs]
    for value, reference in zip(actual, expected):
        if not math.isfinite(value):
            ok = False
        elif reference == 0.0:
            ok = ok and value == 0.0
        else:
            relative = abs(value / reference - 1.0)
            max_relative = max(max_relative, relative)
            ok = ok and relative <= tolerance
    cases += 1
    if not ok:
        failures += 1
        if len(examples) < 8:
            examples.append({
                "input": [value.hex() for value in matrix],
                "expected": [value.hex() for value in expected],
                "actual": [value.hex() for value in actual],
            })


for exponent in (512, 768, 990, 1023):
    for gap in (1022, 1023, 1024, 1048, 1074):
        for sign in (1.0, -1.0):
            source = [math.ldexp(1.0, exponent), 0.0,
                      math.ldexp(1.0, exponent), math.ldexp(sign, exponent - gap)]
            for row_swap in range(2):
                for col_swap in range(2):
                    verify([source[(row ^ row_swap) * 2 + (col ^ col_swap)]
                            for row in range(2) for col in range(2)], 0.0)

scale = math.ldexp(1.0, -500)
verify([math.ldexp(1.0, -1074), 1.0, 0.75 * scale, scale], 2 * sys.float_info.epsilon)

# 基础整数矩阵避免近奇异，行缩放覆盖六百多个十进数量级。
for _ in range(2000):
    while True:
        base = [rng.randint(-4, 4) for _ in range(4)]
        if abs(base[0] * base[3] - base[1] * base[2]) >= 4:
            break
    row_scales = [rng.randint(-1000, 1000), rng.randint(-1000, 1000)]
    verify([math.ldexp(float(value), row_scales[index // 2])
            for index, value in enumerate(base)], 32 * sys.float_info.epsilon)

print(json.dumps({"cases": cases, "failures": failures,
                  "maxRelativeError": max_relative, "examples": examples}, indent=2))
sys.exit(1 if failures else 0)
