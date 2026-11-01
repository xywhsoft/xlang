"""Independent SemVer grammar/value oracle for all nine public operations on metadata inputs."""
import argparse
from pathlib import Path
import re
import subprocess

PATTERN = re.compile(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)"
                     r"(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?"
                     r"(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?")


def parse(text):
    match = PATTERN.fullmatch(text)
    if not match or any(int(n) > 2**63 - 1 for n in match.group(1, 2, 3)):
        return None
    pre, build = match[4] or "", match[5] or ""
    if len(pre) > 255 or len(build) > 255:
        return None
    if any(p.isascii() and p.isdigit() and len(p) > 1 and p[0] == "0" for p in pre.split(".")):
        return None
    return (*map(int, match.group(1, 2, 3)), pre, build)


def expected(text):
    value = parse(text)
    cleaned = text.strip(" \t\r\n\f\v")
    if cleaned.startswith(("v", "V", "=")):
        cleaned = cleaned[1:].lstrip(" \t\r\n\f\v")
    if not parse(cleaned):
        cleaned = ""
    if value is None:
        return ["0", "0", "-1", "-1", "-1", "-2", "", "", cleaned]
    major, minor, patch, pre, build = value
    return ["1", str(int(not pre)), str(major), str(minor), str(patch), "0", pre, build, cleaned]


def cases():
    result = []
    cores = ("1.2.3", ".".join([str(2**63 - 1)] * 3))
    for core in cores:
        for p in (0, 1, 31, 254, 255, 256):
            for b in (0, 1, 31, 254, 255, 256):
                result.append(core + ("-" + "a" * p if p else "") + ("+" + "0" * b if b else ""))
    for bad in ("", ".", "..", ".a", "a.", "a..b", "a+b", "x/y", "a b", "\x7f", "é"):
        result += ["1.2.3+" + bad, "1.2.3-rc+" + bad]
    result += ["1.2.3+meta.000-a.Z", "1.2.3-rc.1+meta.000-a.Z"]
    for source in ("1.2.3+meta.000", "1.2.3-rc.1+linux-x64", "0.0.0+000", "1.2.3+---"):
        for index in range(len(source) + 1):
            result.append(source[:index])
        for index in range(len(source)):
            for replacement in "a0.-+_/ ":
                result.append(source[:index] + replacement + source[index + 1:])
    return list(dict.fromkeys(result))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("cli", type=Path)
    args = parser.parse_args()
    versions = cases()
    source = "".join("M\t" + value + "\t\n" for value in versions)
    result = subprocess.run([str(args.cli.resolve())], input=source, capture_output=True,
                            text=True, encoding="utf8", timeout=90)
    if result.returncode != 0 or result.stderr:
        raise AssertionError((result.returncode, result.stderr))
    lines = result.stdout.splitlines()
    if len(lines) != len(versions):
        raise AssertionError("Incomplete metadata oracle output")
    for version, line in zip(versions, lines):
        if line.split("\t") != expected(version):
            raise AssertionError(f"Metadata values differ: {version!r}")
    print(f"semver independent metadata: {len(versions)} versions, {len(versions) * 9} values PASS")


if __name__ == "__main__":
    main()
