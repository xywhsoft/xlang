#!/usr/bin/env python3
"""同步 xlang xhttp.form_data 使用的 XRT FormData 源码快照。"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys


PACKAGE_FILES = {
	"include/xhttp/features.h": "extlibs/xhttp/include/xhttp/features.h",
	"include/xrt/form_data.h": "extlibs/xhttp/include/xrt/form_data.h",
	"src/internal/xrt_form_data.h": (
		"extlibs/xhttp/src/internal/xrt_form_data.h"
	),
	"src/http/form_data.c": "extlibs/xhttp/src/http/form_data.c",
	"src/http/form_data_multipart.c": (
		"extlibs/xhttp/src/http/form_data_multipart.c"
	),
	"src/http/form_data_parse.c": (
		"extlibs/xhttp/src/http/form_data_parse.c"
	),
}


def transformed(relative: str, data: bytes) -> bytes:
	"""把内部模块入口改为包头，公开扩展依赖仍由依赖包提供。"""
	text = data.decode("utf-8")
	if relative == "include/xrt/form_data.h":
		lines = text.splitlines(keepends=True)
		return "".join(
			line
			for line in lines
			if not line.lstrip().startswith("#include <xrt/")
		).encode("utf-8")
	if relative == "src/internal/xrt_form_data.h":
		lines = text.splitlines(keepends=True)
		result: list[str] = []
		inserted = False
		for line in lines:
			if line.lstrip().startswith("#include <xrt/"):
				if not inserted:
					result.append(
						'#include "../../include/'
						'xhttp_form_data_package.h"\n'
					)
					inserted = True
				continue
			result.append(line)
		return "".join(result).encode("utf-8")
	if relative.startswith("src/http/"):
		lines = text.splitlines(keepends=True)
		return "".join(
			line
			for line in lines
			if not line.lstrip().startswith("#include <xrt/")
		).encode("utf-8")
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
		"format": "xlang-xhttp-form-data-snapshot-1",
		"modules": [
			"form_data",
			"form_data_multipart",
			"form_data_parse",
		],
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
	(package_root / "xhttp_form_data.snapshot.json").write_bytes(
		snapshot_text(files)
	)


def check(package_root: Path, files: dict[str, bytes]) -> bool:
	expected = dict(files)
	expected["xhttp_form_data.snapshot.json"] = snapshot_text(files)
	stale: list[str] = []
	for relative, data in expected.items():
		target = package_root / relative
		if not target.is_file() or target.read_bytes() != data:
			stale.append(relative)
	if stale:
		for relative in stale:
			print(
				f"stale xhttp.form_data package snapshot: {relative}",
				file=sys.stderr,
			)
		return False
	print(
		"xhttp.form_data package snapshot is current: "
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
	package_root = repo_root / "release" / "extlibs" / "xhttp.form_data"
	files = expected_files(xrt_root)
	if args.sync:
		sync(package_root, files)
	return 0 if check(package_root, files) else 1


if __name__ == "__main__":
	raise SystemExit(main())
