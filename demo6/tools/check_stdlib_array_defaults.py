#!/usr/bin/env python3
"""Resident boxed Array defaults: strict C, failure atomicity, aliases and full OOM prefixes.

Not native Sequence/language Array migration. --shared checks real product ABI;
Linux --sanitize instruments the entire selected native closure except in --shared.
"""
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
    parser.add_argument("--shared", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    cc, nm = shutil.which("gcc"), shutil.which("nm")
    if not cc or not nm or out.exists() or (args.sanitize and os.name == "nt"):
        parser.error("Require gcc/nm, a fresh output and Linux for sanitizers")
    sources = ["test/cases/stdlib_array_default_native_host.c", "test/support/array_default_native.c",
               "src/runtime/xl6_array_default.c"]
    product = root / ("release/xl.dll" if os.name == "nt" else "release/libxl.so")
    files = [Path(__file__), Path(cc), Path(nm), Path(sys.executable), root / "lib/xrt/xrt.h"]
    files += [root / name for name in sources]
    files += sorted((root / "src/runtime").rglob("*.h"))
    files += sorted((root / "release/include").rglob("*.h"))
    if args.shared:
        files += [product]
    inputs = [{"Path": str(p.resolve()), "Sha256": digest(p)} for p in sorted(set(files))]
    out.mkdir(parents=True)
    if args.shared and os.name == "nt":
        shutil.copy2(product, out / "xl.dll")
    rows, sweeps = [], []
    report = {"Scope": __doc__, "Complete": False, "InputsUnchanged": False,
              "Sanitized": args.sanitize, "SharedRuntime": args.shared,
              "RuntimeInstrumented": args.sanitize and not args.shared,
              "Inputs": inputs, "Rows": rows, "Sweeps": sweeps}
    env = dict(os.environ)
    strict = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-I.", "-Irelease", "-Irelease/include"]
    links = [] if os.name == "nt" else ["-pthread", "-lm"]
    compiled = sources
    if args.shared:
        compiled = sources[:1]
        links = [str(product)] if os.name == "nt" else ["-Lrelease", "-lxl", "-Wl,-rpath,"+str(root/"release"), *links]
    if os.name != "nt":
        strict += ["-D_GNU_SOURCE"]
    if args.sanitize:
        strict += ["-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-pie", "-no-pie"]
        env.update(ASAN_OPTIONS="detect_stack_use_after_return=1:detect_leaks=1:halt_on_error=1",
                   UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")

    def run(name: str, command: list[str]) -> str:
        result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=60)
        data = result.stdout+result.stderr
        (out/(name+".log")).write_bytes(data)
        rows.append({"Name": name, "Command": command, "Exit": result.returncode, "Complete": result.returncode==0})
        if result.returncode:
            raise RuntimeError(f"{name}: {data[-4000:].decode(errors='replace')}")
        return data.decode(errors="replace")

    try:
        for level in (0,2):
            lane=f"O{level}"
            binary=out/(lane+(".exe" if os.name=="nt" else ""))
            run(lane+".build",[cc,*strict,f"-O{level}",*compiled,*links,"-o",str(binary)])
            symbols=run(lane+".symbols",[nm, *(["-D"] if args.shared and os.name!="nt" else []),
                "--defined-only",str(product if args.shared else binary)])
            names={line.split()[-1].lstrip("_") for line in symbols.splitlines() if line.split()}
            assert {"xl6ArrayResizeDefaultV1","xrtValueArrayExtend"} <= names
            if not args.shared:
                assert not any(re.match(r"xl6(?:ArrayCreate|Sequence|Stack|Log|Regex|Process)|xrtTypedArray",n) for n in names)
            run(lane+".vectors",[str(binary)])
            for abi in range(0,11):
                if abi==1:
                    continue  # FORBIDDEN is refused before allocation in vectors.
                for point in range(513):
                    log=run(f"{lane}.{abi}.{point}",[str(binary),str(abi),str(point)])
                    match=re.search(r"hit=([01]) balanced",log)
                    if not match:
                        raise RuntimeError("Missing balanced allocation outcome")
                    if match[1]=="0":
                        sweeps.append({"Lane":lane,"Abi":abi,"FirstNoHit":point})
                        break
                else:
                    raise RuntimeError("Incomplete allocation prefix")
        report["InputsUnchanged"]=all(digest(Path(row["Path"]))==row["Sha256"] for row in inputs)
        report["Complete"]=report["InputsUnchanged"]
        if not report["Complete"]:
            raise RuntimeError("Acceptance inputs changed")
    except Exception as error:
        report["Error"]=f"{type(error).__name__}: {error}"
        print(error,file=sys.stderr)
    finally:
        (out/"results.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(f"Array defaults: {len(rows)} checks, {len(sweeps)} closed allocation prefixes, complete={report['Complete']}")
    return 0 if report["Complete"] else 1


if __name__=="__main__":
    raise SystemExit(main())
