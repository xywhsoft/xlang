"""Use Python tarfile's independent integer encoder for C/XLang metadata parity."""
import argparse
import io
import json
import os
from pathlib import Path
import subprocess
import tarfile


def make_case(field, value, pax=False):
    entry = tarfile.TarInfo("numeric.txt")
    entry.mode = 0o644
    offsets = {"mtime": (136, 12), "mode": (100, 8), "uid": (108, 8),
               "gid": (116, 8), "devmajor": (329, 8), "devminor": (337, 8),
               "size": (124, 12)}
    if pax:
        setattr(entry, field, value)
        payload = entry.tobuf(format=tarfile.PAX_FORMAT, encoding="utf-8")
    else:
        payload = bytearray(entry.tobuf(format=tarfile.GNU_FORMAT, encoding="utf-8"))
        offset, width = offsets[field]
        encoded = tarfile.itn(value, width, tarfile.GNU_FORMAT)
        if tarfile.nti(encoded) != value:
            raise AssertionError("independent encoder did not preserve the integer")
        payload[offset:offset + width] = encoded
        total = tarfile.calc_chksums(payload[:512])[0]
        payload[148:156] = f"{total:06o}\0 ".encode("ascii")
    payload = bytes(payload) + bytes(1024)
    if pax:
        with tarfile.open(fileobj=io.BytesIO(payload), mode="r:", encoding="utf-8") as reader:
            decoded = reader.next()
    else:
        # 头字段解码与归档偏移校验分开，负 size 不必进入 Python 的文件遍历器。
        decoded = tarfile.TarInfo.frombuf(payload[:512], "utf-8", "strict")
    if decoded is None or getattr(decoded, field) != value:
        raise AssertionError("independent TAR decoder changed the metadata")
    valid = (-(1 << 63) <= decoded.mtime < (1 << 63) and
             0 <= decoded.mode < (1 << 32) and
             0 <= decoded.uid < (1 << 64) and 0 <= decoded.gid < (1 << 64) and
             0 <= decoded.devmajor < (1 << 32) and 0 <= decoded.devminor < (1 << 32) and
             decoded.size == 0)
    expected = (f"mtime={decoded.mtime};mode={decoded.mode};uid={decoded.uid};gid={decoded.gid};"
                f"major={decoded.devmajor};minor={decoded.devminor}" if valid else "invalid:3")
    return payload, expected


def compare(label, command, release, work, cases):
    result = subprocess.run(command, cwd=release, capture_output=True, text=True,
                            encoding="utf-8", timeout=90)
    (work / f"{label}.json").write_text(json.dumps({
        "command": [str(item) for item in command], "exit": result.returncode,
        "stdout": result.stdout, "stderr": result.stderr,
    }, ensure_ascii=False, indent=2), encoding="utf-8")
    actual = result.stdout.splitlines()
    expected = [item["expected"] for item in cases]
    if result.returncode != 0 or actual != expected:
        raise AssertionError(f"{label}: metadata differs\n{result.stdout}\n{result.stderr}")
    print(f"TAR numeric {label}: {len(cases)} cases PASS")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("release", type=Path)
    parser.add_argument("c_probe", type=Path)
    parser.add_argument("work", type=Path)
    parser.add_argument("--aot", type=Path)
    args = parser.parse_args()
    release = args.release.resolve()
    work = args.work.resolve()
    fixtures = work / "numeric_cases"
    fixtures.mkdir()
    specs = [("mtime", n, False) for n in
             (-(1 << 64), -(1 << 63) - 1, -(1 << 63), -1, 0, (1 << 63) - 1, 1 << 63)]
    specs += [("mode", n, False) for n in
              (0, 0o7777, (1 << 31) - 1, 1 << 31, (1 << 32) - 1, 1 << 32, (1 << 32) + 420, -1)]
    for field in ("uid", "gid"):
        specs += [(field, n, False) for n in (0, (1 << 56) - 1, -1)]
        specs += [(field, n, True) for n in ((1 << 63) - 1, 1 << 63, (1 << 64) - 1, 1 << 64, -1)]
    for field in ("devmajor", "devminor"):
        specs += [(field, n, False) for n in ((1 << 31) - 1, 1 << 31, (1 << 32) - 1, 1 << 32)]
    specs.append(("size", -1, False))
    cases = []
    for index, (field, value, pax) in enumerate(specs):
        payload, expected = make_case(field, value, pax)
        path = fixtures / f"{index:02d}_{field}.tar"
        path.write_bytes(payload)
        cases.append({"field": field, "value": value, "pax": pax,
                      "file": str(path), "expected": expected})
    (work / "numeric_expected.json").write_text(json.dumps(cases, indent=2), encoding="utf-8")
    files = [item["file"] for item in cases]
    xl = release / ("xl.exe" if os.name == "nt" else "xl")
    compare("C", [str(args.c_probe.resolve()), *files], release, work, cases)
    compare("JIT", [str(xl), "run", "extlibs/tar/test/numeric_fields.xl", "--", *files],
            release, work, cases)
    if args.aot:
        compare("AOT", [str(args.aot.resolve()), *files], release, work, cases)


if __name__ == "__main__":
    main()
