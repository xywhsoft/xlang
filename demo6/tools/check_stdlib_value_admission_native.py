#!/usr/bin/env python3
"""Exact boxed language domains: strict native vectors, cleanup and OOM prefixes."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    cc, nm = shutil.which("gcc"), shutil.which("nm")
    if not cc or not nm or out.exists() or (args.sanitize and os.name == "nt"):
        parser.error("Require gcc/nm, a fresh evidence directory and Linux for sanitizers")
    sources = ["test/cases/stdlib_value_admission_native_host.c", "test/support/value_admission_native.c",
               "src/runtime/xl6_value_admission.c"]
    files = [Path(__file__), Path(cc), Path(nm), Path(sys.executable), root / "lib/xrt/xrt.h"]
    files += [root / name for name in sources]
    for directory in ("src/runtime", "release/include"):
        files += sorted((root / directory).rglob("*.h"))
    inputs = [{"Path": str(p.resolve()), "Sha256": digest(p)} for p in sorted(set(files))]
    out.mkdir(parents=True)
    rows, sweeps = [], []
    report = {"Complete": False, "InputsUnchanged": False, "Platform": sys.platform,
              "Sanitized": args.sanitize, "Scope": __doc__, "Inputs": inputs, "Rows": rows, "Sweeps": sweeps}
    env = dict(os.environ)
    strict = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-I.", "-Irelease", "-Irelease/include"]
    links = [] if os.name == "nt" else ["-pthread", "-lm"]
    if os.name != "nt":
        strict += ["-D_GNU_SOURCE"]
    if args.sanitize:
        strict += ["-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-pie", "-no-pie"]
        env.update(ASAN_OPTIONS="detect_leaks=1:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")

    def run(name: str, command: list[str]) -> str:
        result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=45)
        data = result.stdout + result.stderr
        (out / (name + ".log")).write_bytes(data)
        rows.append({"Name": name, "Command": command, "Exit": result.returncode, "Complete": result.returncode == 0})
        if result.returncode:
            raise RuntimeError(f"{name}: {data[-3000:].decode(errors='replace')}")
        return data.decode(errors="replace")

    try:
        for level in (0, 2):
            lane = f"O{level}"
            binary = out / (lane + (".exe" if os.name == "nt" else ""))
            run(lane + ".build", [cc, *strict, f"-O{level}", *sources, *links, "-o", str(binary)])
            symbols = run(lane + ".symbols", [nm, "--defined-only", str(binary)])
            names = {line.split()[-1].lstrip("_") for line in symbols.splitlines() if line.split()}
            assert {"xl6ValueAdmitsV1", "xl6CollectionCheckedTakeV1", "xl6ValueReadF32V1", "xl6ValueCheckedRetainV1"} <= names
            assert not any(re.match(r"xl6(?:Array|Stack|Log|Regex|Process)[A-Z]|xrtTypedArray", name) for name in names)
            run(lane + ".vectors", [str(binary)])
            for mode in ("validate", "mismatch"):
                for point in range(513):
                    log = run(f"{lane}.{mode}.{point}", [str(binary), mode, str(point)])
                    match = re.search(r"hit=([01]) balanced", log)
                    if not match:
                        raise RuntimeError("Missing balanced allocation outcome")
                    if match[1] == "0":
                        sweeps.append({"Lane": lane, "Operation": mode, "FirstNoHit": point})
                        break
                else:
                    raise RuntimeError("Incomplete allocation prefix")
        report["InputsUnchanged"] = all(digest(Path(row["Path"])) == row["Sha256"] for row in inputs)
        report["Complete"] = report["InputsUnchanged"]
        if not report["Complete"]:
            raise RuntimeError("Acceptance inputs changed")
    except Exception as error:
        report["Error"] = str(error)
        print(error, file=sys.stderr)
    finally:
        (out / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Value admission: {len(rows)} checks, {sum(row['FirstNoHit'] + 1 for row in sweeps)} prefixes, complete={report['Complete']}")
    return 0 if report["Complete"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
