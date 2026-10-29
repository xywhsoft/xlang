#!/usr/bin/env python3
"""Ordinary generic FixedStack: IR refusals, strict C, actual language OOM, JIT/AOT.

Uses the real shared runtime, not a pruned closure. Linux sanitizer flags cover
generated C and the test host, not the resident shared libraries.
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
import traceback


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
        parser.error("Require gcc, a fresh output, Windows current-source driver and Linux for sanitizers")
    public = json.loads((args.public_directory / "build.json").read_text(encoding="utf-8-sig")) if args.public_directory else None
    if public and (public["RuntimeOnly"] or not public["SourceXrt"]):
        parser.error("Require the full current-source compiler")
    staged = None
    if public:
        staged = json.loads((args.public_directory / "artifact_sources.json").read_text(encoding="utf-8-sig"))
    cli = Path(public["Driver"]) if public else root / "release/xl"
    fixtures = ("stdlib_fixed_stack_only", "stdlib_fixed_stack", "xlir_generic_tuple_returns")
    refusals = ("sem_fixed_stack_private_error", "sem_fixed_stack_reserve_error", "sem_fixed_stack_payload_error",
                "sem_generic_nested_nullable_error")
    files = [Path(__file__), Path(cc), Path(sys.executable), cli,
             root / "test/cases/xlir_fixed_stack_contract.c", root / "test/cases/xlir_module_product_oom_host.c",
             root / "test/support/module_product_memory.c", root / "test/support/module_product_memory.h"]
    files += [root / f"test/cases/{name}.xl" for name in (*fixtures, *refusals)]
    for directory in ("src/ir", "src/cgen", "src/compile", "src/sem", "src/stdlib", "src/runtime", "release/include"):
        files += sorted(p for p in (root / directory).rglob("*") if p.suffix in (".c", ".h", ".inc"))
    if public:
        files += [args.public_directory / "build.json", args.public_directory / "artifact_sources.json",
                  args.public_directory / "xl.dll", root / "release/xl.dll"]
        files += [Path(row["Path"]) for row in (*public["Objects"], *public["Inputs"], *staged["Files"])]
    else:
        files += [root / "release/libxl.so", root / "release/libxlc.so"]
    inputs = [{"Path": str(p.resolve()), "Sha256": digest(p)} for p in sorted(set(files))]
    out.mkdir(parents=True)
    private = out / "include/__xl_runtime"
    private.mkdir(parents=True)
    for directory in (root / "src/runtime", root / "release/include"):
        for header in directory.glob("*.h"):
            shutil.copy2(header, private / header.name)
    if os.name == "nt":
        shutil.copy2(root / "release/xl.dll", out / "xl.dll")
    rows = []
    report = {"Scope": __doc__, "Complete": False, "InputsUnchanged": False, "Platform": sys.platform,
              "Sanitized": args.sanitize, "Inputs": inputs, "Rows": rows, "Sweeps": [], "Contracts": []}
    env = dict(os.environ)
    if args.sanitize:
        env.update(ASAN_OPTIONS="detect_leaks=1:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")

    def run(name: str, command: list[str], timeout: int = 60) -> str:
        started = time.monotonic()
        try:
            result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=timeout)
        except subprocess.TimeoutExpired as error:
            (out / (name + ".log")).write_bytes((error.stdout or b"") + (error.stderr or b""))
            rows.append({"Name": name, "Command": command, "Exit": None, "Complete": False, "TimedOut": True})
            raise
        data = result.stdout + result.stderr
        (out / (name + ".log")).write_bytes(data)
        rows.append({"Name": name, "Command": command, "Exit": result.returncode,
                     "Complete": result.returncode == 0, "Seconds": time.monotonic() - started})
        if result.returncode:
            raise RuntimeError(f"{name}: exit {result.returncode}: {data[-3000:].decode(errors='replace')}")
        return data.decode(errors="replace")

    def emit(fixture: str, mode: str, target: Path) -> list[str]:
        source = f"test/cases/{fixture}.xl"
        if public:
            return [str(cli), source, mode, str(target)]
        if mode == "ir-lowered":
            return [str(cli), "ir", source, "--lowered", "--format", "json", "--emit", str(target)]
        return [str(cli), "cgen", source, "--inc-path", str(out / "include"), "--emit", str(target)]

    strict = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-Wno-unused-label",
              "-Wno-unused-variable", "-Wno-unused-parameter", "-Wno-unused-const-variable", "-Wno-missing-field-initializers",
              "-I.", "-Irelease", "-Irelease/include", "-I" + str(out / "include")]
    links = ["release/xl.dll"] if os.name == "nt" else ["-Lrelease", "-lxl", "-Wl,-rpath," + str(root / "release"), "-pthread", "-ldl", "-lm"]
    if os.name != "nt":
        strict += ["-D_GNU_SOURCE"]
    if args.sanitize:
        strict += ["-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-pie", "-no-pie"]
    try:
        contract = out / ("contract.exe" if os.name == "nt" else "contract")
        compiler_links = ([row["Path"] for row in public["Objects"] if Path(row["Path"]).name != "xlir_public_source_driver.o"] +
                          ["release/xl.dll", "-lws2_32", "-liphlpapi", "-lshell32", "-lole32", "-ladvapi32"] if public else ["-lxlc", *links])
        run("contract.build", [cc, "-std=c11", "-O0", "-Wall", "-Wextra", "-Werror", "-DXL_STATIC_EMBED",
            "-I.", "-Irelease", "-Irelease/include", "-Ilib/tcc", "test/cases/xlir_fixed_stack_contract.c", *compiler_links, "-o", str(contract)])
        for fixture in fixtures:
            prefix = out / fixture
            generated, ir = prefix.with_suffix(".c"), prefix.with_suffix(".json")
            for repeat in (False, True):
                run(f"{fixture}.emit.{repeat}", emit(fixture, "cgen-default", generated if not repeat else prefix.with_suffix(".repeat.c")))
                run(f"{fixture}.ir.{repeat}", emit(fixture, "ir-lowered", ir if not repeat else prefix.with_suffix(".repeat.json")))
            assert generated.read_bytes() == prefix.with_suffix(".repeat.c").read_bytes()
            assert ir.read_bytes() == prefix.with_suffix(".repeat.json").read_bytes()
            lowered = json.loads(ir.read_text(encoding="utf-8"))
            if fixture.startswith("stdlib_fixed_stack"):
                assert any(row["admissionName"] == "generic-dependent" for row in lowered["intrinsicCalls"])
                text = generated.read_text(encoding="utf-8")
                assert "xl6StackCreateV1" not in text and "xl6SequencePrimitiveCreateV1(2," in text
                assert "fixedStack" not in text  # No dedicated runtime/Cgen ABI.
            contract_c = prefix.with_suffix(".contract.c")
            proof = run(f"{fixture}.contract", [str(contract), f"test/cases/{fixture}.xl", str(contract_c)], timeout=180)
            match = re.search(r"generic resource contract: (\d+) dependent; (\d+) concrete; (\d+) void; (\d+) nullable; (\d+) products; (\d+) mutations refused", proof)
            assert match and int(match[6]) > 0
            report["Contracts"].append({"Fixture": fixture, "Dependent": int(match[1]), "Concrete": int(match[2]),
                "Void": int(match[3]), "Nullable": int(match[4]), "Products": int(match[5]), "RefusedMutations": int(match[6])})
            assert generated.read_bytes() == contract_c.read_bytes()
            for level in (0, 2):
                memory = out / f"memory.O{level}.o"
                if not memory.exists():
                    run(f"memory.O{level}.build", [cc, *strict, f"-O{level}", "-c", "test/support/module_product_memory.c", "-o", str(memory)])
                binary = prefix.with_suffix(f".O{level}" + (".exe" if os.name == "nt" else ""))
                run(f"{fixture}.O{level}.build", [cc, *strict, f"-O{level}", "-DXL6_PRODUCT_USER_ENTRY", "-include", str(generated),
                    "test/cases/xlir_module_product_oom_host.c", str(memory), *links, "-o", str(binary)], timeout=360 if args.sanitize else 120)
                assert "hit=0 balanced" in run(f"{fixture}.O{level}.normal", [str(binary)])
                for point in range(4097):
                    proof = run(f"{fixture}.O{level}.oom.{point}", [str(binary), str(point)])
                    assert re.search(r"hit=[01] balanced", proof)
                    if "hit=0 balanced" in proof:
                        report["Sweeps"].append({"Fixture": fixture, "Level": level, "FirstNoHit": point})
                        break
                else:
                    raise AssertionError("Unclosed language allocation prefix")
            source = f"test/cases/{fixture}.xl"
            jit = [str(cli), source, "run", str(prefix)] if public else [str(cli), "run", source, "--inc-path", str(out / "include")]
            log = run(f"{fixture}.jit", jit)
            assert not re.search(r"\[(?:sem|checked|cgen)\]", log)
            if public:
                run(f"{fixture}.aot.build", [str(cli), source, "exe", str(prefix)])
                product = prefix.with_suffix(".exe")
            else:
                product = prefix.with_suffix(".product")
                run(f"{fixture}.aot.build", [str(cli), "build", source, "--inc-path", str(out / "include"), "-o", str(product)])
            assert product.is_file()
            run(f"{fixture}.aot.run", [str(product)])
        for fixture in refusals:
            target = out / (fixture + ".refused.c")
            command = emit(fixture, "cgen-default", target)
            result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=60)
            data = result.stdout + result.stderr
            (out / (fixture + ".log")).write_bytes(data)
            phase = b"[parse]" if fixture == "sem_generic_nested_nullable_error" else b"[sem]"
            accepted = result.returncode != 0 and phase in data and (not target.exists() or not target.stat().st_size)
            rows.append({"Name": fixture, "Command": command, "Exit": result.returncode, "Complete": accepted})
            assert accepted, f"{fixture}: expected {phase!r} refusal before C output: {data!r}"
        report["InputsUnchanged"] = all(digest(Path(row["Path"])) == row["Sha256"] for row in inputs)
        assert report["InputsUnchanged"], "Frozen inputs changed during acceptance"
        report["Complete"] = True
    except Exception as error:
        report["Error"] = f"{type(error).__name__}: {error}"
        report["Traceback"] = traceback.format_exc()
        print(report["Traceback"], file=sys.stderr)
    finally:
        (out / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"FixedStack: {len(rows)} checks; {len(report['Sweeps'])} closed OOM prefixes; complete={report['Complete']}")
    return 0 if report["Complete"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
