"""Assemble and verify a Windows x64 portable candidate; never overwrite a target.

Python is a developer tool only. The delivered program has no Python dependency.
No input providers are executed or changed by assembly.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import struct
import uuid
import zipfile
from sdk_release_materials import collect as collect_sdk_materials
from font_resources import verify as verify_font_resources
from catalog_sources import verify as verify_catalog_sources
from compiler_release_materials import collect as collect_compiler_materials, REBUILD_README

FORMAT = "xlang-ide-portable-1"
MAX_FILES = 10000
MAX_FILE = 256 * 1024 * 1024
MAX_TOTAL = 1024 * 1024 * 1024
CORE = ("xlang_ide.exe", "xlang-ls.exe", "xge.dll",
        "xl.exe", "xl.dll", "xlc.dll")
SYSTEM_DLLS = set("advapi32.dll bcrypt.dll comdlg32.dll dbghelp.dll gdi32.dll imm32.dll iphlpapi.dll kernel32.dll msvcrt.dll ntdll.dll ole32.dll oleaut32.dll shell32.dll user32.dll version.dll winhttp.dll winmm.dll ws2_32.dll wsock32.dll".split())
RESERVED = {"con", "prn", "aux", "nul", *(f"com{i}" for i in range(10)), *(f"lpt{i}" for i in range(10))}

class PackageError(ValueError):
    pass

def require(ok, message):
    if not ok:
        raise PackageError(message)

def sha(path):
    value = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()

def load_json(path, limit=16 * 1024 * 1024):
    require(Path(path).stat().st_size <= limit, f"Oversize JSON: {path}")
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))

def write_json(path, value):
    Path(path).write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

def safe_name(name):
    require(isinstance(name, str) and 0 < len(name) <= 512, "Invalid package path length")
    require("\\" not in name and not name.startswith("/") and "\0" not in name, f"Invalid path: {name!r}")
    parts = name.split("/")
    for part in parts:
        require(part not in ("", ".", "..") and part[-1:] not in (".", " "), f"Invalid path: {name!r}")
        require(not any(ord(c) < 32 or c in ':<>"|?*' for c in part), f"Invalid Windows path: {name!r}")
        require(part.split(".")[0].casefold() not in RESERVED, f"Reserved Windows path: {name!r}")
    return name

def check_hierarchy(names):
    keys = {name.casefold() for name in names}
    require(len(keys) == len(names), "Case-colliding paths")
    for name in keys:
        parts = name.split("/")
        require(not any("/".join(parts[:i]) in keys for i in range(1, len(parts))), "File/directory path conflict")

def plain(path, directory=False):
    data = Path(path).lstat()
    require(not stat.S_ISLNK(data.st_mode) and not getattr(data, "st_file_attributes", 0) & 0x400,
            f"Links/reparse points are not portable inputs: {path}")
    require(stat.S_ISDIR(data.st_mode) if directory else stat.S_ISREG(data.st_mode), f"Wrong file type: {path}")
    return data

def tree(root):
    root = Path(root)
    plain(root, True)
    files = []
    def scan(directory):
        for item in sorted(directory.iterdir(), key=lambda p: p.name.casefold()):
            attrs = item.lstat()
            require(not stat.S_ISLNK(attrs.st_mode) and not getattr(attrs, "st_file_attributes", 0) & 0x400,
                    f"Links/reparse points are not portable inputs: {item}")
            if stat.S_ISDIR(attrs.st_mode):
                scan(item)
            else:
                plain(item)
                safe_name(item.relative_to(root).as_posix())
                files.append(item)
                require(len(files) <= MAX_FILES, "Too many input files")
    scan(root)
    return files

def expected_inputs(manifest, root):
    record = load_json(manifest)
    root = Path(root).resolve()
    if "stageRoot" in record:
        base = Path(record["stageRoot"]).resolve()
        expected = {p["path"]: p["sha256"] for p in record["files"]}
    else:
        base = Path(record["directory"]).resolve()
        expected = record["files"]
    require(root.is_relative_to(base), "Toolchain root does not belong to its verification manifest")
    prefix = root.relative_to(base).as_posix()
    prefix = "" if prefix == "." else prefix + "/"
    selected = {name[len(prefix):]: value for name, value in expected.items() if name.startswith(prefix)}
    require(selected, "Verification manifest has no files for the selected toolchain")
    return record, base, expected, selected

def pe_imports(path):
    """Bounded PE32+ import table read. Check only declared core executables/DLLs."""
    data = Path(path).read_bytes()
    def u16(at):
        require(0 <= at <= len(data) - 2, f"Truncated PE: {path}")
        return struct.unpack_from("<H", data, at)[0]
    def u32(at):
        require(0 <= at <= len(data) - 4, f"Truncated PE: {path}")
        return struct.unpack_from("<I", data, at)[0]
    require(data[:2] == b"MZ", f"Not PE: {path}")
    head = u32(0x3c)
    require(data[head:head+4] == b"PE\0\0" and u16(head+4) == 0x8664, f"Not Windows x64 PE: {path}")
    count, optional_size = u16(head+6), u16(head+20)
    optional = head + 24
    require(1 <= count <= 96 and optional_size >= 128 and u16(optional) == 0x20b, f"Invalid PE32+ headers: {path}")
    imports, imports_size = u32(optional+120), u32(optional+124)
    headers = u32(optional+60)
    table = optional + optional_size
    def offset(rva):
        if rva < headers:
            require(rva < len(data), "Invalid header RVA")
            return rva
        for section in range(count):
            start = table + section * 40
            virtual, address, length, raw = u32(start+8), u32(start+12), u32(start+16), u32(start+20)
            if address <= rva < address + max(virtual, length):
                delta = rva - address
                require(delta < length and raw + delta < len(data), "Invalid PE section RVA")
                return raw + delta
        raise PackageError(f"Unmapped PE RVA in {path}")
    require(imports and 20 <= imports_size <= 1024 * 1024, f"Missing/invalid PE imports: {path}")
    result = []
    for i in range(min(1024, imports_size // 20)):
        descriptor = offset(imports+i*20)
        require(descriptor+20 <= len(data), "Truncated import descriptor")
        if not any(data[descriptor:descriptor+20]):
            return sorted(set(result))
        start = offset(u32(descriptor+12))
        end = data.find(b"\0", start, min(len(data), start+256))
        require(end > start, "Invalid import name")
        name = data[start:end].decode("ascii").lower()
        require("/" not in name and "\\" not in name and ":" not in name and name.endswith(".dll"), "Invalid imported DLL name")
        result.append(name)
    raise PackageError(f"Unterminated PE imports: {path}")

def check_core(root, with_c_lsp=False, with_console=False):
    inventory = {}
    for name in (*CORE, *(("xlang_ide_console.exe",) if with_console else ()), *(("toolchains/clangd/bin/clangd.exe",) if with_c_lsp else ())):
        path = Path(root) / name
        plain(path)
        imports = pe_imports(path)
        for dependency in imports:
            if dependency in SYSTEM_DLLS:
                continue
            nearby = path.parent / dependency
            shared = Path(root) / dependency
            require(nearby.is_file() or shared.is_file(), f"Missing local core dependency {dependency}: {name}")
        inventory[name] = {"machine": "AMD64", "imports": imports}
    return inventory

def check_features(manifest, paths):
    enabled = manifest.get("features", {}).get("cLsp", "clangd" in manifest.get("toolchains", {}))
    require(type(enabled) is bool, "Invalid C LSP feature flag")
    if enabled:
        require("toolchains/clangd/bin/clangd.exe" in paths, "C LSP runtime missing")
    else:
        require("clangd" not in manifest.get("toolchains", {}), "Disabled C LSP has a server toolchain")
        require(not any(p.casefold().startswith(("toolchains/clangd/", "sources/clangd-adaptation/", "notices/clangd/", "provenance/clangd-adaptation/")) or p.casefold()=="provenance/clangd-inputs.json" for p in paths), "Disabled C LSP has server assets")
    return enabled

def check_development_layout(manifest, paths):
    descriptor = manifest.get("developmentMaterials")
    if descriptor is None:  # Historical candidates kept their materials inside.
        return
    require(isinstance(descriptor, dict) and descriptor.get("directory") == "../development-materials" and
            descriptor.get("manifest") == "materials-manifest.json", "Invalid development materials location")
    digest = descriptor.get("sha256")
    require(isinstance(digest, str) and len(digest) == 64 and all(c in '0123456789abcdef' for c in digest), "Invalid development materials digest")
    require(not any(p.casefold().startswith(("sources/", "provenance/")) for p in paths), "Development materials remain in runtime payload")
    require("xlang_ide_console.exe" not in {p.casefold() for p in paths}, "Console frontend remains in runtime payload")

def verify_development_materials(payload, manifest=None):
    """Check the sibling developer bundle; runtime extraction never needs it."""
    payload = Path(payload)
    manifest = manifest if manifest is not None else verify_payload(payload)
    if "developmentMaterials" not in manifest:
        return payload
    check_development_layout(manifest, {row["path"] for row in manifest["files"]})
    plain(payload.parent, True)
    root = payload.parent / "development-materials"
    plain(root, True)
    index = root / "materials-manifest.json"
    plain(index)
    require(sha(index) == manifest["developmentMaterials"]["sha256"], "Development materials manifest mismatch")
    data = load_json(index)
    require(data.get("format") == "xlang-ide-development-materials-1", "Invalid development materials manifest")
    rows = data.get("files")
    require(isinstance(rows, list) and 0 < len(rows) < MAX_FILES, "Invalid development materials inventory")
    expected, keys, total = {"materials-manifest.json"}, {"materials-manifest.json"}, 0
    for row in rows:
        name = safe_name(row["path"])
        require(name.startswith(("sources/", "provenance/")) and name.casefold() not in keys, "Invalid or duplicate development material path")
        keys.add(name.casefold()); expected.add(name)
        require(type(row["size"]) is int and 0 <= row["size"] <= MAX_FILE, "Invalid development material size")
        total += row["size"]; require(total <= MAX_TOTAL, "Oversize development materials")
        plain(root / name)
        require((root / name).stat().st_size == row["size"] and sha(root / name) == row["sha256"], "Development material mismatch: " + name)
    check_hierarchy(expected)
    require({p.relative_to(root).as_posix() for p in tree(root)} == expected, "Unlisted development material files")
    require(data.get("runtimeArtifacts") == {name: sha(payload/name) for name in CORE}, "Development materials belong to a different runtime")
    return root

def verify_payload(root, allow_user_state=False):
    root = Path(root)
    manifest = load_json(root / "package-manifest.json")
    require(manifest.get("format") == FORMAT and manifest.get("platform") == "windows-x64", "Invalid package manifest")
    records = manifest.get("files")
    require(isinstance(records, list) and 0 < len(records) < MAX_FILES, "Invalid manifest file list")
    paths = {"package-manifest.json"}
    keys = {"package-manifest.json"}
    total = 0
    for item in records:
        name = safe_name(item["path"])
        require(name.casefold() not in keys, f"Duplicate manifest path: {name}")
        keys.add(name.casefold()); paths.add(name)
        require(isinstance(item["size"], int) and 0 <= item["size"] <= MAX_FILE, "Invalid manifest size")
        total += item["size"]
        require(total <= MAX_TOTAL, "Oversize payload")
        attrs = plain(root / name)
        mutable = allow_user_state and name.startswith("option/")
        require(mutable or (attrs.st_size == item["size"] and sha(root/name) == item["sha256"]), f"Payload mismatch: {name}")
    require(set(CORE).issubset(paths), "Core files missing from manifest")
    check_development_layout(manifest, paths)
    check_hierarchy(paths)
    actual = {p.relative_to(root).as_posix() for p in tree(root)}
    extras = actual - paths
    require(not extras or (allow_user_state and all(p.startswith("option/") for p in extras)), f"Unlisted payload files: {sorted(extras)[:5]}")
    require(check_core(root, check_features(manifest, paths), "xlang_ide_console.exe" in paths) == manifest["corePE"], "Core PE metadata mismatch")
    return manifest

def inspect_zip(archive):
    """Validate the entire directory and all bytes before writing any extraction."""
    with zipfile.ZipFile(archive) as bundle:
        entries = bundle.infolist()
        require(0 < len(entries) <= MAX_FILES, "Invalid ZIP entry count")
        names, keys, total = {}, set(), 0
        for item in entries:
            name = safe_name(item.filename)
            require(name.startswith("xlang-ide/") and len(name.split("/")) > 1 and not item.is_dir(), "Invalid ZIP root/entry type")
            key = name.casefold()
            require(key not in keys, f"Duplicate ZIP entry: {name}")
            keys.add(key); names[name[len("xlang-ide/"):]] = item
            mode = item.external_attr >> 16
            require(not mode or stat.S_ISREG(mode), "ZIP links/devices are forbidden")
            require(not item.flag_bits & 1, "Encrypted ZIP is unsupported")
            require(0 <= item.file_size <= MAX_FILE, "Oversize ZIP file")
            total += item.file_size
            require(total <= MAX_TOTAL, "Oversize ZIP payload")
        check_hierarchy(list(names))
        require(set(CORE).issubset(names), "Core files missing from ZIP")
        require("package-manifest.json" in names, "ZIP manifest is missing")
        require(names["package-manifest.json"].file_size <= 16*1024*1024, "Oversize ZIP manifest")
        manifest = json.loads(bundle.read(names["package-manifest.json"]))
        require(manifest.get("format") == FORMAT and manifest.get("platform") == "windows-x64", "Invalid ZIP manifest")
        expected, manifest_keys = {"package-manifest.json"}, {"package-manifest.json"}
        for record in manifest["files"]:
            name = safe_name(record["path"])
            require(name.casefold() not in manifest_keys, "Duplicate ZIP manifest record")
            manifest_keys.add(name.casefold()); expected.add(name)
            require(name in names and names[name].file_size == record["size"], f"ZIP inventory mismatch: {name}")
            digest = hashlib.sha256()
            with bundle.open(names[name]) as stream:
                for block in iter(lambda: stream.read(1024*1024), b""):
                    digest.update(block)
            require(digest.hexdigest() == record["sha256"], f"ZIP content mismatch: {name}")
        require(set(names) == expected, "ZIP contains unlisted files")
        check_features(manifest, expected)
        check_development_layout(manifest, expected)
    return manifest

def extract_verified(archive, destination):
    destination = Path(destination)
    require(not destination.exists(), f"Extraction destination already exists: {destination}")
    for parent in destination.absolute().parents:
        if parent.exists():
            plain(parent, True)
    archive_hash = sha(archive)
    inspect_zip(archive)
    destination.mkdir(parents=True)
    with zipfile.ZipFile(archive) as bundle:
        for item in bundle.infolist():
            target = destination / item.filename
            require(target.resolve().is_relative_to(destination.resolve()), "Extraction target escaped root")
            target.parent.mkdir(parents=True, exist_ok=True)
            with bundle.open(item) as source, target.open("xb") as output:
                shutil.copyfileobj(source, output, 1024*1024)
    require(sha(archive) == archive_hash, "Archive changed during extraction")
    verify_payload(destination / "xlang-ide")
    return destination / "xlang-ide"

def current_build_release(ide, report_path):
    """Accept a successful source snapshot belonging to this checkout only."""
    path = Path(report_path).absolute()
    plain(path)
    report = load_json(path)
    require(report.get('status') == 'current-source-build-passed' and report.get('exitCode') == 0,
            'Current source build has not succeeded; run build_current.bat first')
    require(Path(report.get('sourceRoot', '')).resolve() == ide.resolve(), 'Build report belongs to another checkout')
    root = Path(report.get('root', '')).absolute()
    require(root.resolve().is_relative_to((ide/'build').resolve()) and root.resolve() != (ide/'build').resolve(),
            'Current source build must be within ide/build')
    require(path == root/'current-build-report.json', 'Unexpected current build report location')
    for parent in (root, *root.parents):
        plain(parent, True)
        if parent == ide:
            break
    inputs = report.get('inputs', [])
    require(inputs and isinstance(inputs, list), 'Current build has no source inventory')
    seen = set()
    for row in inputs:
        relative = safe_name(row['path'])
        require(relative.casefold() not in seen, 'Duplicate current source input')
        seen.add(relative.casefold())
        for source in (ide/relative, root/relative):
            plain(source)
            require(sha(source) == row['sha256'], 'Current source changed since build: ' + relative)
    require(report.get('sdkManifestSha256') == sha(ide/'third_party/xge_sdk/sdk_manifest.json'),
            'Current SDK differs from the built SDK')
    outputs = report.get('outputs', {})
    for name in CORE[:3]:
        relative = 'release/' + name
        require(relative in outputs, 'Current build output receipt is incomplete: ' + name)
        plain(root/relative)
        require(sha(root/relative) == outputs[relative], 'Current build output changed: ' + name)
    return root/'release', report, sha(path)


def assemble(args):
    ide = Path(__file__).resolve().parents[1]
    release, sdk = ide.parent / "release", ide / "third_party/xge_sdk"
    build_report = getattr(args, 'build_report', None)
    current_build = None
    if build_report:
        release, current_build, current_build_hash = current_build_release(ide, build_report)
    with_c_lsp = bool(getattr(args, "with_c_lsp", False))
    compiler = Path(args.compiler_root).absolute(); plain(compiler, True)
    server = None
    if with_c_lsp:
        require(getattr(args, "c_server_root", None) and getattr(args, "c_server_manifest", None), "Optional C LSP needs an explicit server and manifest")
        server = Path(args.c_server_root).absolute(); plain(server, True)
    dist = ide / "dist"
    if dist.exists():
        plain(dist, True)
    destination = Path(args.output).absolute() if args.output else dist / ("portable-" + datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:8])
    require(destination.resolve().is_relative_to(dist.resolve()) and destination.resolve() != dist.resolve(), "Output must be a new directory within ide/dist")
    require(not destination.exists(), f"Refusing to overwrite existing candidate: {destination}")
    for parent in destination.parents:
        if parent == ide:
            break
        if parent.exists():
            plain(parent, True)
    compiler_info, compiler_base, compiler_expected, expected_c = expected_inputs(args.compiler_manifest, compiler)
    if with_c_lsp:
        server_info, server_base, _, expected_s = expected_inputs(args.c_server_manifest, server)
    source_plan = []
    destinations = set()
    def add(source, target, digest=None):
        source = Path(source)
        attrs = plain(source)
        name = safe_name(target)
        require(name.casefold() not in destinations, f"Colliding input destinations: {name}")
        require(attrs.st_size <= MAX_FILE, f"Oversize input: {source}")
        actual = sha(source)
        require(not digest or actual == digest, f"Input differs from frozen provenance: {source}")
        destinations.add(name.casefold())
        source_plan.append((source, name, actual, attrs.st_size))
    sdk_info = load_json(sdk / "sdk_manifest.json")
    sdk_dll_hash = next(p["sha256"] for p in sdk_info["files"] if p["path"] == "xge.dll")
    for name in CORE[:3]:
        add(release/name, name, sdk_dll_hash if name == "xge.dll" else None)
    add(ide / "data/commands.json", "data/commands.json")
    require(sha(release / "data/commands.json") == sha(ide / "data/commands.json"), "Release command catalog is stale")
    catalog_sources = verify_catalog_sources(ide / 'data/catalog-sources', ide / 'data/commands.json')
    require(verify_catalog_sources(release / 'data/catalog-sources', release / 'data/commands.json') == catalog_sources, 'Release catalog sources are stale')
    for name, digest in catalog_sources['files'].items():
        add(ide / 'data/catalog-sources' / name, 'data/catalog-sources/' + name, digest)
    font_materials = verify_font_resources(ide / "data/fonts")
    require(verify_font_resources(release / "data/fonts") == font_materials, "Release offline fonts are stale")
    for row in font_materials["files"]:
        add(ide / row["path"], row["path"], row["sha256"])
    compiler_files = [compiler/name for name in ("xl.exe", "xl.dll", "xlc.dll")]
    for directory in ("include", "extlibs"):
        compiler_files += tree(compiler/directory)
    for source in compiler_files:
        rel = source.relative_to(compiler).as_posix()
        require(rel in expected_c, f"Compiler runtime file is not in frozen manifest: {rel}")
        add(source, rel, expected_c[rel])
    # Files removed from selected runtime trees must be detected as well as added ones.
    selected_c = {p.relative_to(compiler).as_posix() for p in compiler_files}
    frozen_c = {p for p in expected_c if p in ("xl.exe", "xl.dll", "xlc.dll") or p.startswith(("include/", "extlibs/"))}
    require(selected_c == frozen_c, "Frozen compiler runtime inventory differs")
    if with_c_lsp:
        server_files = [server/"LICENSE.TXT", *tree(server/"bin"), *tree(server/"lib")]
        selected_s = {p.relative_to(server).as_posix() for p in server_files}
        require(selected_s == set(expected_s), "Frozen clangd runtime inventory differs")
        for source in server_files:
            rel = source.relative_to(server).as_posix()
            add(source, "toolchains/clangd/"+rel, expected_s[rel])
    add(sdk/"sdk_manifest.json", "provenance/sdk_manifest.json")
    add(sdk/"xge-source-files.json", "provenance/xge-source-files.json")
    # Preserve the matched SDK's own source-build and patch provenance.
    require(sdk_info.get("privateCandidate") is True, "Expected private integrated SDK")
    sdk_files = {row["path"]: row["sha256"] for row in sdk_info["files"]}
    for name in ("provenance/source-build-report.json", "provenance/source-build-manifest.json", "provenance/parent-sdk-manifest.json"):
        add(sdk/name, name, sdk_files[name])
    for name in sdk_info['provenance'].get('compatibilityPatchFiles', []):
        require(safe_name(name).startswith('provenance/compatibility-patches/'), 'Invalid SDK patch material path')
        add(sdk/name, name, sdk_files[name])
    if current_build is not None:
        add(build_report, 'provenance/ide-source-build-report.json', current_build_hash)

    add(args.compiler_manifest, "provenance/compiler-inputs.json")
    if with_c_lsp:
        add(args.c_server_manifest, "provenance/clangd-inputs.json")
    notices = []
    notices.append({"path": "data/fonts/LICENSE", "copyright": "data/fonts/COPYRIGHT.txt",
                    "provenance": "data/fonts/source.json", "status": "pinned-unmodified-OFL-font"})
    notices.append({"path": "data/fonts/Consolas-with-Yahei_Nerd_Font.ttf",
                    "provenance": "data/fonts/source.json", "status": "user-supplied-font-license-review-required"})
    copying = compiler_base / "lib/tcc/COPYING"
    require("lib/tcc/COPYING" in compiler_expected, "Frozen TCC license material is absent")
    add(copying, "notices/tcc/COPYING", compiler_expected["lib/tcc/COPYING"])
    notices.append({"path": "notices/tcc/COPYING", "status": "frozen-source-material"})
    sdk_materials = collect_sdk_materials(getattr(args, "sdk_source_root", None), ide/"data/xge-sdk-materials.json", sdk,
        add=add, load_json=load_json, plain=plain, safe_name=safe_name, sha=sha, require=require)
    notices += sdk_materials["materials"]
    # Preserve exact frozen TCC source files. A source copy alone does not prove
    # the static-link/relink obligations for the actual compiler artifacts.
    tcc_sources = sorted(name for name in compiler_expected if name.startswith("lib/tcc/"))
    for name in tcc_sources:
        add(compiler_base/name, "sources/tcc/"+name[len("lib/tcc/"):], compiler_expected[name])
    tcc_materials = {"status": "frozen-source-files-copied-relink-review-pending", "sourceFiles": len(tcc_sources),
                     "sourceDirectory": "sources/tcc/", "relinkReviewComplete": False}
    compiler_sources = collect_compiler_materials(getattr(args, "compiler_source_manifest", None),
        getattr(args, "compiler_source_build_report", None), compiler_base, expected_c,
        add=add, load_json=load_json, plain=plain, safe_name=safe_name, sha=sha, require=require, max_files=MAX_FILES)
    c_server_materials = {"enabled": False, "inputTraceProtocol": 0}
    if with_c_lsp:
        from c_server_materials import collect_c_server_materials
        c_server_materials = collect_c_server_materials(server_info, server_base, expected_s,
            add=add, load_json=load_json, plain=plain, safe_name=safe_name, sha=sha, require=require)
    require(len(source_plan) < MAX_FILES and sum(p[3] for p in source_plan) <= MAX_TOTAL, "Package input budget exceeded")
    destination.parent.mkdir(parents=True, exist_ok=True)
    stage = destination.parent / (".portable-stage-" + uuid.uuid4().hex)
    stage.mkdir()
    payload = stage/"xlang-ide"; payload.mkdir()
    development = stage/"development-materials"; development.mkdir()
    for source, name, digest, size in source_plan:
        root = development if name.startswith(("sources/", "provenance/")) else payload
        target = root/name; target.parent.mkdir(parents=True, exist_ok=True)
        with source.open("rb") as input_file, target.open("xb") as output:
            shutil.copyfileobj(input_file, output, 1024*1024)
        require(target.stat().st_size == size and sha(target) == digest and sha(source) == digest, f"Input changed while copying: {source}")
    if compiler_sources["copiedFiles"]:
        write_json(development/"sources/xlang/SOURCE-MATERIALS.json", compiler_sources)
        (development/"sources/COMPILER-REBUILD.md").write_text(REBUILD_README, encoding="utf8")
    material_files = [{"path": p.relative_to(development).as_posix(), "size": p.stat().st_size, "sha256": sha(p)} for p in tree(development)]
    write_json(development/"materials-manifest.json", {"format": "xlang-ide-development-materials-1", "files": material_files,
        "runtimeArtifacts": {name: sha(payload/name) for name in CORE}})
    development_descriptor = {"directory": "../development-materials", "manifest": "materials-manifest.json", "sha256": sha(development/"materials-manifest.json")}
    settings = {"format": "xlang-ide-settings-1", "toolchain": {"compiler": "../xl.exe"}}
    if with_c_lsp:
        settings["toolchain"]["cServer"] = "../toolchains/clangd/bin/clangd.exe"
    (payload/"option").mkdir()
    write_json(payload/"option/settings.json", settings)
    write_json(payload/"notices/material-index.json", {
        "materials": notices, "status": "release-candidate-materials-incomplete",
        "developmentMaterials": development_descriptor,
        "sourceAndProvenancePathsRelativeTo": "developmentMaterials",
        "cServerSources": c_server_materials, "sdkMaterials": sdk_materials, "tccMaterials": tcc_materials, "compilerSources": compiler_sources,
        "extensionMaterials": [p for p in sorted(selected_c) if Path(p).name.lower().startswith(("license", "copying"))],
        "remaining": ([] if sdk_materials["sourceFiles"] else ["Supply the exact pinned SDK source snapshot"]) + (["Supply corresponding frozen compiler source and build materials"] if not compiler_sources["copiedFiles"] else []) + ["SDK complete distribution terms review, including XGE and xlayout", "TCC corresponding source/relink obligations review", "Extension assets distribution review", "IDE and xlang project distribution terms confirmation"],
        "note": "Files are provenance and inspection materials; candidate assembly does not establish formal release readiness."})
    quickstart = """# xlang IDE Windows x64 便携候选\n\n解压到可写目录，运行 xlang_ide.exe。默认布局：左侧命令列表，中间编辑器及其下方帮助/输出/终端，右侧工作空间与项目属性。可直接打开 .c、.xl 源文件、.xlp 项目或 .xlw 工作空间；无需先创建工作空间。工具栏明确显示执行目标与配置，构建、运行和测试共用该目标。没有项目时默认使用当前文件，有项目时默认使用启动项目，也可明确选择其他项目或当前文件；项目树选择不会改变执行目标。停止按钮只停止当前所选任务，停止全部任务通过独立入口执行。未命名源文件先另存为。单文件产物位于源目录的 .xlang-ide/single-file/<文件名>/program.exe，运行目录为源文件目录。编辑器支持 C 与 xlang。使用构建/运行菜单或 F5；在 C 导出调用处使用 F12 可定位 xlang 声明。命令帮助全文离线可用。\n\n包内 xlang 与 clangd 工具使用相对默认路径，不需要 Python、GCC 或仓库目录。重新选择包内工具后仍可迁移。外部工具、工作空间、字体和工程内的绝对路径由用户管理。option 保存用户设置/会话，复制整个目录可带走 IDE 设置；工程需要一并迁移。界面和代码默认使用 data/fonts/Consolas-with-Yahei_Nerd_Font.ttf，缺失或损坏时退回 Noto；两者的许可独立记录，默认 TTF 公开分发授权尚未核实。Windows 提供系统 DLL。\n\n此包是开发验收候选。查看 package-manifest.json 的载荷摘要与 provenance 的来源快照。命令目录记录自己的编译器生成来源；它与随包编译器是独立版本的静态文档快照。本候选 SDK 来自成套的 SDK62 严格构建，实际源码闭包及原 SDK 快照在 provenance 与 sources/xge 中保留。notices/material-index.json 列出已有材料及仍需核对的发布材料。提供源码参数时，sources/COMPILER-REBUILD.md 说明独立源码重建；开发构建工具不属于 IDE 运行依赖。完整复杂工程、实体输入/混合 DPI、所有干净设备和正式发布验收尚未完成。\n"""
    sdk_description = '本候选 SDK 为 XGE ' + sdk_info['version'] + '，按其来源记录中的配置独立构建'
    if sdk_info['provenance'].get('compatibilityPatchFiles'):
        sdk_description += '，并包含 provenance/compatibility-patches 中记录的 IDE 兼容补丁'
    quickstart = quickstart.replace('本候选 SDK 来自成套的 SDK62 严格构建', sdk_description)
    if not with_c_lsp:
        quickstart = quickstart.replace("在 C 导出调用处使用 F12 可定位 xlang 声明。", "xlang 由 xlang-ls 提供语言服务；C 保留语法编辑和编译运行，不启用 C LSP。")
        quickstart = quickstart.replace("包内 xlang 与 clangd 工具", "包内 xlang 工具")
    if c_server_materials['inputTraceProtocol'] == 1:
        quickstart += '\n默认 C 语言服务器带实际输入观察扩展：已读取的间接头文件、缺失探测和搜索候选变化会撤下旧语义结果，再重新分析。未列入项目的已观察头可用 F12 打开。可在偏好中选择普通 clangd；它继续使用显式输入守护。sources/clangd-adaptation 和 provenance/clangd-adaptation 保留扩展源码差异与构建记录，notices/clangd 保留运行库材料。本扩展不宣告 TCC/Clang 预处理或 ABI 全面等价。\n'
    quickstart = quickstart.replace("查看 package-manifest.json 的载荷摘要与 provenance 的来源快照。", "查看 package-manifest.json 的运行载荷摘要。源码和构建来源记录单独保存在候选旁的 development-materials，运行 ZIP 不包含 sources 或 provenance。")
    quickstart = quickstart.replace("在 provenance 与 sources/xge 中保留", "在独立开发资料的 provenance 与 sources/xge 中保留")
    quickstart = quickstart.replace("提供源码参数时，sources/COMPILER-REBUILD.md", "提供源码参数时，独立开发资料中的 sources/COMPILER-REBUILD.md")
    (payload/"QUICKSTART.md").write_text(quickstart, encoding="utf-8")
    files = []
    for path in tree(payload):
        files.append({"path": path.relative_to(payload).as_posix(), "size": path.stat().st_size, "sha256": sha(path)})
    catalog = load_json(payload/"data/commands.json", limit=64 * 1024 * 1024)
    manifest = {"format": FORMAT, "platform": "windows-x64", "status": "development-candidate",
        "createdUtc": datetime.now(timezone.utc).isoformat(), "files": files,
        "mutableState": {"directory": "option/", "initialSeedHashed": True, "userDataNotCopied": True},
        "corePE": check_core(payload, with_c_lsp), "coreDependencyScope": "Declared core binaries only; extension modules/assets preserved but not all exercised",
        "catalog": {"commands": len(catalog["entries"]), "documents": len(catalog["documents"]), "compilerSha256": catalog["compilerSha256"], "embeddedMarkdown": True},
        "toolchains": {"xlang": {"path": "xl.exe", "sha256": sha(payload/"xl.exe"), "provenance": "provenance/compiler-inputs.json"}},
        "features": {"cLsp": with_c_lsp},
        "developmentMaterials": development_descriptor,
        "sdk": sdk_info["provenance"], "fonts": font_materials, "releaseMaterials": "notices/material-index.json"}
    if with_c_lsp:
        manifest["toolchains"]["clangd"] = {"tag": server_info.get("tag"), "sha256": sha(payload/"toolchains/clangd/bin/clangd.exe"), "provenance": "provenance/clangd-inputs.json", "inputTraceProtocol": c_server_materials["inputTraceProtocol"]}
    write_json(payload/"package-manifest.json", manifest)
    verify_payload(payload)
    verify_development_materials(payload, manifest)
    archive = stage/"xlang-ide-windows-x64.zip"
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as bundle:
        for path in tree(payload):
            name = "xlang-ide/" + path.relative_to(payload).as_posix()
            info = zipfile.ZipInfo(name, (2026,1,1,0,0,0)); info.create_system = 3
            info.external_attr = (stat.S_IFREG | 0o644) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            with path.open("rb") as source, bundle.open(info, "w") as target:
                shutil.copyfileobj(source, target, 1024*1024)
    inspect_zip(archive)
    current_c = {p.relative_to(compiler).as_posix() for directory in ("include", "extlibs") for p in tree(compiler/directory)} | {"xl.exe", "xl.dll", "xlc.dll"}
    require(current_c == selected_c, "Compiler inventory changed before publication")
    if with_c_lsp:
        current_s = {p.relative_to(server).as_posix() for directory in ("bin", "lib") for p in tree(server/directory)} | {"LICENSE.TXT"}
        require(current_s == selected_s, "C server inventory changed before publication")
    for source, _, digest, _ in source_plan:
        require(sha(source) == digest, f"Input changed before candidate publication: {source}")
    if current_build is not None:
        _, _, final_build_hash = current_build_release(ide, build_report)
        require(final_build_hash == current_build_hash, 'Current build receipt changed during packaging')
    receipt = {"format": "xlang-ide-package-receipt-1", "destination": str(destination), "inputFiles": len(source_plan),
        "files": len(files)+1, "bytes": sum(item["size"] for item in files), "archiveSha256": sha(archive),
        "manifestSha256": sha(payload/"package-manifest.json"), "inputs": [{"path": str(p), "sha256": digest} for p, _, digest, _ in source_plan]}
    receipt["developmentMaterials"] = {**development_descriptor, "files": len(material_files)+1, "bytes": sum(p["size"] for p in material_files)}
    write_json(stage/"package-receipt.json", receipt)
    require(not destination.exists(), "Candidate destination appeared during assembly")
    stage.rename(destination)
    print(json.dumps({key: value for key, value in receipt.items() if key != "inputs"}, ensure_ascii=False))
    return destination

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler-root", required=True)
    parser.add_argument("--with-c-lsp", action="store_true", help="Explicit legacy C LSP package;default packages omit it")
    parser.add_argument("--c-server-root")
    parser.add_argument("--compiler-manifest", required=True)
    parser.add_argument("--c-server-manifest")
    parser.add_argument("--sdk-source-root", help="Exact source snapshot matching the pinned SDK; current repository sources are not substituted")
    parser.add_argument("--compiler-source-manifest", help="Explicit frozen compiler sources and build input closure")
    parser.add_argument("--compiler-source-build-report", help="Matching actual independent source build evidence")
    parser.add_argument("--build-report", help="Successful build_current snapshot report; current source and output hashes must still match")
    parser.add_argument("--output")
    args = parser.parse_args()
    try:
        assemble(args)
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        parser.exit(1, f"Portable assembly failed: {error}\n")

if __name__ == "__main__":
    main()
