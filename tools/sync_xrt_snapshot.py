#!/usr/bin/env python3

"""校验或同步 DEMO6 使用的 XRT 生成头快照。"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import shutil
import tempfile

from gen_xrt_symbol_table import _api_names, _render as render_symbol_table
from gen_xrt_xlang_matrix import check as check_capability_matrix
from gen_xrt_xlang_matrix import sync as sync_capability_matrix


SCHEMA = 4



def _digest(data: bytes) -> str:
    """返回稳定的大写 SHA-256 文本。"""

    return hashlib.sha256(data).hexdigest().upper()



def _artifact(source: str, data: bytes) -> dict[str, object]:
    """构造单个 XRT 生成物的稳定身份。"""

    return {
        "source": source,
        "sha256": _digest(data),
        "bytes": len(data),
    }



def _metadata(
    product: str,
    implementation_source: str,
    declaration_source: str,
    implementation: bytes,
    declarations: bytes,
    api_count: int,
    owned_api_count: int | None = None,
) -> dict[str, object]:
    """构造单个产品不包含机器路径和时间戳的可复现元数据。"""

    result: dict[str, object] = {
        "schema": SCHEMA,
        "product": product,
        "implementation": _artifact(implementation_source, implementation),
        "declarations": _artifact(declaration_source, declarations),
        "publicApiCount": api_count,
    }
    if owned_api_count is not None:
        result["ownedPublicApiCount"] = owned_api_count
    return result



def _write_if_changed(path: Path, data: bytes) -> bool:
    """仅在内容变化时原子替换文件。"""

    if path.is_file() and path.read_bytes() == data:
        return False
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_bytes(data)
    os.replace(temporary, path)
    return True



def _check_snapshot(root: Path) -> bool:
    """校验 XRT、xruntime 快照、元数据和运行时符号表。"""

    core_header = root / "lib" / "xrt" / "xrt.h"
    core_declarations = root / "release" / "include" / "xrt_decl.h"
    core_metadata_path = root / "lib" / "xrt" / "xrt.snapshot.json"
    runtime_header = root / "lib" / "xruntime" / "xruntime.h"
    runtime_declarations = root / "release" / "include" / "xruntime_decl.h"
    runtime_metadata_path = root / "lib" / "xruntime" / "xruntime.snapshot.json"
    runtime_features = root / "lib" / "xruntime" / "xruntime_features.h"
    symbols_path = root / "src" / "api" / "xl_xrt_symbols.inc"
    upstream_path = root / "lib" / "xrt" / "upstream.json"
    if not all(path.is_file() for path in (
        core_header,
        core_declarations,
        core_metadata_path,
        runtime_header,
        runtime_declarations,
        runtime_metadata_path,
        runtime_features,
        symbols_path,
        upstream_path,
    )):
        print("XRT/xruntime snapshot assets are incomplete", file=sys.stderr)
        return False

    core_implementation_data = core_header.read_bytes()
    core_declaration_data = core_declarations.read_bytes()
    runtime_implementation_data = runtime_header.read_bytes()
    runtime_declaration_data = runtime_declarations.read_bytes()
    try:
        actual_core = json.loads(core_metadata_path.read_text(encoding="utf-8"))
        actual_runtime = json.loads(runtime_metadata_path.read_text(encoding="utf-8"))
        core_names = set(_api_names(core_header))
        runtime_names = set(_api_names(runtime_header))
        if core_names & runtime_names:
            raise ValueError("XRT/xruntime public API ownership overlaps")
        expected_symbols = render_symbol_table(core_header, runtime_header)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"XRT/xruntime snapshot validation failed: {error}", file=sys.stderr)
        return False

    expected_core = _metadata(
        "xrt",
        "xrt/single/xrt.h",
        "xrt/single/xrt_decl.h",
        core_implementation_data,
        core_declaration_data,
        len(core_names),
    )
    expected_runtime = _metadata(
        "xruntime",
        "xrt/single/extlibs/xruntime.h",
        "xrt/single/extlibs/xruntime_decl.h",
        runtime_implementation_data,
        runtime_declaration_data,
        len(runtime_names),
        len(runtime_names),
    )
    expected_runtime["features"] = _artifact(
        "xrt/extlibs/xruntime/include/xruntime/features.h", runtime_features.read_bytes())
    upstream_hash = _digest(upstream_path.read_bytes())
    expected_core["upstreamSha256"] = upstream_hash
    expected_runtime["upstreamSha256"] = upstream_hash
    if actual_core != expected_core:
        print("XRT snapshot metadata is stale", file=sys.stderr)
        return False
    if actual_runtime != expected_runtime:
        print("xruntime snapshot metadata is stale", file=sys.stderr)
        return False
    if symbols_path.read_text(encoding="utf-8") != expected_symbols:
        print("XRT/xruntime symbol table is stale", file=sys.stderr)
        return False
    matrix_current, _ = check_capability_matrix(root)
    if not matrix_current:
        print("XRT/xlang capability matrix is stale", file=sys.stderr)
        return False

    print(
        "XRT/xruntime snapshots are current: "
        f"xrt={expected_core['publicApiCount']} APIs, "
        f"xruntime={expected_runtime['publicApiCount']} APIs "
        f"({expected_runtime['ownedPublicApiCount']} owned)"
    )
    return True



def _validate_source(xrt_root: Path, validations: list[dict[str, object]],
                     allow_development_source: bool = False) -> tuple[Path, Path, Path, Path]:
    """要求源 XRT 与 xruntime 先通过发布成熟度和生成头检查。"""

    core_implementation = xrt_root / "single" / "xrt.h"
    core_declarations = xrt_root / "single" / "xrt_decl.h"
    runtime_implementation = xrt_root / "single" / "extlibs" / "xruntime.h"
    runtime_declarations = xrt_root / "single" / "extlibs" / "xruntime_decl.h"
    runtime_manifest = "extlibs/xruntime/config/modules.json"
    checks = [
        [sys.executable, str(xrt_root / "tools" / "check_release_maturity.py")],
        [sys.executable, str(xrt_root / "tools" / "amalgamate.py"), "--check"],
        [
            sys.executable,
            str(xrt_root / "tools" / "check_release_maturity.py"),
            "--release",
            "--manifest",
            runtime_manifest,
        ],
        [
            sys.executable,
            str(xrt_root / "tools" / "amalgamate.py"),
            "--check",
            "--manifest",
            runtime_manifest,
        ],
    ]
    for path in (
        core_implementation,
        core_declarations,
        runtime_implementation,
        runtime_declarations,
    ):
        if not path.is_file():
            raise ValueError(f"runtime generated header is missing: {path}")
    for command in checks:
        environment = {**os.environ, "PYTHONUTF8": "1"}
        completed = subprocess.run(command, cwd=xrt_root, env=environment,
                                   capture_output=True, text=True, encoding="utf-8")
        maturity = Path(command[1]).name == "check_release_maturity.py"
        record: dict[str, object] = {
            "tool": Path(command[1]).name,
            "arguments": command[2:],
            "exitCode": completed.returncode,
        }
        if completed.returncode:
            record["diagnostic"] = (completed.stdout + completed.stderr).strip()
        validations.append(record)
        if completed.returncode and not (maturity and allow_development_source):
            raise ValueError(str(record.get("diagnostic", "XRT producer validation failed")))
        if completed.returncode:
            print("upstream release maturity is incomplete; consuming explicitly selected development source")
    return (
        core_implementation,
        core_declarations,
        runtime_implementation,
        runtime_declarations,
    )



def _sync(root: Path, xrt_root: Path, allow_development_source: bool = False) -> bool:
    """从通过审计的 XRT 源树同步快照及其派生资产。"""

    # Generate from an isolated read-only export of the current producer tree.
    # Never rewrite a developer's generated files or consume stale amalgamations.
    if not (xrt_root / "source.snapshot.json").is_file():
        revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=xrt_root).decode("ascii").strip()
        dirty = bool(subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=normal"], cwd=xrt_root))
        names = set()
        for flags in (["ls-files", "-z"], ["ls-files", "--others", "--exclude-standard", "-z"]):
            names.update(subprocess.check_output(["git", *flags], cwd=xrt_root).decode("utf-8").split("\0"))
        prefixes = ("config/", "include/", "src/", "tools/", "tests/", "docs/", "examples/", "dev/", "fuzz/", "extlibs/xruntime/")
        fingerprint = hashlib.sha256()
        with tempfile.TemporaryDirectory(prefix="xlang-xrt-") as directory:
            export = Path(directory)
            for name in sorted(names):
                source = xrt_root / name
                if not name or not source.is_file() or not (name.startswith(prefixes) or name in ("LICENSE", "README.md")):
                    continue
                data = source.read_bytes()
                fingerprint.update(name.encode("utf-8") + b"\0" + hashlib.sha256(data).digest())
                target = export / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
            provenance = dict(revision=revision, dirty=dirty, sourceSha256=fingerprint.hexdigest())
            (export / "source.snapshot.json").write_text(json.dumps(provenance, indent=2)+"\n", encoding="utf-8")
            subprocess.run([sys.executable, "-B", "tools/amalgamate.py", "--manifest", "extlibs/xruntime/config/modules.json"],
                           cwd=export, check=True, env={**os.environ, "PYTHONUTF8": "1"}, stdout=subprocess.DEVNULL)
            return _sync(root, export, allow_development_source)

    validations: list[dict[str, object]] = []
    (
        core_source,
        core_declaration_source,
        runtime_source,
        runtime_declaration_source,
    ) = _validate_source(xrt_root, validations, allow_development_source)
    core_destination = root / "lib" / "xrt" / "xrt.h"
    core_declaration_destination = root / "release" / "include" / "xrt_decl.h"
    core_metadata_path = root / "lib" / "xrt" / "xrt.snapshot.json"
    runtime_destination = root / "lib" / "xruntime" / "xruntime.h"
    runtime_declaration_destination = root / "release" / "include" / "xruntime_decl.h"
    runtime_metadata_path = root / "lib" / "xruntime" / "xruntime.snapshot.json"
    runtime_features = root / "lib" / "xruntime" / "xruntime_features.h"
    upstream_path = root / "lib" / "xrt" / "upstream.json"
    symbols_path = root / "src" / "api" / "xl_xrt_symbols.inc"
    # These tracked C headers have eol=lf. Hash the same canonical bytes that
    # survive Git checkout, rather than a Windows generator's local CRLF form.
    # Otherwise committing an apparently valid snapshot invalidates its receipt.
    def header_bytes(path: Path) -> bytes:
        return path.read_bytes().replace(b"\r\n", b"\n")
    core_implementation_data = header_bytes(core_source)
    core_declaration_data = header_bytes(core_declaration_source)
    runtime_implementation_data = header_bytes(runtime_source)
    runtime_declaration_data = header_bytes(runtime_declaration_source)
    features_data = header_bytes(xrt_root / "extlibs" / "xruntime" / "include" / "xruntime" / "features.h")
    provenance_path = xrt_root / "source.snapshot.json"
    if provenance_path.is_file():
        provenance = json.loads(provenance_path.read_text(encoding="utf-8"))
    else:
        revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=xrt_root, text=True).strip()
        provenance = {"revision": revision, "dirty": bool(subprocess.check_output(
            ["git", "status", "--porcelain", "--untracked-files=no"], cwd=xrt_root))}
    provenance["checks"] = validations
    upstream_data = (json.dumps(provenance, ensure_ascii=False, indent=2) + "\n").encode("utf-8")

    for path in (
        core_destination,
        core_declaration_destination,
        runtime_destination,
        runtime_declaration_destination,
        core_metadata_path,
        runtime_metadata_path,
        symbols_path,
    ):
        path.parent.mkdir(parents=True, exist_ok=True)
    changed = _write_if_changed(core_destination, core_implementation_data)
    changed = _write_if_changed(core_declaration_destination, core_declaration_data) or changed
    changed = _write_if_changed(runtime_destination, runtime_implementation_data) or changed
    changed = _write_if_changed(
        runtime_declaration_destination,
        runtime_declaration_data,
    ) or changed
    changed = _write_if_changed(runtime_features, features_data) or changed
    changed = _write_if_changed(upstream_path, upstream_data) or changed

    core_names = set(_api_names(core_destination))
    runtime_names = set(_api_names(runtime_destination))
    if core_names & runtime_names:
        raise ValueError("XRT/xruntime public API ownership overlaps")
    symbols = render_symbol_table(core_destination, runtime_destination)
    core_metadata = _metadata(
        "xrt",
        "xrt/single/xrt.h",
        "xrt/single/xrt_decl.h",
        core_implementation_data,
        core_declaration_data,
        len(core_names),
    )
    runtime_metadata = _metadata(
        "xruntime",
        "xrt/single/extlibs/xruntime.h",
        "xrt/single/extlibs/xruntime_decl.h",
        runtime_implementation_data,
        runtime_declaration_data,
        len(runtime_names),
        len(runtime_names),
    )
    runtime_metadata["features"] = _artifact(
        "xrt/extlibs/xruntime/include/xruntime/features.h", features_data)
    core_metadata["upstreamSha256"] = _digest(upstream_data)
    runtime_metadata["upstreamSha256"] = _digest(upstream_data)
    changed = _write_if_changed(
        core_metadata_path,
        (json.dumps(core_metadata, ensure_ascii=True, indent=2) + "\n").encode("utf-8"),
    ) or changed
    changed = _write_if_changed(
        runtime_metadata_path,
        (json.dumps(runtime_metadata, ensure_ascii=True, indent=2) + "\n").encode("utf-8"),
    ) or changed
    changed = _write_if_changed(
        symbols_path,
        symbols.encode("utf-8"),
    ) or changed
    sync_capability_matrix(root)

    if not _check_snapshot(root):
        return False
    print("XRT snapshot synchronized" if changed else "XRT snapshot did not change")
    return True



def main() -> int:
    """解析命令行并执行纯仓内检查或显式维护同步。"""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
    )
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument("--check", action="store_true")
    modes.add_argument("--sync", action="store_true")
    parser.add_argument("--allow-development-source", action="store_true",
                        help="record incomplete upstream release metadata while requiring fresh generated headers")
    parser.add_argument(
        "--xrt-root",
        type=Path,
        default=Path(os.environ["XRT_SOURCE_ROOT"])
        if "XRT_SOURCE_ROOT" in os.environ else None,
    )
    arguments = parser.parse_args()
    root = arguments.root.resolve()

    if arguments.check:
        return 0 if _check_snapshot(root) else 1
    if arguments.xrt_root is None:
        parser.error("--sync requires --xrt-root or XRT_SOURCE_ROOT")
    try:
        return 0 if _sync(root, arguments.xrt_root.resolve(), arguments.allow_development_source) else 1
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"XRT snapshot synchronization failed: {error}", file=sys.stderr)
        return 1



if __name__ == "__main__":
    raise SystemExit(main())
