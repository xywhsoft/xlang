"""用 Python 标准库 csv 独立验证字段与方言，不以本库自读自写代替互操作。"""
import argparse
import csv
import io
import itertools
import json
from pathlib import Path
import random
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("driver", type=Path)
    parser.add_argument("work", type=Path)
    args = parser.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    driver = str(args.driver.resolve())
    model, encoded = (args.work / name for name in ("rows.bin", "interop.csv"))
    checks = []
    rng = random.Random(2919)
    alphabet = 'abc,;|\t\r\n "\'你好é😀'
    fixtures = [[], [[]], [[""]], [["a", "", "b"], ["", "", ""]],
                [["姓名", "note"], ["张三", 'a\r\nb, "quoted"'], ["😀", "é"]],
                [["界" * 30001, "x" * 70003]],
                [["".join(rng.choice(alphabet) for _ in range(rng.randrange(40)))
                  for _ in range(rng.randrange(6))] for _ in range(80)]]
    csv.field_size_limit(1 << 22)

    def run(mode, delimiter, quote, newline, quote_all=False, bom=False, success=True):
        result = subprocess.run([driver, mode, str(model if mode == "write" else encoded),
                                 str(encoded), delimiter, quote, newline,
                                 str(int(quote_all)), str(int(bom))], capture_output=True, timeout=30)
        if (result.returncode == 0) != success:
            raise AssertionError((mode, result.returncode, result.stderr.decode(errors="replace")))
        return result.stdout.decode("ascii").replace("\r\n", "\n")

    for index, rows in enumerate(fixtures):
        wire = bytearray()
        for row in rows:
            wire += struct.pack("<I", len(row))
            for field in row:
                data = field.encode("utf-8") + b"\0"
                wire += struct.pack("<I", len(data)) + data
        model.write_bytes(wire)
        expected = "".join(str(len(row)) + "".join(":" + field.encode().hex() for field in row) + "\n"
                           for row in rows)
        for delimiter, quote, newline, quote_all in itertools.product(
                (",", ";", "|", "\t"), ('"', "'"), ("lf", "crlf"), (False, True)):
            bom = (index % 2) == 0
            options = dict(delimiter=delimiter, quotechar=quote, doublequote=True,
                           quoting=csv.QUOTE_ALL if quote_all else csv.QUOTE_MINIMAL)
            run("write", delimiter, quote, newline, quote_all, bom)
            actual = encoded.read_bytes()
            assert actual.startswith(b"\xef\xbb\xbf") == bom
            text = actual.decode("utf-8-sig")
            assert list(csv.reader(io.StringIO(text, newline=""), strict=True, **options)) == rows
            checks.append(f"c-to-python-{index}-{ord(delimiter)}-{ord(quote)}-{newline}-{quote_all}")

            # Python 的 LF writer 不会引用裸 CR，输入用 CRLF 保证生成无歧义的标准 CSV。
            output = io.StringIO(newline="")
            csv.writer(output, lineterminator="\r\n", **options).writerows(rows)
            encoded.write_bytes((b"\xef\xbb\xbf" if bom else b"") + output.getvalue().encode())
            assert run("read", delimiter, quote, newline) == expected
            checks.append(f"python-to-c-{index}-{ord(delimiter)}-{ord(quote)}-{newline}-{quote_all}")

    for index, wire in enumerate((b'"unfinished', b'"closed"junk\r\n', b'"double""',
                                  b'a\x00b\n', b'\xff,ok\n', b'\xef\xbb')):
        encoded.write_bytes(wire)
        run("read", ",", '"', "crlf", success=False)
        checks.append(f"strict-text-reject-{index}")
    report = {"passed": len(checks), "checks": checks,
              "scope": "Python csv independent reader/writer; strict UTF-8/NUL rejection is the package contract"}
    (args.work / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"CSV independent interoperability: {len(checks)} checks passed")


if __name__ == "__main__":
    main()
