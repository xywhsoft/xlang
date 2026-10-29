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

from gen_xrt_symbol_table import _api_names, _render as render_symbol_table
from gen_xrt_xlang_matrix import check as check_capability_matrix
from gen_xrt_xlang_matrix import sync as sync_capability_matrix


SCHEMA = 3



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
    symbols_path = root / "src" / "api" / "xl_xrt_symbols.inc"
    if not all(path.is_file() for path in (
        core_header,
        core_declarations,
        core_metadata_path,
        runtime_header,
        runtime_declarations,
        runtime_metadata_path,
        symbols_path,
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
        expected_symbols = render_symbol_table(runtime_header)
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
        "xrt/extlibs/xruntime/single/xruntime.h",
        "xrt/extlibs/xruntime/single/xruntime_decl.h",
        runtime_implementation_data,
        runtime_declaration_data,
        len(runtime_names),
        len(runtime_names - core_names),
    )
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



def _validate_source(xrt_root: Path) -> tuple[Path, Path, Path, Path]:
    """要求源 XRT 与 xruntime 先通过发布成熟度和生成头检查。"""

    core_implementation = xrt_root / "single" / "xrt.h"
    core_declarations = xrt_root / "single" / "xrt_decl.h"
    runtime_implementation = xrt_root / "extlibs" / "xruntime" / "single" / "xruntime.h"
    runtime_declarations = xrt_root / "extlibs" / "xruntime" / "single" / "xruntime_decl.h"
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
        subprocess.run(command, cwd=xrt_root, check=True)
    return (
        core_implementation,
        core_declarations,
        runtime_implementation,
        runtime_declarations,
    )



def _sync(root: Path, xrt_root: Path) -> bool:
    """从通过审计的 XRT 源树同步快照及其派生资产。"""

    (
        core_source,
        core_declaration_source,
        runtime_source,
        runtime_declaration_source,
    ) = _validate_source(xrt_root)
    core_destination = root / "lib" / "xrt" / "xrt.h"
    core_declaration_destination = root / "release" / "include" / "xrt_decl.h"
    core_metadata_path = root / "lib" / "xrt" / "xrt.snapshot.json"
    runtime_destination = root / "lib" / "xruntime" / "xruntime.h"
    runtime_declaration_destination = root / "release" / "include" / "xruntime_decl.h"
    runtime_metadata_path = root / "lib" / "xruntime" / "xruntime.snapshot.json"
    symbols_path = root / "src" / "api" / "xl_xrt_symbols.inc"
    core_implementation_data = core_source.read_bytes()
    core_declaration_data = core_declaration_source.read_bytes()
    runtime_implementation_data = runtime_source.read_bytes()
    runtime_declaration_data = runtime_declaration_source.read_bytes()

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

    core_names = set(_api_names(core_destination))
    runtime_names = set(_api_names(runtime_destination))
    symbols = render_symbol_table(runtime_destination)
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
        "xrt/extlibs/xruntime/single/xruntime.h",
        "xrt/extlibs/xruntime/single/xruntime_decl.h",
        runtime_implementation_data,
        runtime_declaration_data,
        len(runtime_names),
        len(runtime_names - core_names),
    )
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
        return 0 if _sync(root, arguments.xrt_root.resolve()) else 1
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"XRT snapshot synchronization failed: {error}", file=sys.stderr)
        return 1



if __name__ == "__main__":
    raise SystemExit(main())
