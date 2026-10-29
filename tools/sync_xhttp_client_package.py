#!/usr/bin/env python3
"""同步 xlang xhttp.client 使用的 XRT 客户端请求源码快照。"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys


PACKAGE_FILES = {
	"include/xhttp/features.h": "extlibs/xhttp/include/xhttp/features.h",
	"include/xrt/http_client.h": "extlibs/xhttp/include/xrt/http_client.h",
	"include/xrt/http_exchange.h": (
		"extlibs/xhttp/include/xrt/http_exchange.h"
	),
	"include/xrt/http_client_stream.h": (
		"extlibs/xhttp/include/xrt/http_client_stream.h"
	),
	"include/xrt/http_client_runtime.h": (
		"extlibs/xhttp/include/xrt/http_client_runtime.h"
	),
	"include/xrt/http_client_future.h": (
		"extlibs/xhttp/include/xrt/http_client_future.h"
	),
	"src/internal/xrt_http_client.h": (
		"extlibs/xhttp/src/internal/xrt_http_client.h"
	),
	"src/internal/xrt_http_exchange.h": (
		"extlibs/xhttp/src/internal/xrt_http_exchange.h"
	),
	"src/internal/xrt_http_client_stream.h": (
		"extlibs/xhttp/src/internal/xrt_http_client_stream.h"
	),
	"src/internal/xrt_http_client_runtime.h": (
		"extlibs/xhttp/src/internal/xrt_http_client_runtime.h"
	),
	"src/http/http_client_request.c": (
		"extlibs/xhttp/src/http/http_client_request.c"
	),
	"src/http/http_client_request_trailers.c": (
		"extlibs/xhttp/src/http/http_client_request_trailers.c"
	),
	"src/http/http_client_request_te.c": (
		"extlibs/xhttp/src/http/http_client_request_te.c"
	),
	"src/http/http_client_request_query.c": (
		"extlibs/xhttp/src/http/http_client_request_query.c"
	),
	"src/http/http_client_request_form.c": (
		"extlibs/xhttp/src/http/http_client_request_form.c"
	),
	"src/http/http_client_request_form_data.c": (
		"extlibs/xhttp/src/http/http_client_request_form_data.c"
	),
	"src/http/http_client_prepare.c": (
		"extlibs/xhttp/src/http/http_client_prepare.c"
	),
	"src/http/http_client_response.c": (
		"extlibs/xhttp/src/http/http_client_response.c"
	),
	"src/http/http_client_exchange.c": (
		"extlibs/xhttp/src/http/http_client_exchange.c"
	),
	"src/http/http_client_exchange_output.c": (
		"extlibs/xhttp/src/http/http_client_exchange_output.c"
	),
	"src/http/http_client_exchange_input.c": (
		"extlibs/xhttp/src/http/http_client_exchange_input.c"
	),
	"src/http/http_client_stream.c": (
		"extlibs/xhttp/src/http/http_client_stream.c"
	),
	"src/http/http_client_runtime.c": (
		"extlibs/xhttp/src/http/http_client_runtime.c"
	),
	"src/http/http_client_tcp.c": (
		"extlibs/xhttp/src/http/http_client_tcp.c"
	),
	"src/http/http_client_future.c": (
		"extlibs/xhttp/src/http/http_client_future.c"
	),
	"src/http/http_client_https.c": (
		"extlibs/xhttp/src/http/http_client_https.c"
	),
}


def transformed(relative: str, data: bytes) -> bytes:
	"""裁出 Request 私有结构，并把依赖改为已声明的兄弟包契约。"""
	text = data.decode("utf-8")
	if relative.startswith("include/xrt/"):
		lines = text.splitlines(keepends=True)
		return "".join(
			line
			for line in lines
			if not line.lstrip().startswith("#include <xrt/")
		).encode("utf-8")
	if relative == "src/internal/xrt_http_client.h":
		lines = text.splitlines(keepends=True)
		body = "".join(
			line
			for line in lines
			if not line.lstrip().startswith('#include "xrt_')
			and not line.lstrip().startswith("#include <xrt/")
		)
		result = body.replace(
			"#define XRT_INTERNAL_HTTP_CLIENT_H\n",
			"#define XRT_INTERNAL_HTTP_CLIENT_H\n\n"
			'#include "../../include/xhttp_client_package.h"\n'
			'#include "../../../xhttp.body/src/internal/'
			'xrt_http_body.h"\n\n'
			"#include <string.h>\n\n\n\n"
			"/* Header 所有权检查由 xhttp.protocol 包提供。 */\n"
			"bool __xrtHttpHeadersOwnedOverlap(\n"
			"\tconst xhttpheaders* pHeaders,\n"
			"\tconst void* pOutput,\n"
			"\tsize_t iSize\n"
			");\n",
			1,
		)
		return result.encode("utf-8")
	if relative == "src/internal/xrt_http_exchange.h":
		lines = text.splitlines(keepends=True)
		body = "".join(
			line
			for line in lines
			if not line.lstrip().startswith("#include <xrt/")
			and not line.lstrip().startswith(
				'#include "xrt_http_client.h"'
			)
		)
		return body.replace(
			"#define XRT_INTERNAL_HTTP_EXCHANGE_H\n",
			"#define XRT_INTERNAL_HTTP_EXCHANGE_H\n\n"
			'#include "xrt_http_client.h"\n',
			1,
		).encode("utf-8")
	if relative == "src/internal/xrt_http_client_stream.h":
		lines = text.splitlines(keepends=True)
		body = "".join(
			line
			for line in lines
			if not line.lstrip().startswith(
				'#include "xrt_internal.h"'
			)
			and not line.lstrip().startswith("#include <xrt/")
		)
		return body.replace(
			"#define XRT_INTERNAL_HTTP_CLIENT_STREAM_H\n",
			"#define XRT_INTERNAL_HTTP_CLIENT_STREAM_H\n\n"
			'#include "../../include/xhttp_client_package.h"\n',
			1,
		).encode("utf-8")
	if relative == "src/internal/xrt_http_client_runtime.h":
		lines = text.splitlines(keepends=True)
		return "".join(
			line
			for line in lines
			if not line.lstrip().startswith("#include <xrt/")
		).encode("utf-8")
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
		"format": "xlang-xhttp-client-snapshot-1",
		"modules": [
			"http_client_request",
			"http_client_request_trailers",
			"http_client_request_te",
			"http_client_request_query",
			"http_client_request_form",
			"http_client_request_form_data",
			"http_client_prepare",
			"http_client_response",
			"http_exchange",
			"http_client_stream",
			"http_client",
			"http_client_future",
			"http_client_https",
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
	(package_root / "xhttp_client.snapshot.json").write_bytes(
		snapshot_text(files)
	)


def check(package_root: Path, files: dict[str, bytes]) -> bool:
	expected = dict(files)
	expected["xhttp_client.snapshot.json"] = snapshot_text(files)
	stale: list[str] = []
	for relative, data in expected.items():
		target = package_root / relative
		if not target.is_file() or target.read_bytes() != data:
			stale.append(relative)
	if stale:
		for relative in stale:
			print(
				f"stale xhttp.client package snapshot: {relative}",
				file=sys.stderr,
			)
		return False
	print(
		"xhttp.client package snapshot is current: "
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
	package_root = repo_root / "release" / "extlibs" / "xhttp.client"
	files = expected_files(xrt_root)
	if args.sync:
		sync(package_root, files)
	return 0 if check(package_root, files) else 1


if __name__ == "__main__":
	raise SystemExit(main())
