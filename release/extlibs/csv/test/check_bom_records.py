"""独立用Python csv/utf-8-sig解码完整记录，验证首字段数据BOM没有丢失。"""
import argparse
import csv
import hashlib
import io
import itertools
from pathlib import Path
import subprocess


VALUES = ("", "plain", "\ufeff", "\ufeffdata", "data\ufeff", "\ufeff,part", '\ufeff"part')


def verify(data):
    rows = data.decode("ascii").splitlines()
    model = list(itertools.product((44, 59, 9), (34, 39, 32), range(2), range(2), range(2), range(4), range(7)))
    if len(rows) != len(model):
        raise ValueError("incomplete BOM corpus")
    errors = []
    for index, (record, shape) in enumerate(zip(rows, model, strict=True)):
        fields = record.split("\t")
        if len(fields) != 9 or tuple(map(int, fields[:8])) != (index, *shape):
            raise ValueError(f"record identity differs at {index}")
        delimiter, quote, _newline, _all, bom, position, value_index = shape
        value = VALUES[value_index]
        expected = [[value]] if position == 0 else [["head", value]] if position == 1 else [["head"], [value]] if position == 2 else [[], [value]]
        wire = bytes.fromhex(fields[8])
        actual = list(csv.reader(io.StringIO(wire.decode("utf-8-sig"), newline=""),
                                 delimiter=chr(delimiter), quotechar=chr(quote), strict=True))
        if actual != expected or wire.startswith(b"\xef\xbb\xbf") != bool(bom):
            errors.append(index)
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--driver", type=Path)
    source.add_argument("--xl", type=Path)
    parser.add_argument("--source", type=Path)
    parser.add_argument("--records-out", type=Path, required=True)
    args = parser.parse_args()
    command = [str(args.driver.resolve()), "--records"] if args.driver else [str(args.xl.resolve()), "run", str(args.source.resolve()), "--", "--records"]
    result = subprocess.run(command, capture_output=True, timeout=120)
    args.records_out.write_bytes(result.stdout)
    if result.returncode or result.stderr:
        raise ValueError(f"record producer failed ({result.returncode}): {result.stderr.decode(errors='replace')}")
    errors = verify(result.stdout)
    # 坏输出负控：删除记录、篡改身份、替换结果必须被识别。
    records = result.stdout.splitlines()
    mutants = [b"\n".join(records[:-1]), b"999\t" + result.stdout.split(b"\t", 1)[1],
               result.stdout.replace(b"706c61696e", b"6f74686572", 1)]
    for mutant in mutants:
        try:
            rejected = bool(verify(mutant))
        except (ValueError, UnicodeError, csv.Error):
            rejected = True
        if not rejected:
            raise ValueError("negative control not rejected")
    print(f"CSV independent BOM: 2016 records, {len(errors)} mismatches, 3 negative controls; sha256={hashlib.sha256(result.stdout).hexdigest()}")
    if errors:
        raise ValueError(f"U+FEFF round-trip mismatches: {errors[:12]}")


if __name__ == "__main__":
    main()
