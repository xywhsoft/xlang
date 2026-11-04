"""Stage declarative XGE/XUI packages against one exact host-XRT engine build.

This copies no XRT implementation and does not modify release/. The engine
binary, headers, feature configuration and generated host contract must come
from a single build. A separate report pins the inputs and staged bytes.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
from gen_host_xrt_catalog import render_host_catalog


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--update-host-catalog',action='store_true',help='Regenerate the module-filtered resolver catalog only')
    for name in ("engine-root", "engine-build", "out", "report"):
        parser.add_argument("--" + name, type=Path)
    parser.add_argument("--bootstrap-only", action="store_true",
                        help="Stage only _xge_native while the public declarations are being generated")
    args = parser.parse_args()
    if args.update_host_catalog:
        target=args.root.resolve()/'src/runtime/xl6_host_xrt_symbols.h'
        target.write_text(render_host_catalog(args.root.resolve()),encoding='utf-8',newline='\n')
        print(str(target));return
    if any(getattr(args,name) is None for name in ('engine_root','engine_build','out','report')):
        parser.error('Staging requires --engine-root, --engine-build, --out and --report')
    root, native, build, out, report = [x.resolve() for x in
        (args.root, args.engine_root, args.engine_build, args.out, args.report)]
    if out.exists():
        raise SystemExit("The staging destination must not exist: " + str(out))
    if report.exists():
        raise SystemExit("The report destination must not exist: " + str(report))
    if report == out or out in report.parents:
        raise SystemExit("Keep the acceptance report outside the staged runtime packages")
    for target in (out, report):
        if target == root / "release" or root / "release" in target.parents:
            raise SystemExit("Stage in an isolated directory; release delivery is a separate operation")
    metadata = json.loads((build / "build-report.json").read_text(encoding="utf-8"))
    if metadata.get("xrt_mode") != "host" or not metadata.get("linked"):
        raise SystemExit("The engine must be a linked shared-host-XRT build")
    canonical_xrt = hashlib.sha256((root / "lib/xrt/xrt.h").read_bytes().replace(b"\r\n", b"\n")).hexdigest()
    if metadata.get("xrt_host_header_sha256") != canonical_xrt:
        raise SystemExit("The engine's XRT fingerprint does not match the language runtime")
    debug_flags=[flag for flag in metadata.get('flags',[]) if flag.startswith('-DXGE_DEBUGMODE=')]
    if len(debug_flags)!=1 or debug_flags[0] not in ('-DXGE_DEBUGMODE=0','-DXGE_DEBUGMODE=1'):
        raise SystemExit('The engine build does not identify its exact debug ABI input')
    debug=debug_flags[0][-1]=='1'
    windows_binary='xgedbg.dll' if debug else 'xge.dll'
    linux_binary='libxgedbg.so' if debug else 'libxge.so'
    platform = "win_x64" if (build / windows_binary).is_file() else "linux_x64"
    binary = build / (windows_binary if platform == "win_x64" else linux_binary)
    if not binary.is_file():
        raise SystemExit("The engine build has no supported native binary")
    headers = sorted((native / "include").glob("*.h"))
    if not {"xge.h", "xui.h", "xge_host_xrt.h"} <= {p.name for p in headers}:
        raise SystemExit("The engine source root has an incomplete public SDK")
    # Reproduce the engine builder's ABI witness. Merely matching XRT does
    # not prove that mutable XGE/XUI public headers match this binary.
    config=(build/'xge_build_config.h').read_text(encoding='utf-8')
    match=re.search(r'^#define XGE_BUILD_ABI_FINGERPRINT UINT64_C\(0x([0-9a-f]{16})\)$',config,re.M)
    if not match:
        raise SystemExit('The engine configuration has no supported ABI fingerprint')
    abi_files=[native/path for path in ('include/xge.h','include/xui.h','include/xge_host_xrt.h',
        'include/xge_config.h','include/xui_config.h','extlibs/xrt/xrt.h')]
    abi_inputs=config[:match.start()]+''.join(path.read_text(encoding='utf-8') for path in abi_files)
    actual_abi=hashlib.sha256((abi_inputs+metadata['compiler'][2]+debug_flags[0][-1]).encode()).hexdigest()[:16]
    if actual_abi!=match.group(1):
        raise SystemExit('The supplied native SDK headers do not match the engine ABI fingerprint')
    generated = [build / "host-xrt" / name for name in
        ("xge_host_xrt_contract.h", "xge_host_xrt_fingerprint.h", "xge_host_xrt_slots.inc")]
    slots = re.findall(r"^XGE_HOST_XRT_SLOT\((\w+)\)$", generated[2].read_text(encoding="utf-8"), re.M)
    if not slots or slots != sorted(set(slots)) or slots != metadata.get("xrt_host_imports"):
        raise SystemExit("The generated resolver slots do not match the engine import inventory")
    symbols = slots + ["xrtErrorBuild", "xrtSetErrorTake", "xrtFree"]
    catalog = root / "src/compile/xl6_artifact_native_symbols.inc"
    lookup = {}
    for name,module in re.findall(r'XL6_ARTIFACT_NATIVE_SYMBOL\("([^"]+)", (?:"([^"]+)"|NULL)\)',catalog.read_text(encoding="utf-8")):
        lookup.setdefault(name,[]).append(module)
    missing = sorted(set(symbols) - lookup.keys())
    if missing:
        raise SystemExit("Host XRT symbols are absent from the exact linkage catalog: " + ", ".join(missing))
    modules = sorted({module for s in symbols for module in lookup[s] if module})
    resolver_catalog=root/'src/runtime/xl6_host_xrt_symbols.h'
    if resolver_catalog.read_text(encoding='utf-8') != render_host_catalog(root):
        raise SystemExit('The host resolver catalog is stale; run --update-host-catalog')
    packages = ["_xge_native"] if args.bootstrap_only else ["_xge_native", "xge", "xui"]
    sources = root / "lib/bindings"
    if not args.bootstrap_only and 'XGE_ENABLE_3D' in metadata.get('features',[]):
        packages.append('xge3d')
    # A native compiler flag alone does not configure a later header consumer.
    # Only a matching first-include configuration can expose debug-only types.
    debug_config=re.search(r'^#define XGE_DEBUGMODE ([01])$',config,re.M)
    if debug and (not debug_config or debug_config.group(1)!='1'):
        raise SystemExit('The native debug engine SDK must declare XGE_DEBUGMODE=1 before the first public include')
    if debug_config and debug_config.group(1)!=debug_flags[0][-1]:
        raise SystemExit('The SDK debug configuration differs from the native compiler flags')
    if not args.bootstrap_only and debug_flags[0]=='-DXGE_DEBUGMODE=1' and debug_config and debug_config.group(1)=='1':
        packages.append('xgeDebug')
    package_sources={name:sources/({'xge3d':'xge/three_d','xgeDebug':'xge/debug'}.get(name,name)) for name in packages}
    xrt_config = native / "extlibs/xrt/xrt_config.h"
    inputs = {str(p): sha(p) for p in [Path(__file__).resolve(), Path(__file__).with_name('gen_host_xrt_catalog.py'), catalog, resolver_catalog, root / "lib/xrt/xrt.h", xrt_config,*abi_files,
                                      build / "build-report.json", binary,
                                      build / "xge_build_config.h", *headers, *generated]}
    for package in packages:
        package_source=package_sources[package]
        if not (package_source / "xlang.package.json").is_file():
            raise SystemExit("Package sources are not available: " + package)
        inputs.update({str(p):sha(p) for p in package_source.rglob('*') if p.is_file() and
                       not (package=='xge' and p.relative_to(package_source).parts[0] in ('three_d','debug'))})
    # No cleanup of an existing tree is performed, including after a failure.
    out.mkdir(parents=True)
    for package in packages:
        shutil.copytree(package_sources[package],out/package,
                        ignore=shutil.ignore_patterns('three_d','debug') if package=='xge' else None)
    target = out / "_xge_native"
    (target / "native").mkdir()
    for header in [*headers, *generated, build / "xge_build_config.h"]:
        shutil.copy2(header, target / "include" / header.name)
    # Preserve the upstream SDK's include spelling without shipping a second
    # full runtime header. This is a declaration-only forwarding include.
    xrt_include = target / "include/extlibs/xrt"
    xrt_include.mkdir(parents=True)
    shutil.copy2(xrt_config, xrt_include / xrt_config.name)
    (xrt_include / "xrt.h").write_text(
        "/* Shared host XRT declarations; no private implementation. */\n#include <xrt_decl.h>\n", encoding="utf-8")
    shutil.copy2(binary, target / "native" / binary.name)
    manifest_path = target / "xlang.package.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    # Both the TinyCC loader and GNU ld accept the actual DLL/.so. Using the
    # runtime image avoids an incompatible COFF static/import archive in TCC.
    link = "native/" + binary.name
    manifest["links"] = [link]
    manifest["loadlibs"] = [link]
    manifest["artifact"]["xrtModules"] = modules
    manifest["native"]["loadName"] = binary.name
    manifest["native"]["platform"] = platform
    manifest["native"]["sha256"] = sha(binary)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    if not all(sha(p) == value for p, value in inputs.items()):
        raise SystemExit("A staging input changed while it was being copied")
    output = {str(p.relative_to(out)): sha(p) for p in sorted(out.rglob("*")) if p.is_file()}
    result = dict(complete=True, platform=platform, packages=packages,
                  hostXrtSha256=canonical_xrt, hostSymbolCount=len(slots),
                  nativeAbiFingerprint=actual_abi,
                  xrtModules=modules, engineBuild=str(build / "build-report.json"),
                  inputs=inputs, outputs=output, inputsUnchanged=True,
                  testsRun=False, shippingDelivery=False)
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(dict(out=str(out), report=str(report), files=len(output),
                          hostSymbols=len(slots), xrtModules=len(modules))))


if __name__ == "__main__":
    main()
