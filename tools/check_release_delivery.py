"""Offline delivery checks from an isolated copy of the ordinary products/SDK."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--products", type=Path)
    parser.add_argument("--directory", type=Path)
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc"))
    args = parser.parse_args()
    root = args.root.resolve()
    products = (args.products or root / "release").resolve()
    directory = (args.directory or root / "build/test/release-delivery").resolve()
    directory.mkdir(parents=True, exist_ok=True)
    payload = directory / "payload"
    payload.mkdir(exist_ok=True)
    work = directory / "empty-cwd"
    work.mkdir(exist_ok=True)
    shutil.copytree(root / "release/include", payload / "include", dirs_exist_ok=True)
    names = ["xl.exe", "xl.dll", "xlc.dll"] if os.name == "nt" else ["xl", "libxl.so", "libxlc.so"]
    inputs = {}
    for source in (root / "release/include").rglob("*"):
        if source.is_file():
            inputs[str(source)] = hashlib.sha256(source.read_bytes()).hexdigest()
    for name in names:
        source = products / name
        inputs[str(source)] = hashlib.sha256(source.read_bytes()).hexdigest()
        shutil.copy2(source, payload / name)
    cli = payload / names[0]
    env = dict(os.environ)
    for key in list(env):
        if key.startswith("XL6_") or key.startswith("XLANG_"):
            del env[key]
    env["PATH"] = str(payload) + os.pathsep + env.get("PATH", "")
    env["XL6_NESTED_SDK_WORK"] = str(work)
    report = {"complete": False, "inputsUnchanged": False, "inputs": inputs, "checks": []}

    def save():
        (directory / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    def run(name, command):
        result = subprocess.run(list(map(str, command)), cwd=work, env=env,
                                capture_output=True, timeout=120)
        data = result.stdout + result.stderr
        (directory / (name + ".log")).write_bytes(data)
        report["checks"].append({"name": name, "command": list(map(str, command)), "exit": result.returncode})
        save()
        if result.returncode:
            raise RuntimeError(name + " failed: " + data.decode("utf-8", "replace"))
        print(name, flush=True)

    suffix = ".exe" if os.name == "nt" else ""
    for relative in ["test/cases/tcc_c99_vsnprintf.c", "test/cases/stdlib_nested_sdk.xl",
                     "test/cases/stdlib_nested_aot.xl"]:
        source = root / relative
        inputs[str(source)] = hashlib.sha256(source.read_bytes()).hexdigest()
        copied = payload / source.name
        shutil.copy2(source, copied)
        binary = payload / (source.stem + suffix)
        run(source.stem + "-jit", [cli, "run", copied])
        run(source.stem + "-build", [cli, "build", copied, "-o", binary])
        run(source.stem + "-aot", [binary])

    for library_name in names[1:]:
        library = payload / library_name
        for relative in ["release/examples/xl_dll_embed_string.c", "test/cases/stdlib_nested_sdk_host.c"]:
            source = root / relative
            inputs[str(source)] = hashlib.sha256(source.read_bytes()).hexdigest()
            copied = payload / source.name
            shutil.copy2(source, copied)
            name = library.stem + "-" + source.stem
            binary = payload / (name + suffix)
            command = [args.cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(payload),
                       copied, library, "-o", binary]
            if os.name != "nt":
                command += ["-Wl,-rpath," + str(payload)]
            run(name + "-build", command)
            run(name + "-run", [binary, work] if source.stem == "stdlib_nested_sdk_host" else [binary])
    report["inputsUnchanged"] = all(hashlib.sha256(Path(path).read_bytes()).hexdigest() == sha
                                    for path, sha in inputs.items())
    report["complete"] = report["inputsUnchanged"]
    save()
    if not report["complete"]:
        raise RuntimeError("delivery inputs changed during validation")


if __name__ == "__main__":
    main()
