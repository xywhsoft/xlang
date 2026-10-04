#!/usr/bin/env python3
"""Verify the native Array foundation; does NOT claim language IR migration."""
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
    parser.add_argument("--cc", default="gcc")
    parser.add_argument("--nm", default="nm")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    if out.exists():
        parser.error("Use a fresh evidence directory; preserve previous results")
    if args.sanitize and os.name == "nt":
        parser.error("This gate supports ASan/UBSan on Linux only")
    cc, nm = shutil.which(args.cc), shutil.which(args.nm)
    if not cc or not nm:
        parser.error("Native C compiler and nm are required")
    out.mkdir(parents=True)
    files = [Path(__file__), Path(cc), Path(nm), Path(sys.executable)]
    files += [root / name for name in (
        "test/cases/stdlib_array_native_host.c", "test/cases/stdlib_array_iterator_native_host.c", "test/support/array_native.c",
        "test/support/stack_native.c", "src/runtime/xl6_sequence.c",
        "src/runtime/xl6_array.c", "src/runtime/xl6_stack.c", "src/runtime/xl6_nullable_string.c",
        "src/runtime/xl6_array_iterator.c",
        "src/runtime/xl6_ownership_plan.c", "lib/xrt/xrt.h", "lib/xruntime/xruntime.h")]
    # Freeze project header dependencies, including the real resident layouts.
    for directory in ("src/runtime", "src/api", "release/include"):
        files += sorted((root / directory).rglob("*.h"))
    inputs = [{"Path": str(p.resolve()), "Sha256": digest(p)} for p in sorted(set(files))]
    rows: list[dict[str, object]] = []
    sweeps: list[dict[str, object]] = []
    report = {"Scope": "native Array foundation, not language Array migration",
              "Platform": sys.platform, "Sanitized": args.sanitize, "Inputs": inputs,
              "Rows": rows, "Sweeps": sweeps, "Complete": False, "InputsUnchanged": False}
    (out / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    env = dict(os.environ)
    if args.sanitize:
        env.update(ASAN_OPTIONS="detect_leaks=1:halt_on_error=1",
                   UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")

    def run(name: str, command: list[str]) -> str:
        result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=45)
        data = result.stdout + result.stderr
        (out / (name + ".log")).write_bytes(data)
        rows.append({"Name": name, "Command": command, "Exit": result.returncode,
                     "Complete": result.returncode == 0})
        if result.returncode:
            raise RuntimeError(f"{name}: exit {result.returncode}\n{data[-4000:].decode(errors='replace')}")
        return data.decode(errors="replace")

    strict = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-I.", "-Irelease", "-Irelease/include"]
    if os.name != "nt":
        strict += ["-D_GNU_SOURCE"]
    if args.sanitize:
        strict += ["-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-pie", "-no-pie"]
    operations = ("create", "reserve", "push", "set", "insert", "read", "take", "clone",
                  "trim", "resize", "append", "self-append", "remove", "swap", "reverse", "find",
                  "clear", "equals", "graph-create", "graph-refresh", "graph-commit",
                  "nullable-create", "nullable-push", "nullable-set", "nullable-insert", "nullable-read",
                  "nullable-clone", "nullable-trim", "nullable-append", "nullable-self-append",
                  "cursor-create", "cursor-read", "nullable-cursor-create", "nullable-cursor-read",
                  "cursor-graph-create", "cursor-graph-refresh", "cursor-graph-commit")
    try:
        for level in (0, 2):
            lane = f"O{level}.cursor"
            binary = out / (lane + (".exe" if os.name == "nt" else ""))
            sources = ["test/cases/stdlib_array_iterator_native_host.c", "test/support/array_native.c",
                       "src/runtime/xl6_sequence.c", "src/runtime/xl6_array_iterator.c"]
            links = [] if os.name == "nt" else ["-pthread", "-lm"]
            run(lane + ".build", [cc, *strict, f"-O{level}", *sources, *links, "-o", str(binary)])
            symbols = run(lane + ".symbols", [nm, "--defined-only", str(binary)])
            names = {line.split()[-1].lstrip("_") for line in symbols.splitlines() if line.split()}
            assert "xl6ArrayIteratorCreateV1" in names and "xl6SequenceWrapV1" in names, "Missing snapshot-only closure"
            assert "xrtTypedArrayCreate" in names, "Missing selected typed storage"
            assert not any(re.match(r"xl6(?:Array(?!Iterator)|Stack)[A-Z]|xrtTypedStack[A-Z]", n) for n in names), "Snapshot-only closure pulled Array/Stack facade"
            assert not {"xl6NullableStringTypeV1", "xl6NullableStringEqualV1", "xl6NullableStringConverterV1"} & names, "Scalar snapshot pulled nullable string provider"
            run(lane + ".vectors", [str(binary)])
            for mixed in (False, True):
                lane = f"O{level}.{'mixed' if mixed else 'array'}"
                binary = out / (lane + (".exe" if os.name == "nt" else ""))
                sources = ["test/cases/stdlib_array_native_host.c",
                           "test/support/stack_native.c" if mixed else "test/support/array_native.c",
                           "src/runtime/xl6_sequence.c", "src/runtime/xl6_array.c", "src/runtime/xl6_array_iterator.c", "src/runtime/xl6_nullable_string.c", "src/runtime/xl6_ownership_plan.c"]
                defines = ["-DXL6_TEST_MIXED_STACK"] if mixed else []
                if mixed:
                    sources += ["src/runtime/xl6_stack.c"]
                links = [] if os.name == "nt" else ["-pthread", "-lm"]
                run(lane + ".build", [cc, *strict, f"-O{level}", *defines, *sources, *links, "-o", str(binary)])
                symbols = run(lane + ".symbols", [nm, "--defined-only", str(binary)])
                names = {line.split()[-1].lstrip("_") for line in symbols.splitlines() if line.split()}
                assert "xl6ArrayCreateV1" in names and "xl6SequenceOwnershipAdapterV1" in names, "Missing Array/Sequence resident owner"
                assert "xl6ArrayIteratorCreateV1" in names and "xl6ArrayIteratorReadV1" in names, "Missing actual native snapshot cursor"
                assert "xrtTypedArrayCreate" in names, "Missing actual typed Array implementation"
                assert ("xl6StackCreateV1" in names) == mixed and ("xrtTypedStackCreate" in names) == mixed, "Incorrect Stack closure"
                assert "xl6StackOwnershipAdapterV1" not in names, "Retired Stack ownership adapter remains"
                unrelated = sorted(n for n in names if re.match(r"(?:xl6(?:Regex|Log|Process)|xrt(?:Regex|Logger|Process))[A-Z]", n))
                assert not unrelated, f"Unrelated native closure: {unrelated}"
                run(lane + ".vectors", [str(binary)])
                # Duplicate the logical-allocation prefix at O0/O2. Mixed graph
                # vectors above share the exact same physical owner admission.
                if mixed:
                    continue
                for operation in operations:
                    for point in range(513):
                        log = run(f"{lane}.{operation}.{point}", [str(binary), operation, str(point)])
                        match = re.search(r"hit=([01]) balanced", log)
                        if not match:
                            raise RuntimeError("Missing balanced fault outcome")
                        if match[1] == "0":
                            sweeps.append({"Lane": lane, "Operation": operation, "FirstNoHit": point})
                            break
                    else:
                        raise RuntimeError(f"Incomplete logical allocation prefix: {lane}/{operation}")
        report["InputsUnchanged"] = all(digest(Path(p["Path"])) == p["Sha256"] for p in inputs)
        if not report["InputsUnchanged"]:
            raise RuntimeError("Source or tool changed during acceptance")
        report["Complete"] = True
    except (RuntimeError, AssertionError, subprocess.TimeoutExpired) as exc:
        report["Failure"] = str(exc)
        print(str(exc), file=sys.stderr)
    finally:
        (out / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if not report["Complete"]:
        return 1
    print(f"native Array: {len(rows)} checks, {len(sweeps)} closed fault prefixes; selected native closure balanced")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
