#!/usr/bin/env python3
"""根据清单生成 TCC 静态 VFS 资源表。"""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import os
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Resource:
    virtual: str
    source: Path


@dataclass(frozen=True)
class PackedResource:
    virtual: str
    data: bytes
    packed: bytes
    method: str


def normalize_virtual_name(text: str) -> str:
    return text.replace("\\", "/").strip("/").lower()


def c_string(text: str) -> str:
    result: list[str] = []
    for byte in text.encode("utf-8"):
        if byte == ord("\\"):
            result.append("\\\\")
        elif byte == ord('"'):
            result.append('\\"')
        elif 32 <= byte < 127:
            result.append(chr(byte))
        else:
            result.append(f"\\{byte:03o}")
    return "".join(result)


def resolve_manifest_path(
    text: str,
    root: Path,
    tcc_dist: Path,
    xrt_obj: Path,
    output_dir: Path,
) -> Path:
    value = text
    replacements = {
        "%XL6_ROOT%": str(root),
        "%XL6_EMBED_TCC_DIST%": str(tcc_dist),
        "%XL6_EMBED_TCC_DIST_X64%": str(tcc_dist),
        "%XL6_XRT_OBJ%": str(xrt_obj),
        "%XL6_VFS_OUTPUT_DIR%": str(output_dir),
    }
    for token, replacement in replacements.items():
        value = value.replace(token, replacement)

    # 清单可能由 Windows 与 POSIX 构建共用，统一采用 Path 可识别的分隔符。
    value = value.replace("\\", "/")
    path = Path(value)
    return (path if path.is_absolute() else root / path).resolve()


def load_resources(
    manifest: Path,
    root: Path,
    tcc_dist: Path,
    xrt_obj: Path,
    output_dir: Path,
) -> list[Resource]:
    resources: list[Resource] = []
    seen: set[str] = set()

    def add(virtual_name: str, source: Path) -> None:
        virtual = normalize_virtual_name(virtual_name)
        if virtual in seen:
            return
        if not source.is_file():
            raise RuntimeError(f"missing VFS resource source: {source}")
        seen.add(virtual)
        resources.append(Resource(virtual, source.resolve()))

    for raw_line in manifest.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = [part.strip() for part in line.split("|")]
        if len(parts) < 3:
            raise RuntimeError(f"bad manifest line: {raw_line}")
        kind = parts[0].lower()
        if kind == "file":
            add(
                parts[1],
                resolve_manifest_path(parts[2], root, tcc_dist, xrt_obj, output_dir),
            )
            continue
        if kind != "dir":
            raise RuntimeError(f"unknown manifest kind: {kind}")

        virtual_prefix = normalize_virtual_name(parts[1])
        source_dir = resolve_manifest_path(parts[2], root, tcc_dist, xrt_obj, output_dir)
        if not source_dir.is_dir():
            raise RuntimeError(f"missing VFS resource directory: {source_dir}")
        patterns = [
            value.strip()
            for value in (parts[3].split(";") if len(parts) >= 4 else [])
            if value.strip()
        ]
        for source in sorted(path for path in source_dir.rglob("*") if path.is_file()):
            if patterns and not any(
                fnmatch.fnmatch(source.name, pattern) for pattern in patterns
            ):
                continue
            relative = source.relative_to(source_dir).as_posix()
            add(f"{virtual_prefix}/{relative}", source)
    return resources


@dataclass(frozen=True)
class Blob:
    data: bytes
    packed: bytes
    method: str


def build_catalog(resources, pack_tool, level, no_compress, mode="solid", block_size=262144):
    """Content aliases share extents. Solid blobs never exceed block_size."""
    files = [(r, r.source.read_bytes()) for r in resources]
    blocks, extents, aliases, unique = [], {}, {}, {}
    pending = bytearray()
    pending_rows = []
    families, variants = {}, {}

    def flush():
        if pending:
            index = len(blocks)
            blocks.append(bytes(pending))
            for name, start, size in pending_rows:
                extents[name] = [(index, start, size)]
            pending.clear()
            pending_rows.clear()

    for r, data in files:
        key = hashlib.sha256(data).digest()
        if mode != "individual" and key in unique:
            canonical, expected = unique[key]
            if data != expected:
                raise RuntimeError("SHA256 collision in resource inputs")
            aliases[r.virtual] = canonical
            continue
        unique[key] = (r.virtual, data)
        # Profiles differ mainly in ELF visibility bytes. Reuse exact byte
        # ranges of the canonical object; store only the changed middle range.
        # This is storage deduplication, never object metadata rewriting.
        if mode == "solid" and r.virtual.startswith("__xl_objects/"):
            family = r.virtual.split("/", 2)[-1]
            previous = families.get(family)
            if previous and len(previous[1]) == len(data) and data:
                name, before = previous
                first = next(i for i, (a, b) in enumerate(zip(before, data)) if a != b)
                last = next(i for i in range(len(data)-1, first-1, -1) if before[i] != data[i]) + 1
                if (last-first)*2 < len(data) and last-first <= block_size:
                    patch = "@patch/" + r.virtual
                    if len(pending) + last-first > block_size:
                        flush()
                    pending_rows.append((patch, len(pending), last-first))
                    pending.extend(data[first:last])
                    variants[r.virtual] = (name, first, last, patch)
                    continue
            families.setdefault(family, (r.virtual, data))
        if mode != "solid":
            flush()
            extents[r.virtual] = [(len(blocks), 0, len(data))]
            blocks.append(data)
        elif len(data) > block_size:
            flush()
            extents[r.virtual] = []
            for start in range(0, len(data), block_size):
                chunk = data[start:start + block_size]
                extents[r.virtual].append((len(blocks), 0, len(chunk)))
                blocks.append(chunk)
        elif not data:
            extents[r.virtual] = []
        else:
            if len(pending) + len(data) > block_size:
                flush()
            pending_rows.append((r.virtual, len(pending), len(data)))
            pending.extend(data)
    flush()
    def slice_spans(spans, start, end):
        result, position = [], 0
        for index, offset, size in spans:
            low, high = max(start, position), min(end, position+size)
            if high > low:
                result.append((index, offset+low-position, high-low))
            position += size
        return result
    for name, (canonical, first, last, patch) in variants.items():
        before = extents[canonical]
        length = sum(size for _, _, size in before)
        extents[name] = slice_spans(before, 0, first) + extents[patch] + slice_spans(before, last, length)
    for alias, canonical in aliases.items():
        extents[alias] = extents[canonical]
    # Chunk-level dedup also shares common precompiled profiles.
    blobs, lookup, remap = [], {}, {}
    with tempfile.TemporaryDirectory(prefix="xl-vfs-") as tmp:
        src, dst = Path(tmp)/"input", Path(tmp)/"packed"
        for i, data in enumerate(blocks):
            key = hashlib.sha256(data).digest()
            if mode != "individual" and key in lookup:
                j = lookup[key]
                if blobs[j].data != data:
                    raise RuntimeError("SHA256 collision in resource blocks")
                remap[i] = j
                continue
            packed, method = data, "TCC_BUILTIN_RESOURCE_STORE"
            if not no_compress and len(data) > 32:
                src.write_bytes(data)
                subprocess.run([str(pack_tool), str(src), str(dst), str(level)], check=True)
                candidate = dst.read_bytes()
                if len(candidate) < len(data):
                    packed, method = candidate, "TCC_BUILTIN_RESOURCE_LZMA"
            remap[i] = len(blobs)
            lookup[key] = len(blobs)
            blobs.append(Blob(data, packed, method))
    for name, spans in extents.items():
        extents[name] = [(remap[i], offset, size) for i, offset, size in spans]
    return files, blobs, extents


def write_catalog(output, resources, pack_tool, level, no_compress, mode, block_size):
    if len({r.virtual for r in resources}) != len(resources):
        raise RuntimeError("duplicate virtual resource name")
    files, blobs, extents = build_catalog(resources, pack_tool, level, no_compress, mode, block_size)
    lines = ['/* Generated VFS catalog: immutable, shared, bounded solid blocks. */',
             '#include "tcc_builtin_vfs.h"', '#if TCC_BUILTIN_CATALOG_VERSION != 2',
             '#error wrong_builtin_catalog_header', '#endif', '']
    for i, blob in enumerate(blobs):
        lines.append(f"static const unsigned char tcc_builtin_res_{i:04d}[] = {{")
        for offset in range(0, len(blob.packed), 16):
            lines.append("    " + ", ".join(f"0x{x:02x}" for x in blob.packed[offset:offset+16]) + ",")
        if not blob.packed: lines.append("    0")
        lines.append("};")
    lines.append("static const TCCBuiltinBlob tcc_builtin_blobs[] = {")
    for i, blob in enumerate(blobs):
        lines.append(f"    {{tcc_builtin_res_{i:04d}, {len(blob.packed)}u, {len(blob.data)}u, {blob.method}}},")
    if not blobs: lines.append("    {0}")
    lines.append("};")
    for i, (r, data) in enumerate(files):
        lines.append(f"static const TCCBuiltinSpan tcc_builtin_spans_{i}[] = {{")
        for j, offset, size in extents[r.virtual]:
            lines.append(f"    {{&tcc_builtin_blobs[{j}], {offset}u, {size}u}},")
        if not extents[r.virtual]: lines.append("    {0}")
        lines.append("};")
    lines.append("const TCCBuiltinResource tcc_builtin_resources[] = {")
    for i, (r, data) in enumerate(files):
        lines.append(f'    {{ "{c_string(r.virtual)}", tcc_builtin_spans_{i}, {len(extents[r.virtual])}u, {len(data)}u }},')
    if not files: lines.append("    {0}")
    lines.extend(["};", f"const unsigned int tcc_builtin_resources_count = {len(files)}u;", ""])
    output.parent.mkdir(parents=True, exist_ok=True)
    content = "\n".join(lines)
    output.write_text(content, encoding="utf-8", newline="\n")
    stamp = dict(schema=2, format=2, packing=mode, blockSize=block_size,
                 resourceCount=len(files), blobCount=len(blobs),
                 rawBytes=sum(len(data) for _, data in files),
                 decodedBlobBytes=sum(len(b.data) for b in blobs),
                 packedBytes=sum(len(b.packed) for b in blobs),
                 spanCount=sum(len(extents[r.virtual]) for r, _ in files),
                 generatedSha256=hashlib.sha256(output.read_bytes()).hexdigest(),
                 resources=[dict(virtual=r.virtual, source=str(r.source),
                     sha256=hashlib.sha256(data).hexdigest()) for r, data in files])
    Path(str(output)+".stamp.json").write_text(json.dumps(stamp, indent=2)+"\n", encoding="utf-8")
    print(f"generated {len(files)} resources in {len(blobs)} shared blobs: {stamp['rawBytes']} -> {stamp['packedBytes']} bytes ({mode})")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--tcc-dist", type=Path, required=True)
    parser.add_argument("--xrt-obj", type=Path, required=True)
    parser.add_argument("--pack-tool", type=Path, required=True)
    parser.add_argument("--pack-level", type=int, default=9)
    parser.add_argument("--no-compress", action="store_true")
    parser.add_argument("--precompiled", type=Path)
    parser.add_argument("--packing", choices=("individual", "dedup", "solid"), default=os.environ.get("XL6_VFS_PACKING", "solid"))
    parser.add_argument("--block-size", type=int, default=1048576)
    args = parser.parse_args()
    if args.block_size < 4096 or args.block_size > 1048576:
        parser.error("block size must be between 4 KiB and 1 MiB")

    root = args.root.resolve()
    manifest = args.manifest
    output = args.output
    tcc_dist = args.tcc_dist
    xrt_obj = args.xrt_obj
    pack_tool = args.pack_tool
    if not manifest.is_absolute():
        manifest = root / manifest
    if not output.is_absolute():
        output = root / output
    if not tcc_dist.is_absolute():
        tcc_dist = root / tcc_dist
    if not xrt_obj.is_absolute():
        xrt_obj = root / xrt_obj
    if not pack_tool.is_absolute():
        pack_tool = root / pack_tool

    resources = load_resources(
        manifest.resolve(),
        root,
        tcc_dist.resolve(),
        xrt_obj.resolve(),
        output.resolve().parent,
    )
    # Nested catalogs are serialized from shared physical blobs at compile time.
    resources = [r for r in resources if not r.virtual.endswith("/tcc_builtin_resources_runtime.c")]
    precompiled = args.precompiled or output.resolve().parent / "precompiled/manifest.json"
    compiler_catalog = any(r.virtual.startswith("__xl_compiler/") for r in resources)
    if compiler_catalog or any(r.virtual == "include/xl.h" for r in resources):
        if not precompiled.is_file():
            raise RuntimeError("language SDK requires precompile_builtin.py output: " + str(precompiled))
        manifest_data = json.loads(precompiled.read_text(encoding="utf-8"))
        from precompile_builtin import source_fingerprint
        if manifest_data["sources"] != source_fingerprint(root):
            raise RuntimeError("precompiled objects do not match current source tree")
        resources = [r for r in resources if not (r.virtual.startswith("__xl_compiler/") and r.source.suffix in (".c", ".inc"))]
        for row in manifest_data["objects"]:
            if not compiler_catalog and row.get("product") != "xrt":continue
            source = Path(row["source"])
            if hashlib.sha256(source.read_bytes()).hexdigest() != row["sha256"]:
                raise RuntimeError("stale precompiled object: " + str(source))
            resources.append(Resource(normalize_virtual_name(row["virtual"]), source))

    write_catalog(output.resolve(), resources, pack_tool.resolve(),
                  max(0, min(9, args.pack_level)), args.no_compress, args.packing, args.block_size)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
