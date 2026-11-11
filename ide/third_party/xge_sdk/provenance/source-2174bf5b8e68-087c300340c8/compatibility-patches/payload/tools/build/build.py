#!/usr/bin/env python3
"""Small profile builder: compiler dependency files, cached objects, shared config."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import shlex
import subprocess
import sys
import time
import host_xrt
from relink import QualifiedRelink, check_locations, image_names

ROOT = Path(__file__).resolve().parents[2]
PROFILE_CONFIG = json.loads((ROOT / "config/profiles.json").read_text(encoding="utf-8"))
PROFILES = {name: value["id"] for name, value in PROFILE_CONFIG["profiles"].items()}
INCLUDE_FLAGS = shlex.split((ROOT / "config/includes.rsp").read_text(encoding="utf-8"))


def run(args, **kwargs):
    return subprocess.run([str(a) for a in args], check=True, cwd=ROOT, **kwargs)


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def public_abi_inputs(root):
    paths = sorted(path.relative_to(root).as_posix() for path in (root / "include").glob("*.h"))
    paths.append("extlibs/xrt/xrt.h")
    return "XGE_PUBLIC_ABI_V2\n" + "".join(
        name + "\0" + (root / name).read_text(encoding="utf-8") + "\0" for name in paths)


def stamp(path):
    s = Path(path).stat()
    return [s.st_mtime_ns, s.st_size]


def dependencies(path):
    text = path.read_text().replace("\\\n", " ")
    text = re.split(r":\s", text, maxsplit=1)[1]
    return [re.sub(r"\\(.)", r"\1", p) for p in re.findall(r"(?:\\.|[^\s])+", text)]


def enabled(cc, flags, names):
    source = '#include "xui_config.h"\n'
    for name in names:
        source += f"#if {name}\nXGE_SELECTED_{name}\n#endif\n"
    output = run([cc, *flags, "-E", "-P", "-x", "c", "-"], input=source, text=True, capture_output=True)
    return set(re.findall(r"^XGE_SELECTED_(\w+)$", output.stdout, re.M))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("profile", choices=PROFILES, nargs="?", default="default")
    parser.add_argument("output", nargs="?")
    parser.add_argument("--define", action="append", default=[], help="shared NAME=0/1 override")
    parser.add_argument("--sources-only", action="store_true", help="print selected XUI sources without building")
    parser.add_argument("--source-group", help="print a named source group from config/modules.json")
    parser.add_argument("--debug", action="store_true", help="build the xgedbg library with debug APIs enabled")
    parser.add_argument("--host-xrt", action="store_true", help="omit XRT implementation and bind the host's pinned runtime before use")
    parser.add_argument("--relink-from", type=Path, help="reuse only exact qualified objects from this build, with no compilation fallback")
    parser.add_argument("--relink-source", type=Path, help="explicit immutable source root of --relink-from")
    args = parser.parse_args()
    if bool(args.relink_from) != bool(args.relink_source) or (args.relink_from and not args.host_xrt):
        parser.error("--relink-from and --relink-source are required together, with --host-xrt")
    os.chdir(ROOT)
    manifest = json.loads((ROOT / "config/modules.json").read_text(encoding="utf-8"))
    if args.source_group:
        try:
            print(" ".join(manifest["source_groups"][args.source_group]))
        except KeyError as exc:
            raise RuntimeError(f"Unknown source group: {args.source_group}") from exc
        return
    started = time.perf_counter()
    windows = os.name == "nt"
    cc = shutil.which(os.environ.get("CC", "gcc"))
    if not cc:
        raise RuntimeError("C compiler not found")
    version = run([cc, "--version"], text=True, capture_output=True).stdout
    identity = [cc, stamp(cc), version]
    out = Path(args.output or ("build/ide" if args.profile == "ide" else "build" if args.profile == "default" else f"build/{args.profile}"))
    if args.relink_from:
        check_locations(ROOT, out, args.relink_from, args.relink_source)
    out.mkdir(parents=True, exist_ok=True)
    objects = out / ("obj-debug" if args.debug else "obj")
    objects.mkdir(exist_ok=True)
    flags = ["-O2", "-Wall", "-Wextra", "-Wno-unused-parameter", "-Wno-unused-function",
             "-Wno-cast-function-type", "-DXGE_DLL", "-DXGE_BUILD_DLL", "-DXUI_DLL",
             "-DXUI_BUILD_DLL", "-DBUILD_DLL", f"-DXGE_DEBUGMODE={int(args.debug)}",
             f"-DXGE_BUILD_PROFILE={PROFILES[args.profile]}"]
    flags += INCLUDE_FLAGS
    flags += ["-D" + d for d in args.define]
    host_metadata = None
    host_generated = out / "host-xrt"
    if args.host_xrt:
        host_metadata = host_xrt.generate(ROOT / "extlibs/xrt/xrt.h", host_generated)
        flags += ["-DXGE_XRT_HOST=1", "-I"+host_generated.resolve().as_posix(),
                  "-include", "extlibs/xrt/xrt_config.h", "-include",
                  (host_generated.resolve()/"xge_host_xrt_imports.h").as_posix()]
    if args.profile == "ide":
        flags += ["-DXGE_XRT_PROFILE_IDE"]
        flags += ["-include", "extlibs/xrt/xrt_config.h", "-Iextlibs/xrt", "-Iextlibs/xrt/include"]
        flags += [f"-Iextlibs/xrt/extlibs/{p}/include" for p in ("xllm", "xllm-session", "xwork")]
    if not windows:
        flags += ["-fPIC"]
    names = set(re.findall(r"(?:XGE|XUI|XGE3D)_ENABLE_\w+", (ROOT / "include/xge_config.h").read_text() + (ROOT / "include/xui_config.h").read_text()))
    features = enabled(cc, flags, names)
    if "XGE_ENABLE_TEXT" in features and not any(d.split("=")[0] == "XGE_ENABLE_HARFBUZZ" for d in args.define):
        flags += ["-DXGE_ENABLE_HARFBUZZ=1"]
        features = enabled(cc, flags, names)
    sources = ["single/xge.c"]
    # Evaluate compound source guards with the compiler, not a second macro interpreter.
    selections = list(manifest["xui_sources"].items())
    source_probe = '#include "xui_config.h"\n' + "".join(f"#if XGE_ENABLE_XUI && ({guard})\nXGE_SOURCE_{i}\n#endif\n" for i, (_, guard) in enumerate(selections))
    selected = run([cc, *flags, "-E", "-P", "-x", "c", "-"], input=source_probe, text=True, capture_output=True).stdout
    sources += [selections[int(i)][0] for i in re.findall(r"^XGE_SOURCE_(\d+)$", selected, re.M)]
    if args.sources_only:
        print(" ".join(sources[1:]))
        return
    selections = list(manifest.get("3d_sources", {}).items())
    source_probe = '#include "xge_config.h"\n' + "".join(f"#if {guard}\nXGE_SOURCE_{i}\n#endif\n" for i, (_, guard) in enumerate(selections))
    selected = run([cc, *flags, "-E", "-P", "-x", "c", "-"], input=source_probe, text=True, capture_output=True).stdout
    sources += [selections[int(i)][0] for i in re.findall(r"^XGE_SOURCE_(\d+)$", selected, re.M)]
    if args.profile == "ide":
        for product in ("xllm", "xllm-session", "xwork"):
            extension = json.loads((ROOT / f"extlibs/xrt/extlibs/{product}/config/modules.json").read_text())
            sources += ["extlibs/xrt/" + source for module in extension["modules"] for source in module["sources"]]
    config = "#ifndef XGE_BUILD_CONFIG_H\n#define XGE_BUILD_CONFIG_H\n"
    api_major = int(re.search(r"#define\s+XGE_VERSION_MAJOR\s+(\d+)", (ROOT / "include/xge.h").read_text(encoding="utf-8")).group(1))
    config += f"#define XGE_BUILD_API_MAJOR {api_major}\n"
    config += f"#define XGE_BUILD_PROFILE {PROFILES[args.profile]}\n"
    config += "".join(f"#define {name} {int(name in features)}\n" for name in sorted(names))
    if args.profile == "ide":
        config += "#define XGE_XRT_PROFILE_IDE\n"
    if args.host_xrt:
        config += "#define XGE_XRT_HOST 1\n"
    abi_inputs = config + public_abi_inputs(ROOT)
    abi_value = hashlib.sha256((abi_inputs + version + str(int(args.debug))).encode()).hexdigest()[:16]
    config += f"#define XGE_BUILD_ABI_FINGERPRINT UINT64_C(0x{abi_value})\n"
    flags += [f"-DXGE_BUILD_ABI_FINGERPRINT=0x{abi_value}ULL"]
    config += "#endif\n"
    config_path = out / "xge_build_config.h"
    if not config_path.exists() or config_path.read_text() != config:
        config_path.write_text(config)
    libs = ["-lm"]
    runtime_dependencies = []
    if windows:
        libs += ["-lws2_32", "-liphlpapi", "-lgdi32", "-luser32", "-lshell32", "-lole32", "-loleaut32", "-luuid", "-limm32", "-lwinmm", "-lavrt", "-ldwrite"]
    else:
        libs += ["-lX11", "-lXi", "-lXcursor", "-lGL", "-ldl", "-lpthread"]
    if windows and "XUI_ENABLE_WEBVIEW" in features and os.environ.get("XUI_ENABLE_WEBVIEW2") == "1":
        sdk = Path(os.environ.get("WEBVIEW2_SDK_DIR", ""))
        include = sdk / "build/native/include"
        loader = sdk / "build/native/x64/WebView2Loader.dll.lib"
        loader_runtime = sdk / "build/native/x64/WebView2Loader.dll"
        if not (include / "WebView2.h").is_file() or not loader.is_file() or not loader_runtime.is_file():
            raise RuntimeError("Set WEBVIEW2_SDK_DIR to an extracted WebView2 SDK")
        # GCC writes include paths into Make dependency files. Forward slashes
        # keep Windows separators distinct from Make's character escapes.
        flags += ["-DXUI_ENABLE_WEBVIEW2", "-isystem", include.as_posix()]
        if os.environ.get("XUI_WEBVIEW_TEST_EXPORTS") == "1":
            flags += ["-DXUI_WEBVIEW_TEST_EXPORTS"]
        libs += [loader.as_posix()]
        runtime_dependencies.append(loader_runtime)
    compiled = []
    reused = []
    link_objects = []
    relink = QualifiedRelink(ROOT, out, args.relink_from, args.relink_source, identity, flags,
                            args.profile, args.define, api_major, features, args.debug, digest, stamp) if args.relink_from else None

    def compile_object(source, compiler, options, shared=False):
        if relink:
            link_objects.append(relink.object(source, compiler, options, shared))
            reused.append(source)
            return
        object_dir = ROOT / "build/shared/obj" if shared else objects
        object_dir.mkdir(parents=True, exist_ok=True)
        obj = object_dir / (source.replace("/", "_").replace("\\", "_") + ".o")
        dep = obj.with_suffix(".d")
        record = obj.with_suffix(".json")
        command = [compiler, *options, "-MD", "-MF", str(dep), "-MT", str(obj), "-c", source, "-o", str(obj)]
        key = digest([identity, compiler, stamp(compiler), command])
        try:
            cache = json.loads(record.read_text())
            valid = obj.is_file() and cache["key"] == key and all(stamp(p) == s for p, s in cache["deps"].items())
        except (OSError, KeyError, ValueError):
            valid = False
        if valid:
            reused.append(source)
        else:
            print(f"[compile] {source}", flush=True)
            # A failed compile cannot leave an object that is accepted on the next run.
            if record.exists():
                record.unlink()
            run(command)
            record.write_text(json.dumps({"key": key, "deps": {p: stamp(p) for p in dependencies(dep)}}))
            compiled.append(source)
        link_objects.append(obj)

    for source in sources:
        compile_object(source, cc, flags)
    if "XGE_ENABLE_HARFBUZZ" in features:
        cxx = shutil.which(os.environ.get("CXX", "g++"))
        if not cxx:
            raise RuntimeError("The existing optional HarfBuzz dependency requires g++")
        hb_flags = ["-std=c++17", "-O2", "-fno-exceptions", "-fno-rtti", "-ffunction-sections", "-fdata-sections", "-DNDEBUG", "-DHB_NO_BUFFER_SERIALIZE", "-DHB_NO_BUFFER_VERIFY", "-DHB_NO_DRAW", "-DHB_NO_OT_FETCH", "-DHB_NO_MMAP", "-DHB_NO_COLOR"]
        if not windows:
            # Linux uses the already linked pthread backend for HarfBuzz mutexes.
            hb_flags += ["-DHAVE_PTHREAD=1"]
        compile_object("extlibs/harfbuzz/src/harfbuzz.cc", cxx, hb_flags + ([] if windows else ["-fPIC"]), shared=True)
    if windows:
        resource = objects / ("xgedbg_res.o" if args.debug else "xge_res.o")
        resource_record = resource.with_suffix(".json")
        windres = shutil.which("windres")
        if not windres:
            raise RuntimeError("windres not found")
        resource_key = digest([windres, stamp(windres), stamp("platform/windows/xge.rc"), stamp("res/xge/icons/xx_dll.ico")])
        if relink:
            resource = relink.resource(windres)
        elif not resource.exists() or not resource_record.exists() or resource_record.read_text() != resource_key:
            run([windres, "-O", "coff", "-i", "platform/windows/xge.rc", "-o", resource])
            resource_record.write_text(resource_key)
        link_objects.append(resource)
    host_imports = []
    if args.host_xrt:
        nm = shutil.which("nm")
        if not nm:
            raise RuntimeError("nm is required to inventory hosted XRT imports")
        undefined = run([nm, "-u", *link_objects], capture_output=True, text=True).stdout
        host_imports = host_xrt.required_symbols(undefined, host_metadata)
        host_xrt.write_slots(host_generated, host_imports)
        compile_object("src/xge/core/xge_host_xrt.c", cc, flags)
        sources.append("src/xge/core/xge_host_xrt.c")
    artifacts = image_names(api_major, args.host_xrt, args.debug, windows)
    library = out / artifacts["runtime_image"]
    unstripped = out / artifacts["unstripped_image"]
    import_library = out / artifacts["import_library"] if windows else None
    link_args = [cc, "-shared", "-o", str(library), *map(str, link_objects), *libs]
    if windows:
        link_args += [f"-Wl,--out-implib,{import_library}"]
    else:
        link_args += ["-Wl,--no-undefined", "-Wl,-soname," + artifacts["runtime_soname"]]
    link_key = digest([identity, link_args, {str(p): stamp(p) for p in link_objects}, {p: stamp(p) for p in libs if Path(p).is_file()}])
    link_record = out / "link.key"
    linked = not (library.is_file() and unstripped.is_file() and (not windows or import_library.is_file()) and link_record.is_file() and link_record.read_text() == link_key)
    if linked:
        print(f"[link] {library}", flush=True)
        if link_record.exists():
            link_record.unlink()
        run(link_args)
        if args.host_xrt:
            defined = run([nm, "-g", "--defined-only", library], capture_output=True, text=True).stdout
            if re.search(r"\b(?:__)?xrt[A-Z]\w*\s*$", defined, re.M):
                library.unlink()
                raise RuntimeError("Hosted XGE unexpectedly defines XRT implementation symbols")
        shutil.copy2(library, unstripped)
        strip = shutil.which("strip")
        if not strip:
            raise RuntimeError("strip not found")
        run([strip, "--strip-unneeded", library])
        link_record.write_text(link_key)
    packaged_runtime = []
    for dependency in runtime_dependencies:
        target = out / dependency.name
        expected = hashlib.sha256(dependency.read_bytes()).hexdigest()
        if dependency.resolve() != target.resolve() and (
                not target.is_file() or hashlib.sha256(target.read_bytes()).hexdigest() != expected):
            shutil.copy2(dependency, target)
        if not target.is_file() or hashlib.sha256(target.read_bytes()).hexdigest() != expected:
            raise RuntimeError(f"Runtime dependency could not be packaged: {dependency.name}")
        packaged_runtime.append({"source": str(dependency.resolve()), "path": str(target.resolve()),
                                 "sha256": expected, "bytes": target.stat().st_size})
    qualification = relink.finish() if relink else None
    report = {"api_major": api_major, "public_abi_schema": 2, "profile": args.profile, "defines": args.define, "compiler": identity, "flags": flags,
              "source_root": str(ROOT), **artifacts, "qualified_object_build": qualification,
              "xrt_mode": "host" if args.host_xrt else "embedded", "xrt_host_imports": host_imports,
              "xrt_host_header_sha256": host_metadata["headerSha256"] if host_metadata else None,
              "features": sorted(features), "sources": sources, "compiled": compiled, "reused": reused,
              "linked": linked, "runtime_dependencies": packaged_runtime, "seconds": time.perf_counter() - started,
              "runtime_bytes": library.stat().st_size, "unstripped_bytes": unstripped.stat().st_size}
    report_name = "build-report-debug.json" if args.debug else "build-report.json"
    (out / report_name).write_text(json.dumps(report, indent=2) + "\n")
    mode = f"{args.profile} debug" if args.debug else args.profile
    print(f"[XGE {mode}] {len(compiled)} compiled, {len(reused)} reused, {report['seconds']:.2f}s, {report['runtime_bytes']} runtime bytes", flush=True)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as exc:
        if exc.stderr:
            print(exc.stderr, file=sys.stderr)
        sys.exit(exc.returncode or 1)
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        sys.exit(1)
