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



class BuiltinResource(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char_p),
        ("data", ctypes.POINTER(ctypes.c_ubyte)),
        ("packed_size", ctypes.c_size_t),
        ("size", ctypes.c_size_t),
        ("method", ctypes.c_uint),
    ]


class IndexedResource(ctypes.Structure):
    _fields_ = [("size", ctypes.c_uint64), ("name_offset", ctypes.c_uint32),
                ("first_span", ctypes.c_uint32), ("span_count", ctypes.c_uint32)]


class Blob(ctypes.Structure):
    _fields_ = [("data", ctypes.POINTER(ctypes.c_ubyte)),
                ("packed_size", ctypes.c_uint32), ("size", ctypes.c_uint32),
                ("method", ctypes.c_uint32)]


class Span(ctypes.Structure):
    _fields_ = [("blob_index", ctypes.c_uint32), ("offset", ctypes.c_uint32),
                ("size", ctypes.c_uint32)]


class Catalog:
    def __init__(self, library):
        def count(name):
            return ctypes.c_uint.in_dll(library, "tcc_builtin_"+name).value
        self.names = bytes((ctypes.c_ubyte*count("names_size")).in_dll(library, "tcc_builtin_names"))
        self.blobs = (Blob*count("blobs_count")).in_dll(library, "tcc_builtin_blobs")
        self.spans = (Span*count("spans_count")).in_dll(library, "tcc_builtin_spans")
        self.resources = (IndexedResource*count("resources_count")).in_dll(library, "tcc_builtin_resources")
        self.cache = {}

    def name(self, resource):
        start = resource.name_offset
        if start >= len(self.names) or self.names.find(b"\0", start) < 0:
            raise AssertionError("invalid catalog name extent")
        return self.names[start:self.names.index(0, start)].decode("utf-8")

    def read(self, resource):
        start, count = resource.first_span, resource.span_count
        if start > len(self.spans) or count > len(self.spans)-start:
            raise AssertionError("invalid catalog span extent")
        chunks = []
        for span in self.spans[start:start+count]:
            if span.blob_index >= len(self.blobs):
                raise AssertionError("invalid catalog blob index")
            blob = self.blobs[span.blob_index]
            if (blob.size > 1048576 or blob.packed_size > blob.size
                    or span.offset > blob.size or span.size > blob.size-span.offset):
                raise AssertionError("invalid bounded catalog blob/span extent")
            if span.blob_index not in self.cache:
                self.cache[span.blob_index] = decode(ctypes.string_at(blob.data, blob.packed_size),
                                                    blob.size, blob.method)
            chunks.append(self.cache[span.blob_index][span.offset:span.offset+span.size])
        data = b"".join(chunks)
        if len(data) != resource.size:
            raise AssertionError("catalog logical size mismatch")
        return data


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
    try:
        data = reader.decompress(alone, max_length=size + 1)
    except lzma.LZMAError:
        # Independent test encoders may write an end marker. liblzma expects
        # unknown length for that stream; exact length/EOF/trailing checks below
        # still apply. The production SDK packer omits the end marker.
        reader = lzma.LZMADecompressor(format=lzma.FORMAT_ALONE)
        data = reader.decompress(packed[:5] + b"\xff"*8 + packed[5:], max_length=size + 1)
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
        stamp_path = build / ("tcc_builtin_resources_runtime.c.stamp.json" if kind == "runtime"
                              else "tcc_builtin_resources.c.stamp.json")
        stamp = json.loads(stamp_path.read_text(encoding="utf-8"))
        if stamp.get("format") != 3:
            raise AssertionError("expected indexed v3 catalog receipt")
        resources = stamp["resources"]
        for path in (manifest, product, stamp_path, *[Path(item["source"]) for item in resources]):
            inputs[str(path)] = digest(path)
        lanes.append((kind, manifest, product, resources))
    for path in (Path(__file__).resolve(), root / "tools/gen_tcc_vfs_resources.py",
                 root / "test/test_linux_embedded_resources.py",
                 root / "lib/tcc/tcc_builtin_vfs.h", root / "build_linux.sh"):
        inputs[str(path)] = digest(path)
    results = []
    for kind, manifest, product, resources in lanes:
        library = ctypes.CDLL(str(product), mode=ctypes.RTLD_LOCAL)
        catalog = Catalog(library)
        if len(catalog.resources) != len(resources):
            raise AssertionError(f"{kind}: product/manifest resource count mismatch")
        names = [catalog.name(resource) for resource in catalog.resources]
        if names != sorted(names, key=lambda name: name.encode("utf-8")) or len(set(names)) != len(names):
            raise AssertionError("catalog lookup directory is not strictly sorted")
        total = 0
        for actual, expected in zip(catalog.resources, resources):
            source = Path(expected["source"]).read_bytes()
            if (catalog.name(actual) != expected["virtual"]
                    or hashlib.sha256(source).hexdigest() != expected["sha256"]
                    or catalog.read(actual) != source):
                raise AssertionError(f"stale product resource: {expected['virtual']}")
            total += actual.size
        count = len(resources)
        results.append({"Product": str(product), "Resources": count,
                        "CompressedBlobs": sum(blob.method == 1 for blob in catalog.blobs),
                        "SourceBytes": total})
        print(f"{product.name}: {count} actual embedded resources match current source")
    if any(digest(Path(path)) != expected for path, expected in inputs.items()):
        raise AssertionError("verification input changed during execution")
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps({"Complete": True, "InputsUnchanged": True,
        "Scope": "Linux actual runtime/compiler shared-product resource bytes; not self-hosting or 53-module acceptance",
        "Inputs": inputs, "Products": results}, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
