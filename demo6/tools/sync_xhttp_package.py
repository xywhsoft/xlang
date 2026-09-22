#!/usr/bin/env python3
"""同步 xlang xhttp 基础包使用的 XRT xhttp 源码快照。"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys


PACKAGE_FILES = {
    "include/xhttp/features.h": "extlibs/xhttp/include/xhttp/features.h",
    "include/xrt/url.h": "extlibs/xhttp/include/xrt/url.h",
    "include/xrt/query.h": "extlibs/xhttp/include/xrt/query.h",
    "include/xrt/form.h": "extlibs/xhttp/include/xrt/form.h",
    "include/xrt/query_params.h": "extlibs/xhttp/include/xrt/query_params.h",
    "src/internal/xrt_url.h": "extlibs/xhttp/src/internal/xrt_url.h",
    "src/internal/xrt_query.h": "extlibs/xhttp/src/internal/xrt_query.h",
    "src/internal/xrt_query_codec.h": "extlibs/xhttp/src/internal/xrt_query_codec.h",
    "src/internal/xrt_form.h": "extlibs/xhttp/src/internal/xrt_form.h",
    "src/internal/xrt_query_params.h": "extlibs/xhttp/src/internal/xrt_query_params.h",
    "src/internal/xrt_internal.h": "extlibs/xhttp/src/internal/xrt_internal.h",
    "src/internal/xrt_codec.h": "extlibs/xhttp/src/internal/xrt_codec.h",
    "src/internal/xrt_core_internal.h": "src/internal/xrt_internal.h",
    "src/internal/xrt_core_codec.h": "src/internal/xrt_codec.h",
    "src/url/url.c": "extlibs/xhttp/src/url/url.c",
    "src/url/query.c": "extlibs/xhttp/src/url/query.c",
    "src/url/query_codec.c": "extlibs/xhttp/src/url/query_codec.c",
    "src/url/form.c": "extlibs/xhttp/src/url/form.c",
    "src/url/query_params.c": "extlibs/xhttp/src/url/query_params.c",
}


def transformed(relative: str, data: bytes) -> bytes:
    """把上游仓库内相对桥接改为包内精确快照桥接。"""
    if relative == "include/xrt/url.h":
        text = data.decode("utf-8")
        text = text.replace("#include <xrt/core.h>", "#include <xrt.h>")
        text = text.replace(
            "#if defined(XHTTP_FEATURE_URL_PARAM)\n\t#include <xrt/http.h>\n#endif",
            "",
        )
        return text.encode("utf-8")
    if relative in {"include/xrt/query.h", "include/xrt/form.h"}:
        text = data.decode("utf-8")
        text = text.replace("#include <xrt/core.h>", "#include <xrt.h>")
        text = text.replace("#include <xrt/memory.h>\n", "")
        return text.encode("utf-8")
    if relative == "include/xrt/query_params.h":
        text = data.decode("utf-8")
        text = text.replace("#include <xrt/form.h>", '#include "form.h"')
        text = text.replace("#include <xrt/query.h>", '#include "query.h"')
        return text.encode("utf-8")
    if relative == "src/internal/xrt_url.h":
        text = data.decode("utf-8")
        text = text.replace("#include <xrt/http.h>", "#include <xrt.h>")
        text = text.replace(
            "#include <xrt/url.h>",
            '#include "../../include/xrt/url.h"',
        )
        return text.encode("utf-8")
    if relative == "src/internal/xrt_query.h":
        text = data.decode("utf-8")
        text = text.replace(
            "#include <xrt/query.h>",
            '#include "../../include/xrt/query.h"',
        )
        return text.encode("utf-8")
    if relative == "src/internal/xrt_form.h":
        text = data.decode("utf-8")
        text = text.replace(
            "#include <xrt/form.h>",
            '#include "../../include/xrt/form.h"',
        )
        return text.encode("utf-8")
    if relative == "src/internal/xrt_query_params.h":
        text = data.decode("utf-8")
        text = text.replace(
            "#include <xrt/query_params.h>",
            '#include "../../include/xrt/query_params.h"',
        )
        return text.encode("utf-8")
    if relative == "src/internal/xrt_internal.h":
        text = data.decode("utf-8")
        text = text.replace(
            '#include "../../../../src/internal/xrt_internal.h"',
            '#include "../../include/xhttp_package.h"\n'
            '#include "xrt_core_internal.h"',
        )
        return text.encode("utf-8")
    if relative == "src/internal/xrt_codec.h":
        text = data.decode("utf-8")
        text = text.replace(
            '#include "../../../../src/internal/xrt_codec.h"',
            '#include "xrt_core_codec.h"',
        )
        return text.encode("utf-8")
    if relative == "src/internal/xrt_core_codec.h":
        text = data.decode("utf-8")
        text = text.replace(
            '#include "xrt_internal.h"',
            '#include "xrt_core_internal.h"',
        )
        text = text.replace("#include <xrt/codec.h>", "#include <xrt.h>")
        return text.encode("utf-8")
    if relative == "src/internal/xrt_core_internal.h":
        text = data.decode("utf-8")
        errors = {
            "void __xrtErrorSetInvalidArgument(void);": (
                "static inline void __xrtErrorSetInvalidArgument(void)\n"
                "{\n"
                "\txrtSetErrorInfo(\n"
                "\t\tXERR_ARGUMENT, \"xlang.extlib.xhttp\", 1,\n"
                "\t\t\"invalid argument\"\n"
                "\t);\n"
                "}"
            ),
            "void __xrtErrorSetValue(void);": (
                "static inline void __xrtErrorSetValue(void)\n"
                "{\n"
                "\txrtSetErrorInfo(\n"
                "\t\tXERR_VALUE, \"xlang.extlib.xhttp\", 2,\n"
                "\t\t\"invalid value\"\n"
                "\t);\n"
                "}"
            ),
            "void __xrtErrorSetSizeOverflow(void);": (
                "static inline void __xrtErrorSetSizeOverflow(void)\n"
                "{\n"
                "\txrtSetErrorInfo(\n"
                "\t\tXERR_RANGE, \"xlang.extlib.xhttp\", 3,\n"
                "\t\t\"size overflow\"\n"
                "\t);\n"
                "}"
            ),
            "void __xrtErrorSetRange(void);": (
                "static inline void __xrtErrorSetRange(void)\n"
                "{\n"
                "\txrtSetErrorInfo(\n"
                "\t\tXERR_RANGE, \"xlang.extlib.xhttp\", 4,\n"
                "\t\t\"range is out of bounds\"\n"
                "\t);\n"
                "}"
            ),
        }
        for declaration, implementation in errors.items():
            if declaration not in text:
                raise ValueError(
                    f"missing XRT error declaration in {relative}: {declaration}"
                )
            text = text.replace(declaration, implementation)
        return text.encode("utf-8")
    return data


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def expected_files(xrt_root: Path) -> dict[str, bytes]:
    result: dict[str, bytes] = {}
    for target, source in PACKAGE_FILES.items():
        source_path = xrt_root / source
        if not source_path.is_file():
            raise FileNotFoundError(f"missing xhttp source: {source_path}")
        result[target] = transformed(target, source_path.read_bytes())
    return result


def snapshot_text(files: dict[str, bytes]) -> bytes:
    snapshot = {
        "format": "xlang-xhttp-snapshot-1",
        "modules": [
            "url",
            "query",
            "query_codec",
            "form_urlencoded",
            "query_params",
        ],
        "files": {
            name: digest(data)
            for name, data in sorted(files.items())
        },
    }
    return (json.dumps(snapshot, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def sync(package_root: Path, files: dict[str, bytes]) -> None:
    for relative, data in files.items():
        target = package_root / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.is_file() or target.read_bytes() != data:
            target.write_bytes(data)
    (package_root / "xhttp.snapshot.json").write_bytes(snapshot_text(files))


def check(package_root: Path, files: dict[str, bytes]) -> bool:
    expected = dict(files)
    expected["xhttp.snapshot.json"] = snapshot_text(files)
    stale: list[str] = []
    for relative, data in expected.items():
        target = package_root / relative
        if not target.is_file() or target.read_bytes() != data:
            stale.append(relative)
    if stale:
        for relative in stale:
            print(f"stale xhttp package snapshot: {relative}", file=sys.stderr)
        return False
    print(f"xhttp package snapshot is current: {len(files)} source files")
    return True


def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent
    default_xrt = repo_root.parent.parent / "xrt"
    parser = argparse.ArgumentParser()
    parser.add_argument("--xrt-root", type=Path, default=default_xrt)
    parser.add_argument("--sync", action="store_true")
    args = parser.parse_args()

    xrt_root = args.xrt_root.resolve()
    package_root = repo_root / "release" / "extlibs" / "xhttp"
    files = expected_files(xrt_root)
    if args.sync:
        sync(package_root, files)
    return 0 if check(package_root, files) else 1


if __name__ == "__main__":
    raise SystemExit(main())
