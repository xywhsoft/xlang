"""Collect source and notice materials for the exact pinned SDK, without building it."""
from pathlib import Path


def collect(source_root, descriptor, sdk, *, add, load_json, plain, safe_name, sha, require):
    descriptor = Path(descriptor)
    data = load_json(descriptor)
    require(data.get("format") == "xlang-ide-sdk-materials-1", "Invalid SDK materials descriptor")
    sdk = Path(sdk)
    source_manifest = sdk / "xge-source-files.json"
    require(sha(source_manifest) == data["sourceManifestSha256"], "SDK materials source manifest differs")
    sdk_info = load_json(sdk / "sdk_manifest.json")
    dll = next(row["sha256"] for row in sdk_info["files"] if row["path"] == "xge.dll")
    require(dll == data["sdkDllSha256"] == sha(sdk / "xge.dll"), "SDK materials DLL differs")
    expected = {row["path"]: row["sha256"] for row in load_json(source_manifest)["files"]}
    entries = data["files"]
    require(isinstance(entries, list) and 0 < len(entries) < 10000, "Invalid SDK material file count")
    selected = {}
    for row in entries:
        name = safe_name(row["path"])
        require(name.casefold() not in selected, "Duplicate SDK material path")
        require(expected.get(name) == row["sha256"], "SDK material is outside pinned source inventory")
        require(type(row["size"]) is int and 0 <= row["size"] <= 256 * 1024 * 1024, "Invalid SDK material size")
        selected[name.casefold()] = row
    dependencies = {safe_name(name) for name in data["preprocessorDependencies"]}
    require(dependencies and all(name.casefold() in selected for name in dependencies), "SDK preprocessing closure missing")
    require(set(data["translationUnits"]).issubset(dependencies), "SDK translation unit missing from closure")
    require(all(name.casefold() in selected for name in data["buildEntrypoints"]), "SDK build entrypoint missing")
    # Verify every designated source, including all named notices and embedded-notice
    # headers, before registering any input for the package staging directory.
    root = Path(source_root).absolute() if source_root else None
    if root:
        plain(root, True)
        for row in entries:
            source = root / row["path"]
            require(source.resolve().is_relative_to(root.resolve()), "SDK material source escaped root")
            for parent in source.parents:
                if parent == root:
                    break
                plain(parent, True)
            require(plain(source).st_size == row["size"] and sha(source) == row["sha256"],
                    f"SDK material differs from pinned source: {row['path']}")
    materials = []
    for row in data["materials"]:
        name = safe_name(row["source"])
        require(name.casefold() in selected, "SDK notice source missing from selected inventory")
        item = {"component": row["component"], "source": name, "kind": row["kind"],
                "sha256": selected[name.casefold()]["sha256"], "status": "pinned-source-root-not-supplied"}
        if root:
            item.update({"path": "sources/xge/" + name, "status": "matches-pinned-sdk-source"})
            if row["kind"] == "named-notice":
                target = "notices/xge/" + name
                add(root / name, target, item["sha256"])
                item["path"] = target
        materials.append(item)
    add(descriptor, "provenance/sdk-materials.json")
    if root:
        for row in entries:
            add(root / row["path"], "sources/xge/" + row["path"], row["sha256"])
    return {"status": "pinned-sdk-source-materials-verified" if root else "pinned-sdk-source-materials-unavailable",
            "materials": materials, "sourceFiles": len(entries) if root else 0,
            "sourceBytes": sum(row["size"] for row in entries) if root else 0,
            "preprocessorDependencyFiles": len(dependencies), "translationUnits": len(data["translationUnits"]),
            "sourceScope": data["sourceScope"], "sourceManifestSha256": data["sourceManifestSha256"],
            "sdkDllSha256": dll, "legalDistributionReviewComplete": False}
