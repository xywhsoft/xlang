"""用Python标准库IEEE754 binary16转换核对C解码器，不重用产品数学公式。"""
import math
from pathlib import Path
import struct
import sys


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: check_half.py RECORDS.tsv")
    total = nans = 0
    with Path(sys.argv[1]).open(encoding="ascii") as source:
        for line in source:
            code, representation = line.rstrip("\n").split("\t")
            if len(code) != 4 or len(representation) != 16 or int(code, 16) != total:
                raise ValueError(f"missing or reordered codepoint at record {total}")
            expected = struct.unpack(">e", total.to_bytes(2, "big"))[0]
            actual_bits = int(representation, 16)
            actual = struct.unpack(">d", actual_bits.to_bytes(8, "big"))[0]
            if math.isnan(expected):
                if not math.isnan(actual):
                    raise ValueError(f"{code}: NaN was lost")
                nans += 1  # NaN载荷不属于本包保证，不强求相同payload。
            elif actual_bits != struct.unpack(">Q", struct.pack(">d", expected))[0]:
                raise ValueError(f"{code}: IEEE754 bits differ")
            total += 1
    if (total, nans) != (65536, 2046):
        raise ValueError(f"incomplete binary16 records: {total}/{nans}")
    print("cbor binary16: 65536 values, 2046 NaNs, PASS")


if __name__ == "__main__":
    main()
