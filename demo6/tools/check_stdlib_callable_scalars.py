#!/usr/bin/env python3
"""Verified F32/time/integer callable programs: both adapters, strict C, JIT/AOT and OOM.

Strict C/hosts link the real shared runtime. Linux --sanitize instruments the
generated C and hosts, not shared libraries. Native pruning is independently
checked by check_xlir_artifact_runtime.ps1, including the unused-signature case.
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
import time


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--public-directory", type=Path)
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    cc = shutil.which("gcc")
    if not cc or out.exists() or (os.name == "nt" and not args.public_directory) or (args.sanitize and os.name == "nt"):
        parser.error("Require gcc, fresh output, Windows source driver and Linux for sanitizers")
    public = json.loads((args.public_directory / "build.json").read_text(encoding="utf-8-sig")) if args.public_directory else None
    if public and (public["RuntimeOnly"] or not public["SourceXrt"]):
        parser.error("Require a full current-source compiler")
    staged = json.loads((args.public_directory / "artifact_sources.json").read_text(encoding="utf-8-sig")) if public else None
    cli = Path(public["Driver"]) if public else root / "release/xl"
    fixtures = ["stdlib_callable_scalars", "stdlib_callable_scalars_unused", "stdlib_callable_integer_domains"]
    files = [Path(__file__), Path(cc), Path(sys.executable), cli,
             root / "test/cases/stdlib_callable_scalars_contract.c",
             root / "test/cases/stdlib_callable_scalars_host.c",
             root / "test/cases/stdlib_callable_integer_domains_host.c",
             root / "test/cases/xlir_callable_bridge_mutations.h",
             root / "test/cases/stdlib_collection_admission_host.c",
             root / "test/cases/xlir_module_product_oom_host.c",
             root / "test/support/module_product_memory.c", root / "test/support/module_product_memory.h"]
    files += [root / f"test/cases/{name}.xl" for name in fixtures]
    for directory in ("src/ir", "src/cgen", "src/compile", "src/runtime", "release/include"):
        files += [p for p in (root / directory).rglob("*") if p.suffix in (".c", ".h", ".inc")]
    if public:
        if digest(cli) != public["DriverSha256"].lower():
            parser.error("Source compiler binary changed")
        for manifest in (public, staged):
            for row in (manifest.get("Inputs", []) + manifest.get("Headers", []) +
                        manifest.get("Objects", []) + manifest.get("Files", [])):
                if "Sha256" in row and digest(Path(row["Path"])) != row["Sha256"].lower():
                    parser.error("Stale source/staged manifest: " + row["Path"])
        for row in staged["Files"]:
            if digest(Path(row["Source"])) != row["Sha256"].lower():
                parser.error("Staged source changed: " + row["Source"])
        files += [args.public_directory / "build.json", args.public_directory / "artifact_sources.json",
                  root / "release/xl.dll", args.public_directory / "xl.dll"]
        files += [Path(row["Path"]) for row in public["Objects"] + public["Inputs"] + staged["Files"]]
    else:
        files += [root / "release/libxl.so", root / "release/libxlc.so"]
    inputs = [{"Path": str(p.resolve()), "Sha256": digest(p)} for p in sorted(set(files))]
    out.mkdir(parents=True)
    private = out / "include/__xl_runtime"
    private.mkdir(parents=True)
    for directory in (root / "src/runtime", root / "release/include"):
        for header in directory.glob("*.h"):
            shutil.copy2(header, private / header.name)
    if public:
        shutil.copy2(root / "release/xl.dll", out / "xl.dll")
    rows: list[dict] = []
    report = {"Scope": __doc__, "Complete": False, "InputsUnchanged": False,
              "Platform": sys.platform, "Sanitized": args.sanitize, "Inputs": inputs,
              "Rows": rows, "Sweeps": [], "Contracts": []}
    env = dict(os.environ)
    if args.sanitize:
        env["ASAN_OPTIONS"] = "detect_leaks=1:abort_on_error=1"
        env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"

    def run(name: str, command: list[str], timeout: int = 60) -> str:
        start = time.monotonic()
        try:
            result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=timeout)
        except subprocess.TimeoutExpired as error:
            (out / (name + ".log")).write_bytes((error.stdout or b"") + (error.stderr or b""))
            rows.append({"Name": name, "Command": command, "Exit": None, "Complete": False, "TimedOut": True})
            raise
        text = result.stdout + result.stderr
        (out / (name + ".log")).write_bytes(text)
        rows.append({"Name": name, "Command": command, "Exit": result.returncode,
                     "Complete": result.returncode == 0, "Seconds": time.monotonic() - start})
        if result.returncode:
            raise RuntimeError(name + ": " + text[-5000:].decode(errors="replace"))
        return text.decode(errors="replace")

    def emit(name: str, mode: str, path: Path) -> list[str]:
        source = f"test/cases/{name}.xl"
        if public:
            return [str(cli), source, mode, str(path)]
        if mode == "ir-lowered":
            return [str(cli), "ir", source, "--lowered", "--format", "json", "--emit", str(path)]
        return [str(cli), "cgen", source, "--emit", str(path)]

    strict = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-Wno-unused-label",
              "-Wno-unused-variable", "-Wno-unused-parameter", "-Wno-unused-const-variable", "-Wno-missing-field-initializers",
              "-I.", "-Irelease", "-Irelease/include", "-I" + str(out / "include")]
    links = ["release/xl.dll"] if public else ["-Lrelease", "-lxl", "-Wl,-rpath," + str(root / "release"), "-pthread", "-ldl", "-lm"]
    if not public:
        strict += ["-D_GNU_SOURCE"]
    if args.sanitize:
        strict += ["-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-pie", "-no-pie"]
    suffix = ".exe" if public else ""
    try:
        compiler_links = ([row["Path"] for row in public["Objects"] if Path(row["Path"]).name != "xlir_public_source_driver.o"] +
                          [*links, "-lws2_32", "-liphlpapi", "-lshell32", "-lole32", "-ladvapi32"] if public else ["-lxlc", *links])
        contract = out / ("contract" + suffix)
        run("contract.build", [cc, "-std=c11", "-O0", "-Wall", "-Wextra", "-Werror", "-DXL_STATIC_EMBED", "-I.",
            "-Irelease", "-Irelease/include", "-Ilib/tcc", "test/cases/stdlib_callable_scalars_contract.c", *compiler_links, "-o", str(contract)])
        for name in fixtures:
            prefix = out / name
            generated, ir, binding = prefix.with_suffix(".c"), prefix.with_suffix(".json"), prefix.with_suffix(".bind.h")
            for repeat in (False, True):
                run(f"{name}.emit.{repeat}", emit(name, "cgen-default", prefix.with_suffix(".repeat.c") if repeat else generated))
                run(f"{name}.ir.{repeat}", emit(name, "ir-lowered", prefix.with_suffix(".repeat.json") if repeat else ir))
            assert generated.read_bytes() == prefix.with_suffix(".repeat.c").read_bytes()
            assert ir.read_bytes() == prefix.with_suffix(".repeat.json").read_bytes()
            integers = name.endswith("_integer_domains")
            log = run(name + ".contract", [str(contract), f"test/cases/{name}.xl", str(prefix.with_suffix(".contract.c")),
                str(binding), str(prefix.with_suffix(".contract.json"))],
                # The 16-domain fixture re-verifies the entire module and both
                # emitters for every plan mutation; keep all checks, with a
                # finite separate budget for this substantially richer oracle.
                timeout=360 if integers else 60)
            proof = re.search(r"scalar bridge: f32=(\d+) time=(\d+) mutations=(\d+) resident-reader=([01]) integers=(\d+) nominal=(\d+)", log)
            assert proof and (int(proof[5]) >= 32 and int(proof[6]) >= 16 if integers else int(proof[1])) and int(proof[3])
            report["Contracts"].append({"Fixture": name, "F32Slots": int(proof[1]), "TimeSlots": int(proof[2]),
                "Mutations": int(proof[3]), "ResidentReader": bool(int(proof[4])),
                "IntegerSlots": int(proof[5]), "NominalIntegerSlots": int(proof[6])})
            assert generated.read_bytes() == prefix.with_suffix(".contract.c").read_bytes()
            assert ir.read_bytes() == prefix.with_suffix(".contract.json").read_bytes()
            if name.endswith("_unused"):
                assert "xl6ValueReadF32V1" not in generated.read_text(encoding="utf-8")
                assert "xl6ValueReadIntegerV1" not in generated.read_text(encoding="utf-8")
            elif integers:
                assert "xl6ValueReadIntegerV1" in generated.read_text(encoding="utf-8")
                assert "xl6ValueBoxIntegerV1" in generated.read_text(encoding="utf-8")
            else:
                assert "xl6ValueReadF32V1" in generated.read_text(encoding="utf-8")
            for level in (0, 2):
                binary = prefix.with_suffix(f".O{level}" + suffix)
                run(f"{name}.O{level}.build", [cc, *strict, f"-O{level}", str(generated),
                    "test/cases/stdlib_collection_admission_host.c", *links, "-o", str(binary)], timeout=360 if args.sanitize and integers else 180 if args.sanitize else 60)
                run(f"{name}.O{level}.run", [str(binary)])
                if name.endswith("_unused"):
                    continue
                lane = f"{name}.O{level}"
                memory = out / f"{lane}.memory.o"
                run(lane + ".memory.build", [cc, *strict, f"-O{level}", "-c", "test/support/module_product_memory.c", "-o", str(memory)])
                vectors = out / (lane + ".vectors" + suffix)
                host = "stdlib_callable_integer_domains_host" if integers else "stdlib_callable_scalars_host"
                run(lane + ".vectors.build", [cc, *strict, f"-O{level}", "-include", str(generated), "-include", str(binding),
                    f"test/cases/{host}.c", str(memory), *links, "-o", str(vectors)], timeout=360 if args.sanitize and integers else 180 if args.sanitize else 60)
                run(lane + ".vectors.run", [str(vectors)])
                module = out / (lane + ".module" + suffix)
                run(lane + ".module.build", [cc, *strict, f"-O{level}", "-DXL6_PRODUCT_USER_ENTRY", "-include", str(generated),
                    "test/cases/xlir_module_product_oom_host.c", str(memory), *links, "-o", str(module)], timeout=180 if args.sanitize else 60)
                for operation in ("module", "forward", "reverse"):
                    for point in range(4097):
                        command = [str(module), str(point)] if operation == "module" else [str(vectors), operation, str(point)]
                        proof = run(f"{lane}.{operation}.{point}", command)
                        assert re.search(r"hit=[01] balanced", proof)
                        if "hit=0 balanced" in proof:
                            report["Sweeps"].append({"Lane": f"{lane}.{operation}", "FirstNoHit": point})
                            break
                    else:
                        raise AssertionError("Allocation sweep did not close")
            source = f"test/cases/{name}.xl"
            log = run(name + ".jit", [str(cli), source, "run", str(prefix)] if public else [str(cli), "run", source])
            assert not re.search(r"\[(?:sem|checked|cgen)\]", log)
            product = prefix.with_suffix(".exe" if public else ".product")
            run(name + ".aot.build", [str(cli), source, "exe", str(prefix)] if public else [str(cli), "build", source, "-o", str(product)])
            assert product.is_file()
            run(name + ".aot.run", [str(product)])
        report["InputsUnchanged"] = all(digest(Path(row["Path"])) == row["Sha256"] for row in inputs)
        report["Complete"] = report["InputsUnchanged"]
        assert report["Complete"], "Frozen inputs changed"
    except Exception as error:
        report["Error"] = f"{type(error).__name__}: {error}"
        print(error, file=sys.stderr)
    finally:
        (out / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Callable scalars: {len(rows)} checks, complete={report['Complete']}")
    return 0 if report["Complete"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
