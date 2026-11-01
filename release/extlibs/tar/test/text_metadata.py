"""Independent tarfile writer/reader oracle; never extracts fixture paths."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import tarfile


def fixtures(work):
    directory = work / "text-cases"
    directory.mkdir()
    rows = []
    def save(shape, entries, fmt):
        output = io.BytesIO()
        with tarfile.open(fileobj=output, mode="w:", format=fmt, encoding="utf-8") as writer:
            for entry in entries:
                writer.addfile(entry)
        packed = output.getvalue()
        with tarfile.open(fileobj=io.BytesIO(packed), mode="r:", encoding="utf-8") as reader:
            decoded = reader.getmembers()
        expected = []
        if len(decoded) != len(entries):
            raise AssertionError("Independent decoder changed entry count")
        for original, actual in zip(entries, decoded):
            values = tuple(getattr(original, k) for k in ("name", "linkname", "uname", "gname"))
            if tuple(getattr(actual, k) for k in ("name", "linkname", "uname", "gname")) != values:
                raise AssertionError("Independent encoder/decoder changed text metadata")
            expected.append(";".join(v.encode("utf-8").hex() for v in values))
        path = directory / f"{len(rows):02d}-{shape}.tar"
        path.write_bytes(packed)
        rows.append(dict(file=str(path), shape=shape, expected=expected,
                         sha256=hashlib.sha256(packed).hexdigest()))
    suffixes = ("", " ", "\n", "\n\n", "\r\n", " \n", "\t", "名称 ")
    for shape in ("ustar", "ustar-prefix", "gnu", "gnu-long", "pax-long"):
        fmt = (tarfile.PAX_FORMAT if shape == "pax-long" else
               tarfile.GNU_FORMAT if shape.startswith("gnu") else tarfile.USTAR_FORMAT)
        for suffix in suffixes:
            entry = tarfile.TarInfo("item" + suffix)
            entry.type = tarfile.SYMTYPE
            entry.linkname = "target" + suffix
            entry.uname, entry.gname = "owner" + suffix, "group" + suffix
            if shape == "ustar-prefix":
                entry.name = "p" * 30 + suffix + "/" + "n" * 85 + suffix
            elif shape.endswith("long"):
                entry.name, entry.linkname = "n" * 105 + suffix, "l" * 105 + suffix
                if shape == "pax-long":
                    entry.uname, entry.gname = "u" * 35 + suffix, "g" * 35 + suffix
            save(shape, [entry], fmt)
    save("empty", [], tarfile.GNU_FORMAT)
    entries = [tarfile.TarInfo(name) for name in ("first", "n" * 105 + "\n", "last ")]
    save("multiple", entries, tarfile.GNU_FORMAT)
    (work / "text-expected.json").write_text(json.dumps(rows, ensure_ascii=False, indent=2), encoding="utf-8")
    return rows


def compare(label, command, cwd, work, rows):
    result = subprocess.run([str(x) for x in command], cwd=cwd, capture_output=True,
                            text=True, encoding="utf-8", timeout=120)
    actual = result.stdout.splitlines()
    expected = [line for row in rows for line in row["expected"]]
    wrong = [i for i, line in enumerate(expected) if i >= len(actual) or actual[i] != line]
    receipt = dict(command=[str(x) for x in command], exit=result.returncode,
                   stdout=result.stdout, stderr=result.stderr, mismatches=wrong, records=len(actual))
    (work / (label + ".json")).write_text(json.dumps(receipt, ensure_ascii=False, indent=2), encoding="utf-8")
    if result.returncode or wrong or len(actual) != len(expected):
        raise AssertionError(f"{label}: exit={result.returncode}, mismatches={wrong}, records={len(actual)}")
    print(f"TAR text {label}: {len(rows)} archives, {len(expected)} records PASS")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("release", type=Path)
    parser.add_argument("c_probe", type=Path)
    parser.add_argument("work", type=Path)
    parser.add_argument("--xl-aot", type=Path)
    parser.add_argument("--c-aot", type=Path)
    parser.add_argument("--c-only", action="store_true")
    args = parser.parse_args()
    release, work = args.release.resolve(), args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    rows = fixtures(work)
    files = [row["file"] for row in rows]
    compare("C", [args.c_probe.resolve(), *files], release, work, rows)
    if args.c_only:
        return
    xl = release / ("xl.exe" if os.name == "nt" else "xl")
    for source in ("metadata_hex_c.c", "metadata_hex.xl"):
        compare(source, [xl, "run", "extlibs/tar/examples/" + source, "--", *files], release, work, rows)
    for label, executable in (("C-AOT", args.c_aot), ("XL-AOT", args.xl_aot)):
        if executable:
            compare(label, [executable.resolve(), *files], release, work, rows)


if __name__ == "__main__":
    main()
