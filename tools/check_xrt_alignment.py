"""Verify the shared XRT revision and exact core bytes in xlang and XGE."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def read(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check(root: Path, xge: Path, expected: str | None = None) -> dict:
    upstream = read(root / "lib/xrt/upstream.json")
    native = read(xge / "extlibs/xrt/upstream.json")
    snapshot = read(root / "lib/xrt/xrt.snapshot.json")
    revision = upstream["revision"]
    if upstream.get("dirty") or native.get("overlay_files"):
        raise ValueError("XRT alignment requires committed snapshots without source overlays")
    if native["source_commit"] != revision or (expected and revision != expected):
        raise ValueError("XRT revision mismatch")
    rows = []
    for kind, local, remote, key in (
        ("implementation", "lib/xrt/xrt.h", "extlibs/xrt/xrt.h", "header_sha256"),
        ("declarations", "release/include/xrt_decl.h", "extlibs/xrt/xrt_decl.h", "declaration_header_sha256"),
    ):
        local_path, remote_path = root / local, xge / remote
        identity = digest(local_path)
        if identity != snapshot[kind]["sha256"].lower() or identity != native[key].lower():
            raise ValueError(f"XRT {kind} provenance digest mismatch")
        if identity != digest(remote_path):
            raise ValueError(f"XRT {kind} bytes differ between repositories")
        if b"\r\n" in local_path.read_bytes() or b"\r\n" in remote_path.read_bytes():
            raise ValueError(f"XRT {kind} must use canonical LF bytes")
        rows.append(dict(kind=kind, sha256=identity, bytes=local_path.stat().st_size))
    return dict(status="passed", revision=revision, products=rows,
                scope="Source snapshots only; runtime instance and binary deployment require separate verification")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--xge-root", type=Path, required=True)
    parser.add_argument("--expected-ref")
    parser.add_argument("--xrt-root", type=Path,
                        help="Also require the source repository's clean HEAD to match the pin")
    args = parser.parse_args()
    try:
        result = check(args.root.resolve(), args.xge_root.resolve(), args.expected_ref)
        if args.xrt_root:
            revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=args.xrt_root, text=True).strip()
            dirty = subprocess.check_output(["git", "status", "--porcelain"], cwd=args.xrt_root)
            if revision != result["revision"] or dirty:
                raise ValueError("XRT source HEAD is different or its worktree is dirty")
            result["sourceRepositoryVerified"] = True
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        parser.exit(1, str(error) + "\n")
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
