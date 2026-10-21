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
CORE = ("xlang_ide.exe", "xlang_ide_console.exe", "xlang-ls.exe", "xge.dll",
        "toolchains/xlang/xl.exe", "toolchains/xlang/xl.dll", "toolchains/xlang/xlc.dll",
        "toolchains/clangd/bin/clangd.exe")
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

def load_json(path):
    require(Path(path).stat().st_size <= 16 * 1024 * 1024, f"Oversize JSON: {path}")
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

def check_core(root):
    inventory = {}
    for name in CORE:
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
    check_hierarchy(paths)
    actual = {p.relative_to(root).as_posix() for p in tree(root)}
    extras = actual - paths
    require(not extras or (allow_user_state and all(p.startswith("option/") for p in extras)), f"Unlisted payload files: {sorted(extras)[:5]}")
    require(check_core(root) == manifest["corePE"], "Core PE metadata mismatch")
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
    for name in CORE[:4]:
        relative = 'release/' + name
        require(relative in outputs, 'Current build output receipt is incomplete: ' + name)
        plain(root/relative)
        require(sha(root/relative) == outputs[relative], 'Current build output changed: ' + name)
    return root/'release', report, sha(path)


def assemble(args):
    ide = Path(__file__).resolve().parents[1]
    release, sdk = ide / "release", ide / "third_party/xge_sdk"
    build_report = getattr(args, 'build_report', None)
    current_build = None
    if build_report:
        release, current_build, current_build_hash = current_build_release(ide, build_report)
    compiler, server = Path(args.compiler_root).absolute(), Path(args.c_server_root).absolute()
    plain(compiler, True); plain(server, True)
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
    for name in CORE[:4]:
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
        add(source, "toolchains/xlang/"+rel, expected_c[rel])
    # Files removed from selected runtime trees must be detected as well as added ones.
    selected_c = {p.relative_to(compiler).as_posix() for p in compiler_files}
    frozen_c = {p for p in expected_c if p in ("xl.exe", "xl.dll", "xlc.dll") or p.startswith(("include/", "extlibs/"))}
    require(selected_c == frozen_c, "Frozen compiler runtime inventory differs")
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
    add(args.c_server_manifest, "provenance/clangd-inputs.json")
    notices = []
    notices.append({"path": "data/fonts/LICENSE", "copyright": "data/fonts/COPYRIGHT.txt",
                    "provenance": "data/fonts/source.json", "status": "pinned-unmodified-OFL-font"})
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
    from c_server_materials import collect_c_server_materials
    c_server_materials = collect_c_server_materials(server_info, server_base, expected_s,
        add=add, load_json=load_json, plain=plain, safe_name=safe_name, sha=sha, require=require)
    require(len(source_plan) < MAX_FILES and sum(p[3] for p in source_plan) <= MAX_TOTAL, "Package input budget exceeded")
    destination.parent.mkdir(parents=True, exist_ok=True)
    stage = destination.parent / (".portable-stage-" + uuid.uuid4().hex)
    stage.mkdir()
    payload = stage/"xlang-ide"; payload.mkdir()
    for source, name, digest, size in source_plan:
        target = payload/name; target.parent.mkdir(parents=True, exist_ok=True)
        with source.open("rb") as input_file, target.open("xb") as output:
            shutil.copyfileobj(input_file, output, 1024*1024)
        require(target.stat().st_size == size and sha(target) == digest and sha(source) == digest, f"Input changed while copying: {source}")
    if compiler_sources["copiedFiles"]:
        write_json(payload/"sources/xlang/SOURCE-MATERIALS.json", compiler_sources)
        (payload/"sources/COMPILER-REBUILD.md").write_text(REBUILD_README, encoding="utf8")
    settings = {"format": "xlang-ide-settings-1", "toolchain": {"compiler": "../toolchains/xlang/xl.exe", "cServer": "../toolchains/clangd/bin/clangd.exe"}}
    (payload/"option").mkdir()
    write_json(payload/"option/settings.json", settings)
    write_json(payload/"notices/material-index.json", {
        "materials": notices, "status": "release-candidate-materials-incomplete",
        "clangdMaterial": "toolchains/clangd/LICENSE.TXT", "cServerSources": c_server_materials, "sdkMaterials": sdk_materials, "tccMaterials": tcc_materials, "compilerSources": compiler_sources,
        "extensionMaterials": [p for p in sorted(selected_c) if Path(p).name.lower().startswith(("license", "copying"))],
        "remaining": ([] if sdk_materials["sourceFiles"] else ["Supply the exact pinned SDK source snapshot"]) + (["Supply corresponding frozen compiler source and build materials"] if not compiler_sources["copiedFiles"] else []) + ["SDK complete distribution terms review, including XGE and xlayout", "TCC corresponding source/relink obligations review", "Extension assets distribution review", "IDE and xlang project distribution terms confirmation"],
        "note": "Files are provenance and inspection materials; candidate assembly does not establish formal release readiness."})
    quickstart = """# xlang IDE Windows x64 便携候选\n\n解压到可写目录，运行 xlang_ide.exe。默认布局：左侧命令列表，中间编辑器及其下方帮助/输出/终端，右侧工作空间与项目属性。打开 .xlw 工作空间或 .xlp 项目；编辑器支持 C 与 xlang。使用构建/运行菜单或 F5；在 C 导出调用处使用 F12 可定位 xlang 声明。命令帮助全文离线可用。\n\n包内 xlang 与 clangd 工具使用相对默认路径，不需要 Python、GCC 或仓库目录。重新选择包内工具后仍可迁移。外部工具、工作空间、字体和工程内的绝对路径由用户管理。option 保存用户设置/会话，复制整个目录可带走 IDE 设置；工程需要一并迁移。缺少微软雅黑或 Consolas 时使用 data/fonts 中的随包中文等宽字体；Windows 提供系统 DLL。\n\n此包是开发验收候选。查看 package-manifest.json 的载荷摘要与 provenance 的来源快照。命令目录记录自己的编译器生成来源；它与随包编译器是独立版本的静态文档快照。本候选 SDK 来自成套的 SDK62 严格构建，实际源码闭包及原 SDK 快照在 provenance 与 sources/xge 中保留。notices/material-index.json 列出已有材料及仍需核对的发布材料。提供源码参数时，sources/COMPILER-REBUILD.md 说明独立源码重建；开发构建工具不属于 IDE 运行依赖。完整复杂工程、实体输入/混合 DPI、所有干净设备和正式发布验收尚未完成。\n"""
    sdk_description = '本候选 SDK 为 XGE ' + sdk_info['version'] + '，按其来源记录中的配置独立构建'
    if sdk_info['provenance'].get('compatibilityPatchFiles'):
        sdk_description += '，并包含 provenance/compatibility-patches 中记录的 IDE 兼容补丁'
    quickstart = quickstart.replace('本候选 SDK 来自成套的 SDK62 严格构建', sdk_description)
    if c_server_materials['inputTraceProtocol'] == 1:
        quickstart += '\n默认 C 语言服务器带实际输入观察扩展：已读取的间接头文件、缺失探测和搜索候选变化会撤下旧语义结果，再重新分析。未列入项目的已观察头可用 F12 打开。可在偏好中选择普通 clangd；它继续使用显式输入守护。sources/clangd-adaptation 和 provenance/clangd-adaptation 保留扩展源码差异与构建记录，notices/clangd 保留运行库材料。本扩展不宣告 TCC/Clang 预处理或 ABI 全面等价。\n'
    (payload/"QUICKSTART.md").write_text(quickstart, encoding="utf-8")
    files = []
    for path in tree(payload):
        files.append({"path": path.relative_to(payload).as_posix(), "size": path.stat().st_size, "sha256": sha(path)})
    catalog = load_json(payload/"data/commands.json")
    manifest = {"format": FORMAT, "platform": "windows-x64", "status": "development-candidate",
        "createdUtc": datetime.now(timezone.utc).isoformat(), "files": files,
        "mutableState": {"directory": "option/", "initialSeedHashed": True, "userDataNotCopied": True},
        "corePE": check_core(payload), "coreDependencyScope": "Declared core binaries only; extension modules/assets preserved but not all exercised",
        "catalog": {"commands": len(catalog["entries"]), "documents": len(catalog["documents"]), "compilerSha256": catalog["compilerSha256"], "embeddedMarkdown": True},
        "toolchains": {"xlang": {"sha256": sha(payload/"toolchains/xlang/xl.exe"), "provenance": "provenance/compiler-inputs.json"},
            "clangd": {"tag": server_info.get("tag"), "sha256": sha(payload/"toolchains/clangd/bin/clangd.exe"), "provenance": "provenance/clangd-inputs.json", "inputTraceProtocol": c_server_materials['inputTraceProtocol']}},
        "sdk": sdk_info["provenance"], "fonts": font_materials, "releaseMaterials": "notices/material-index.json"}
    write_json(payload/"package-manifest.json", manifest)
    verify_payload(payload)
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
    current_s = {p.relative_to(server).as_posix() for directory in ("bin", "lib") for p in tree(server/directory)} | {"LICENSE.TXT"}
    require(current_c == selected_c and current_s == selected_s, "Toolchain inventory changed before publication")
    for source, _, digest, _ in source_plan:
        require(sha(source) == digest, f"Input changed before candidate publication: {source}")
    if current_build is not None:
        _, _, final_build_hash = current_build_release(ide, build_report)
        require(final_build_hash == current_build_hash, 'Current build receipt changed during packaging')
    receipt = {"format": "xlang-ide-package-receipt-1", "destination": str(destination), "inputFiles": len(source_plan),
        "files": len(files)+1, "bytes": sum(item["size"] for item in files), "archiveSha256": sha(archive),
        "manifestSha256": sha(payload/"package-manifest.json"), "inputs": [{"path": str(p), "sha256": digest} for p, _, digest, _ in source_plan]}
    write_json(stage/"package-receipt.json", receipt)
    require(not destination.exists(), "Candidate destination appeared during assembly")
    stage.rename(destination)
    print(json.dumps({key: value for key, value in receipt.items() if key != "inputs"}, ensure_ascii=False))
    return destination

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler-root", required=True)
    parser.add_argument("--c-server-root", required=True)
    parser.add_argument("--compiler-manifest", required=True)
    parser.add_argument("--c-server-manifest", required=True)
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
