"""Prepare an experimental SDK in a new directory; never install or build it."""
import argparse
import hashlib
import json
import shutil
import stat
from pathlib import Path

IDE = Path(__file__).resolve().parents[1]
PATCH = IDE / "third_party/sdk_patches/large-source-experiment-v1"


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def plain(path, directory=False):
    info = path.lstat()
    if info.st_file_attributes & 0x400:
        raise ValueError(f"Reparse path refused: {path}")
    if not (stat.S_ISDIR(info.st_mode) if directory else stat.S_ISREG(info.st_mode)):
        raise ValueError(f"Unexpected path type: {path}")
    if not directory and info.st_nlink != 1:
        raise ValueError(f"Linked source refused: {path}")
    return info


def name(value):
    parts = value.split("/")
    if not value or any(not part or part in (".", "..") or
                        any(c in part for c in '\\:<>"|?*') or
                        part[-1] in ". " for part in parts):
        raise ValueError(f"Unsafe material path: {value}")
    return value


def ancestors(path):
    for parent in (path, *path.parents):
        plain(parent, True)


def prepare(source_root, output):
    source_root = source_root.absolute()
    output = output.absolute()
    ancestors(source_root)
    ancestors(PATCH)
    if output.exists():
        raise ValueError("Output must be a new directory")
    ancestors(output.parent)
    if output.is_relative_to(source_root) or output.is_relative_to(PATCH):
        raise ValueError("Output must be outside both input trees")
    manifest = read(PATCH / "patch-manifest.json")
    if manifest["format"] != "xlang-ide-experimental-sdk-patch-1" or not manifest["privateOnly"]:
        raise ValueError("Unexpected experimental patch manifest")
    descriptor = PATCH / "source-materials-before.json"
    if sha(descriptor) != manifest["beforeDescriptorSha256"]:
        raise ValueError("Original material descriptor changed")
    before = read(descriptor)
    if before["sourceManifestSha256"] != manifest["originalSourceManifestSha256"]:
        raise ValueError("Original source snapshot changed")
    if sha(IDE / "third_party/xge_sdk/xge-source-files.json") != manifest["originalSourceManifestSha256"]:
        raise ValueError("Pinned source snapshot no longer matches this experiment")
    if sha(IDE / "third_party/xge_sdk/xge.dll") != manifest["originalSdkDllSha256"]:
        raise ValueError("Pinned runtime no longer matches this experiment")
    rows = before["files"]
    if not 0 < len(rows) < 10000 or sum(row["size"] for row in rows) > 256 * 1024 * 1024:
        raise ValueError("Source material budget exceeded")
    expected = {}
    for row in rows:
        relative = name(row["path"])
        if relative.casefold() in expected:
            raise ValueError("Duplicate source material")
        expected[relative.casefold()] = row
        source = source_root / relative
        ancestors(source.parent)
        if plain(source).st_size != row["size"] or sha(source) != row["sha256"]:
            raise ValueError(f"Original source changed: {relative}")
    changes = set()
    for change in manifest["changes"]:
        relative = name(change["path"])
        if relative.casefold() in changes:
            raise ValueError("Duplicate patch target")
        changes.add(relative.casefold())
        row = expected.get(relative.casefold())
        if (row["sha256"] if row else None) != change["beforeSha256"]:
            raise ValueError(f"Patch input binding differs: {relative}")
        if row is None and (source_root / relative).exists():
            raise ValueError(f"New source name already exists: {relative}")
        payload = PATCH / name(change["material"])
        ancestors(payload.parent)
        if plain(payload).st_size != change["size"] or sha(payload) != change["afterSha256"]:
            raise ValueError(f"Patch payload changed: {relative}")
    # All validation precedes the creation of the new output tree.
    output.mkdir()
    source = output / "source"
    source.mkdir()
    for row in rows:
        target = source / row["path"]
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_root / row["path"], target)
    for change in manifest["changes"]:
        target = source / change["path"]
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(PATCH / change["material"], target)
    final = {row["path"]: row["sha256"] for row in rows}
    final.update({row["path"]: row["afterSha256"] for row in manifest["changes"]})
    for relative, digest in final.items():
        if sha(source / relative) != digest:
            raise ValueError(f"Prepared source differs: {relative}")
    for row in rows:
        if sha(source_root / row["path"]) != row["sha256"]:
            raise ValueError(f"Input changed while copying: {row['path']}")
    report = {"status": "experimental-sdk-source-prepared", "source": str(source),
              "patchManifestSha256": sha(PATCH / "patch-manifest.json"),
              "files": [{"path": relative, "sha256": digest} for relative, digest in sorted(final.items())],
              "inputUnchanged": True, "sdkInstalled": False, "built": False,
              "productionAccepted": False, "formalRelease": False}
    (output / "source-preparation-report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = prepare(args.source_root, args.output)
    print(json.dumps({"status": report["status"], "source": report["source"],
                      "files": len(report["files"])}, ensure_ascii=False))


if __name__ == "__main__":
    main()
