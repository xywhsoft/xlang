#!/usr/bin/env python3
"""同步 xlang xhttp.body_file 使用的 XRT 异步文件正文源码快照。"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys


PACKAGE_FILES = {
	"include/xhttp/features.h": "extlibs/xhttp/include/xhttp/features.h",
	"include/xrt/http_body.h": "extlibs/xhttp/include/xrt/http_body.h",
	"include/xrt/http_body_file.h": (
		"extlibs/xhttp/include/xrt/http_body_file.h"
	),
	"src/internal/xhttp_internal.h": (
		"extlibs/xhttp/src/internal/xhttp_internal.h"
	),
	"src/internal/xrt_http_body.h": (
		"extlibs/xhttp/src/internal/xrt_http_body.h"
	),
	"src/internal/xrt_http_body_file.h": (
		"extlibs/xhttp/src/internal/xrt_http_body_file.h"
	),
	"src/http/http_body_file.c": (
		"extlibs/xhttp/src/http/http_body_file.c"
	),
}


def transformed(relative: str, data: bytes) -> bytes:
	"""把模块化 XRT 头引用改成 demo6 单头文件包引用。"""
	text = data.decode("utf-8")
	if relative == "include/xrt/http_body.h":
		lines = text.splitlines(keepends=True)
		result: list[str] = []
		seen_xrt = False
		for line in lines:
			if line.lstrip().startswith("#include <xrt/"):
				if not seen_xrt:
					result.append("#include <xrt.h>\n")
					seen_xrt = True
				continue
			result.append(line)
		return "".join(result).encode("utf-8")
	if relative == "include/xrt/http_body_file.h":
		text = text.replace("#include <xrt/file_async.h>\n", "")
		text = text.replace(
			"#include <xrt/http_body.h>",
			'#include "http_body.h"',
		)
		return text.encode("utf-8")
	if relative == "src/internal/xhttp_internal.h":
		begin = text.index("#include <xrt/error.h>")
		end = text.index("#include <limits.h>")
		text = (
			text[:begin]
			+ '#include "../../include/xhttp_body_file_package.h"\n\n'
			+ text[end:]
		)
		return text.encode("utf-8")
	if relative == "src/internal/xrt_http_body.h":
		text = text.replace("#include <xrt/atomic.h>\n", "")
		text = text.replace(
			"#include <xrt/http_body.h>",
			'#include "../../include/xrt/http_body.h"',
		)
		return text.encode("utf-8")
	if relative == "src/internal/xrt_http_body_file.h":
		text = text.replace(
			"#include <xrt/http_body_file.h>",
			'#include "../../include/xrt/http_body_file.h"',
		)
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
		"format": "xlang-xhttp-body-file-snapshot-1",
		"modules": ["http_body_file"],
		"files": {
			name: digest(data)
			for name, data in sorted(files.items())
		},
	}
	return (json.dumps(snapshot, ensure_ascii=False, indent=2) + "\n").encode(
		"utf-8"
	)


def sync(package_root: Path, files: dict[str, bytes]) -> None:
	for relative, data in files.items():
		target = package_root / relative
		target.parent.mkdir(parents=True, exist_ok=True)
		if not target.is_file() or target.read_bytes() != data:
			target.write_bytes(data)
	(package_root / "xhttp_body_file.snapshot.json").write_bytes(
		snapshot_text(files)
	)


def check(package_root: Path, files: dict[str, bytes]) -> bool:
	expected = dict(files)
	expected["xhttp_body_file.snapshot.json"] = snapshot_text(files)
	stale: list[str] = []
	for relative, data in expected.items():
		target = package_root / relative
		if not target.is_file() or target.read_bytes() != data:
			stale.append(relative)
	if stale:
		for relative in stale:
			print(
				f"stale xhttp.body_file package snapshot: {relative}",
				file=sys.stderr,
			)
		return False
	print(
		"xhttp.body_file package snapshot is current: "
		f"{len(files)} source files"
	)
	return True


def main() -> int:
	repo_root = Path(__file__).resolve().parent.parent
	default_xrt = repo_root.parent.parent / "xrt"
	parser = argparse.ArgumentParser()
	parser.add_argument("--xrt-root", type=Path, default=default_xrt)
	parser.add_argument("--sync", action="store_true")
	args = parser.parse_args()

	xrt_root = args.xrt_root.resolve()
	package_root = repo_root / "release" / "extlibs" / "xhttp.body_file"
	files = expected_files(xrt_root)
	if args.sync:
		sync(package_root, files)
	return 0 if check(package_root, files) else 1


if __name__ == "__main__":
	raise SystemExit(main())
