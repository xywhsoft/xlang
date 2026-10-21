"""Verify the pinned, unmodified offline CJK font and its distribution material."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import stat

FONT = "NotoSansMonoCJKsc-Regular.otf"
COMMIT = "f8d157532fbfaeda587e826d4cd5b21a49186f7c"
FONT_SHA = "ec04cc376b34887cedbdf84074e2e226ed2761eeabdcb9173fc1dd7bfd153ef7"
LICENSE_SHA = "6a73f9541c2de74158c0e7cf6b0a58ef774f5a780bf191f2d7ec9cc53efe2bf2"
NAMES = (FONT, "LICENSE", "COPYRIGHT.txt", "source.json")


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def verify(directory):
    root = Path(directory).absolute()
    if not root.is_dir() or root.lstat().st_file_attributes & 0x400:
        raise ValueError("Font directory must be an ordinary directory")
    if {p.name for p in root.iterdir()} != set(NAMES):
        raise ValueError("Pinned font resource inventory differs")
    for name in NAMES:
        data = (root / name).lstat()
        if not stat.S_ISREG(data.st_mode) or data.st_file_attributes & 0x400:
            raise ValueError("Font material must be an ordinary file: " + name)
        if not 0 < data.st_size <= (64 * 1024 * 1024 if name == FONT else 64 * 1024):
            raise ValueError("Invalid font material size: " + name)
    if sha(root / FONT) != FONT_SHA or sha(root / "LICENSE") != LICENSE_SHA:
        raise ValueError("Font or upstream license differs from pinned source")
    with (root / FONT).open("rb") as stream:
        if stream.read(4) != b"OTTO":
            raise ValueError("Expected the original OpenType CFF font")
    source = json.loads((root / "source.json").read_text(encoding="utf8"))
    if source["commit"] != COMMIT or source["modified"] is not False or source["license"] != "SIL Open Font License 1.1":
        raise ValueError("Font provenance differs")
    rows = {row["path"]: row for row in source["files"]}
    for name in (FONT, "LICENSE"):
        if rows[name]["sha256"] != sha(root / name) or rows[name]["bytes"] != (root / name).stat().st_size:
            raise ValueError("Font source receipt differs: " + name)
    copyright_text = (root / "COPYRIGHT.txt").read_text(encoding="utf8")
    if source["fontMetadata"]["0"] not in copyright_text or COMMIT not in copyright_text:
        raise ValueError("Font copyright notice is incomplete")
    return {"family": source["family"], "version": source["fontMetadata"]["5"],
            "license": source["license"], "commit": COMMIT, "modified": False,
            "files": [{"path": "data/fonts/" + name, "sha256": sha(root / name),
                       "size": (root / name).stat().st_size} for name in NAMES]}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True)
    parser.add_argument("--destination")
    args = parser.parse_args()
    materials = verify(args.source)
    if args.destination:
        target = Path(args.destination)
        target.mkdir(parents=True, exist_ok=True)
        if target.lstat().st_file_attributes & 0x400:
            raise ValueError("Font copy target is a reparse point")
        for name in NAMES:
            existing = target / name
            if existing.exists() and existing.lstat().st_file_attributes & 0x400:
                raise ValueError("Font copy target file is a reparse point")
            shutil.copy2(Path(args.source) / name, existing)
        if verify(target) != materials:
            raise ValueError("Copied font resource differs")
    print("Verified pinned CJK font, copyright, license and source receipt")
