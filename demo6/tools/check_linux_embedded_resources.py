#!/usr/bin/env python3
"""Verify the actual Linux products' embedded resource bytes, not just packing inputs."""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import lzma
import sys
from pathlib import Path

from gen_tcc_vfs_resources import load_resources


class BuiltinResource(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char_p),
        ("data", ctypes.POINTER(ctypes.c_ubyte)),
        ("packed_size", ctypes.c_size_t),
        ("size", ctypes.c_size_t),
        ("method", ctypes.c_uint),
    ]


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def decode(packed: bytes, size: int, method: int) -> bytes:
    if method == 0:
        if len(packed) != size:
            raise ValueError("stored resource length mismatch")
        return packed
    if method != 1 or len(packed) < 5:
        raise ValueError("unsupported resource compression contract")
    # The packer stores five LZMA properties followed by a stream without an
    # end marker. Supply the declared length to the independent stdlib decoder.
    alone = packed[:5] + size.to_bytes(8, "little") + packed[5:]
    reader = lzma.LZMADecompressor(format=lzma.FORMAT_ALONE)
    data = reader.decompress(alone, max_length=size + 1)
    if len(data) != size or not reader.eof or reader.unused_data:
        raise ValueError("compressed resource length/trailing data mismatch")
    return data


def verify_resource(actual: BuiltinResource, name: str, source: bytes) -> None:
    actual_name = actual.name.decode("utf-8") if actual.name else ""
    if actual_name != name or actual.size != len(source) or not actual.data:
        raise AssertionError(f"resource identity/length mismatch: {name}")
    # The manifest packer stores a resource or chooses a strictly smaller
    # encoding. Reject an impossible advertised read extent before dereferencing.
    if actual.method not in (0, 1) or actual.packed_size > len(source) or (
            actual.method == 1 and actual.packed_size >= len(source)):
        raise AssertionError(f"resource packed extent/method mismatch: {name}")
    packed = ctypes.string_at(actual.data, actual.packed_size)
    if decode(packed, actual.size, actual.method) != source:
        raise AssertionError(f"stale product resource: {name}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != "linux" or ctypes.sizeof(ctypes.c_void_p) != 8:
        parser.error("this gate requires the supported Linux 64-bit host")
    root = args.root.resolve()
    report = args.report.resolve()
    if report.exists():
        parser.error("use a fresh report path; failed evidence must not be overwritten")
    build = root / "build/linux"
    inputs = {}
    lanes = []
    for kind, library in (("runtime", "libxl.so"), ("xlc", "libxlc.so")):
        manifest = root / f"tools/tcc_vfs_manifest_linux_{kind}.txt"
        product = root / "release" / library
        resources = load_resources(manifest, root, build / "tcc-dist",
                                   build / "xrt.o", build)
        for path in (manifest, product, *[item.source for item in resources]):
            inputs[str(path)] = digest(path)
        lanes.append((kind, manifest, product, resources))
    for path in (Path(__file__).resolve(), root / "tools/gen_tcc_vfs_resources.py",
                 root / "test/test_linux_embedded_resources.py",
                 root / "lib/tcc/tcc_builtin_vfs.h", root / "build_linux.sh"):
        inputs[str(path)] = digest(path)
    results = []
    for kind, manifest, product, resources in lanes:
        library = ctypes.CDLL(str(product), mode=ctypes.RTLD_LOCAL)
        count = ctypes.c_uint.in_dll(library, "tcc_builtin_resources_count").value
        if count != len(resources):
            raise AssertionError(f"{kind}: product/manifest resource count mismatch")
        table = (BuiltinResource * count).in_dll(library, "tcc_builtin_resources")
        compressed = 0
        total = 0
        for index, expected in enumerate(resources):
            actual = table[index]
            source = expected.source.read_bytes()
            verify_resource(actual, expected.virtual, source)
            compressed += actual.method == 1
            total += actual.size
        results.append({"Product": str(product), "Resources": count,
                        "CompressedResources": compressed, "SourceBytes": total})
        print(f"{product.name}: {count} actual embedded resources match current source")
    for kind, manifest, product, resources in lanes:
        current = load_resources(manifest, root, build / "tcc-dist", build / "xrt.o", build)
        if [(item.virtual, item.source) for item in current] != [
                (item.virtual, item.source) for item in resources]:
            raise AssertionError(f"{kind}: resource inventory changed during verification")
    if any(digest(Path(path)) != expected for path, expected in inputs.items()):
        raise AssertionError("verification input changed during execution")
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps({"Complete": True, "InputsUnchanged": True,
        "Scope": "Linux actual runtime/compiler shared-product resource bytes; not self-hosting or 53-module acceptance",
        "Inputs": inputs, "Products": results}, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
