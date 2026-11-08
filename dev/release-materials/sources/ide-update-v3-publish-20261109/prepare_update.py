#!/usr/bin/env python3
"""Create a content-addressed AutoTestIDE update repository.

The generated metadata is uploaded below the configured HTTP root:
    version.txt
    update.json
    update.lzma
    update/<sha256>.lzma

The local verN directory is only a collection of changed objects to copy into
the server's stable update directory. Publish those objects plus
update.json/update.lzma before version.txt. version.txt is the only activation
switch seen by existing clients.
"""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import lzma
import os
import shutil
import struct
import tempfile
from pathlib import Path


DEFAULT_IGNORES = [
    "backup/", "temp/", "update/", "update.json", "update.ignore",
    "screenshot.png", "screenshot_crop.png", "AutoTestIDE.res.o",
    "env/logs/", "env/task/autotestide_*.json", "env/**/__pycache__/",
    "env/librarys/log/", "env/scripts/", "option/option.json", "option/window.json",
    "option/layout.xson", "option/llm.json", "option/fim.json",
    "option/ai_sessions.json",
]

# Git hosting commonly rejects large blobs. The actual stored object is the
# LZMA stream, so split that stream only when it exceeds this limit.
OBJECT_PART_MAX_BYTES = 50 * 1024 * 1024


def normalize(path: Path) -> str:
    return path.as_posix().lstrip("./")


def read_rules(path: Path | None) -> list[str]:
    rules = list(DEFAULT_IGNORES)
    if path is None or not path.exists():
        return rules
    for raw in path.read_text(encoding="utf-8-sig").splitlines():
        line = raw.strip()
        if line and not line.startswith("#"):
            rules.append(line)
    return rules


def match_rule(rel: str, is_dir: bool, rule: str) -> bool:
    pattern = rule[1:] if rule.startswith("/") else rule
    dir_only = pattern.endswith("/")
    if dir_only:
        pattern = pattern.rstrip("/")
        parts = rel.split("/")
        directory_count = len(parts) if is_dir else len(parts) - 1
        if "/" not in pattern:
            return any(fnmatch.fnmatchcase(part, pattern) for part in parts[:directory_count])
        return any(
            fnmatch.fnmatchcase("/".join(parts[:index]), pattern)
            for index in range(1, directory_count + 1)
        )
    if fnmatch.fnmatchcase(rel, pattern) or fnmatch.fnmatchcase(rel + "/", pattern + "/"):
        return True
    return "/" not in pattern and any(fnmatch.fnmatchcase(part, pattern) for part in rel.split("/"))


def ignored(rel: str, is_dir: bool, rules: list[str]) -> bool:
    result = False
    for raw in rules:
        negate = raw.startswith("!")
        rule = raw[1:] if negate else raw
        if rule and match_rule(rel, is_dir, rule):
            result = not negate
    return result


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_atomic(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(delete=False, dir=path.parent) as handle:
        handle.write(data)
        temp_name = handle.name
    os.replace(temp_name, path)


def lzma_alone(data: bytes) -> bytes:
    """Create a native-updater-compatible LZMA-Alone payload."""
    encoded = bytearray(lzma.compress(data, format=lzma.FORMAT_ALONE, preset=6))
    # Python leaves this header field as an unknown length. update.exe rejects
    # unknown lengths before allocating the output buffer.
    encoded[5:13] = struct.pack("<Q", len(data))
    return bytes(encoded)


def object_suffix(compression: str) -> str:
    if compression not in {"lzma-alone", "none"}:
        raise ValueError(f"unsupported compression: {compression}")
    # Object paths are always binary .lzma names. The manifest controls
    # whether the stored bytes need LZMA decoding, avoiding text-mode uploads
    # of uncompressed source files.
    return ".lzma"


def part_number(path: Path, digest: str, suffix: str) -> int:
    prefix = f"{digest}-"
    name = path.name
    if not name.startswith(prefix) or not name.endswith(suffix):
        return -1
    try:
        return int(name[len(prefix):-len(suffix)])
    except ValueError:
        return -1


def existing_parts(archive_dir: Path, digest: str, suffix: str) -> list[Path]:
    parts = [path for path in archive_dir.glob(f"{digest}-*{suffix}") if part_number(path, digest, suffix) > 0]
    parts.sort(key=lambda path: part_number(path, digest, suffix))
    expected = 1
    for path in parts:
        if part_number(path, digest, suffix) != expected or path.stat().st_size <= 0 or path.stat().st_size > OBJECT_PART_MAX_BYTES:
            return []
        expected += 1
    return parts


def part_manifest(parts: list[Path]) -> dict:
    return {
        "archivePartCount": len(parts),
        "archiveParts": [
            {
                "size": path.stat().st_size,
                "sha256": sha256_file(path),
            }
            for path in parts
        ],
    }


def split_archive(archive: Path, digest: str, suffix: str) -> dict:
    parts: list[Path] = []
    with archive.open("rb") as source:
        index = 1
        while chunk := source.read(OBJECT_PART_MAX_BYTES):
            part = archive.with_name(f"{digest}-{index}{suffix}")
            write_atomic(part, chunk)
            parts.append(part)
            index += 1
    if len(parts) < 2:
        raise RuntimeError(f"split archive unexpectedly has {len(parts)} part(s): {archive}")
    for stale in existing_parts(archive.parent, digest, suffix):
        if part_number(stale, digest, suffix) > len(parts):
            stale.unlink()
    archive.unlink()
    return part_manifest(parts)


def copy_atomic(source: Path, destination: Path) -> None:
    temporary = destination.with_name(destination.name + ".tmp")
    with source.open("rb") as src, temporary.open("wb") as dst:
        shutil.copyfileobj(src, dst, length=1024 * 1024)
    os.replace(temporary, destination)


def create_object(source: Path, storage_root: Path, digest: str) -> tuple[str, dict]:
    compressed = storage_root / f"{digest}.lzma"
    storage_root.mkdir(parents=True, exist_ok=True)
    temporary = compressed.with_name(compressed.name + ".tmp")
    # FORMAT_ALONE is the compact LZMA stream directly supported by the
    # bundled 7-Zip LzmaDec SDK in update.exe (unlike the .xz container).
    with source.open("rb") as src, lzma.open(temporary, "wb", format=lzma.FORMAT_ALONE, preset=6) as dst:
        shutil.copyfileobj(src, dst, length=1024 * 1024)
    # Python writes an unknown uncompressed length for FORMAT_ALONE. The
    # native updater intentionally rejects that form before allocation, so
    # write the exact source length into the standard 13-byte header.
    with temporary.open("r+b") as stream:
        stream.seek(5)
        stream.write(struct.pack("<Q", source.stat().st_size))
    if temporary.stat().st_size < source.stat().st_size:
        compression = "lzma-alone"
        archive = compressed
        if archive.exists():
            temporary.unlink()
        else:
            os.replace(temporary, archive)
    else:
        compression = "none"
        archive = compressed
        temporary.unlink()
        copy_atomic(source, archive)
    suffix = object_suffix(compression)
    if archive.stat().st_size > OBJECT_PART_MAX_BYTES:
        return compression, split_archive(archive, digest, suffix)
    return compression, {}


def load_state(path: Path) -> tuple[dict[str, dict], set[str]]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        if data.get("schema") != 4:
            return {}, set()
        files = {
            entry["path"]: {key: value for key, value in entry.items() if key != "path"}
            for entry in data.get("files", [])
            if isinstance(entry, dict) and isinstance(entry.get("path"), str)
            and isinstance(entry.get("sha256"), str) and isinstance(entry.get("size"), int)
        }
        known_paths = {path for path in data.get("knownPaths", []) if isinstance(path, str)}
        return files, known_paths | set(files)
    except (OSError, ValueError, TypeError):
        return {}, set()


def previous_state(out_root: Path) -> tuple[dict[str, dict], set[str]]:
    return load_state(out_root / ".state" / "snapshot.json")


def uses_supported_object(entry: dict) -> bool:
    if entry.get("compression") not in {"lzma-alone", "none"} or "archive" in entry:
        return False
    parts = entry.get("archiveParts")
    if parts is None:
        return "archivePartCount" not in entry
    return isinstance(parts, list) and len(parts) >= 2 and entry.get("archivePartCount") == len(parts) and all(
        isinstance(part, dict) and "archive" not in part and isinstance(part.get("sha256"), str)
        and isinstance(part.get("size"), int) and part["size"] > 0
        for part in parts)


def build_manifest(build_root: Path, release_root: Path, version: int,
                   previous_files: dict[str, dict], known_paths: set[str], rules: list[str], notes: str) -> tuple[dict, dict[str, dict], int]:
    files: list[dict] = []
    current_files: dict[str, dict] = {}
    changed_count = 0
    for path in sorted(build_root.rglob("*")):
        if not path.is_file():
            continue
        rel = normalize(path.relative_to(build_root))
        if ignored(rel, False, rules):
            continue
        digest = sha256_file(path)
        state_entry = {"sha256": digest, "size": path.stat().st_size}
        previous_entry = previous_files.get(rel)
        if (previous_entry is not None and uses_supported_object(previous_entry)
                and previous_entry.get("sha256") == digest
                and previous_entry.get("size") == state_entry["size"]):
            entry = previous_entry
        else:
            compression, object_info = create_object(path, release_root, digest)
            entry = {
                **state_entry,
                "compression": compression,
                **object_info,
            }
            changed_count += 1
        current_files[rel] = entry
        files.append({"path": rel, **entry})
    # An ignored path is local state, not a remotely managed file. In
    # particular, a path that appeared in an older snapshot must not turn into
    # a manifest deletion merely because it was later added to update.ignore.
    # Otherwise the updater removes a user's local configuration on the next
    # release and the IDE recreates it with defaults at startup.
    managed_known_paths = {
        rel for rel in known_paths
        if not ignored(rel, False, rules)
    }
    manifest = {
        "schema": 4,
        "version": version,
        "notes": notes,
        "files": files,
        "delete": sorted(managed_known_paths - set(current_files)),
    }
    return manifest, current_files, changed_count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=Path("build"), help="IDE build directory")
    parser.add_argument("--output", type=Path, required=True, help="directory to upload")
    parser.add_argument("--version", type=int, required=True, help="monotonic release version")
    parser.add_argument("--notes", default="", help="release notes written to update.json")
    parser.add_argument("--ignore", type=Path, help="ignore file; defaults to repository update.ignore")
    args = parser.parse_args()
    build_root = args.build.resolve()
    out_root = args.output.resolve()
    if args.version < 1 or not build_root.is_dir():
        parser.error("--version must be positive and --build must exist")
    try:
        out_root.relative_to(build_root)
    except ValueError:
        pass
    else:
        parser.error("--output must not be inside --build")
    repo_root = Path(__file__).resolve().parents[1]
    ignore_path = args.ignore.resolve() if args.ignore else repo_root / "update.ignore"
    release_root = out_root / f"ver{args.version}"
    previous_files, known_paths = previous_state(out_root)
    rules = read_rules(ignore_path)
    manifest, current_files, changed_count = build_manifest(build_root, release_root, args.version,
                                                             previous_files, known_paths, rules, args.notes)
    manifest_bytes = (json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    # Keep JSON for inspection, but clients download the compressed form.
    write_atomic(out_root / "update.json", manifest_bytes)
    write_atomic(out_root / "update.lzma", lzma_alone(manifest_bytes))
    write_atomic(out_root / "version.txt", f"{args.version}\n".encode("ascii"))
    managed_known_paths = {
        rel for rel in known_paths
        if not ignored(rel, False, rules)
    }
    state_bytes = (json.dumps({"schema": 4, "version": args.version,
                               "knownPaths": sorted(managed_known_paths | set(current_files)),
                               "files": [{"path": path, **entry} for path, entry in sorted(current_files.items())]},
                              ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    write_atomic(out_root / ".state" / "snapshot.json", state_bytes)
    multipart_files = [entry for entry in manifest["files"] if "archiveParts" in entry]
    multipart_parts = sum(entry["archivePartCount"] for entry in multipart_files)
    print(
        f"prepared version {args.version}: {changed_count} changed files, {len(manifest['files'])} tracked files, "
        f"{len(multipart_files)} multipart objects ({multipart_parts} parts), "
        f"{len(manifest['delete'])} deletions"
    )
    print(f"copy the contents of {release_root} into the server update directory, then upload "
          f"{out_root / 'update.json'} and {out_root / 'update.lzma'}; upload version.txt last")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
