"""Exercise pinned SDK material acceptance and rejection without changing providers."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import shutil
import uuid

from package_portable import PackageError, load_json, plain, require, safe_name, sha, write_json
from sdk_release_materials import collect


def check(root):
    ide = Path(__file__).resolve().parents[1]
    sdk = ide / "third_party/xge_sdk"
    descriptor = ide / "data/xge-sdk-materials.json"
    proof = ide / "build/tests/sdk-materials" / uuid.uuid4().hex
    proof.mkdir(parents=True)
    results = []
    def run(source, spec=descriptor, provider=sdk):
        inputs = []
        def add(path, target, digest=None):
            require(not digest or sha(path) == digest, "Registered material source changed")
            inputs.append({"source": str(path), "target": target, "sha256": sha(path)})
        info = collect(source, spec, provider, add=add, load_json=load_json, plain=plain,
                       safe_name=safe_name, sha=sha, require=require)
        return info, inputs
    def accept(name, ok):
        require(ok, name)
        results.append({"case": name, "status": "passed"})
    def reject(name, fn, text):
        try:
            fn()
        except (PackageError, FileNotFoundError) as error:
            require(text in str(error), f"Wrong rejection for {name}: {error}")
            results.append({"case": name, "status": "rejected", "reason": str(error)})
        else:
            raise PackageError(f"Accepted invalid SDK materials: {name}")

    spec = load_json(descriptor)
    info, inputs = run(root)
    named = sum(row['kind'] == 'named-notice' for row in spec['materials'])
    accept("exact-pinned-snapshot", info["sourceFiles"] == len(spec['files'])
           and info["translationUnits"] == len(spec['translationUnits'])
           and info["preprocessorDependencyFiles"] == len(spec['preprocessorDependencies'])
           and len(inputs) == len(spec['files']) + named + 1)
    accept("all-embedded-and-named-materials", len(info["materials"]) == len(spec['materials'])
           and sum(item["kind"] == "named-notice" for item in info["materials"]) == named)
    accept("libunibreak-licence-retained", any(row["target"].endswith("libunibreak/LICENCE") for row in inputs))
    accept("no-unrelated-font-or-current-xrt-license", not any(".ttf" in row["target"].lower()
           or "xrt-inspection" in row["target"] for row in inputs))
    accept("legal-review-remains-pending", info["legalDistributionReviewComplete"] is False)
    missing, absent_inputs = run(None)
    accept("missing-source-is-explicit", missing["sourceFiles"] == 0 and len(absent_inputs) == 1
           and all(row["status"] == "pinned-source-root-not-supplied" for row in missing["materials"]))
    private = proof / "source"
    for row in spec["files"]:
        target = private / row["path"]
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(Path(root) / row["path"], target)
    victim = private / next(row['path'] for row in spec['files'] if row['path'].endswith('/miniaudio/miniaudio.h'))
    original = victim.read_bytes()
    victim.write_bytes(bytes([original[0] ^ 1]) + original[1:])
    reject("same-size-changed-header", lambda: run(private), "differs from pinned source")
    victim.write_bytes(original + b"changed")
    reject("changed-source-size", lambda: run(private), "differs from pinned source")
    victim.write_bytes(original)
    missing_file = private / next(row['source'] for row in spec['materials'] if row['source'].endswith('/libunibreak/LICENCE'))
    missing_bytes = missing_file.read_bytes()
    missing_file.unlink()
    reject("missing-notice", lambda: run(private), "LICENCE")
    missing_file.write_bytes(missing_bytes)
    for name, edit, expected in (
        ("different-dll", lambda value: value.update(sdkDllSha256="0" * 64), "DLL differs"),
        ("different-source-manifest", lambda value: value.update(sourceManifestSha256="0" * 64), "source manifest differs"),
        ("dependency-omitted", lambda value: value["files"].remove(next(row for row in value["files"] if row["path"] == spec['translationUnits'][0])), "closure missing"),
        ("entrypoint-omitted", lambda value: value["files"].remove(next(row for row in value["files"] if row["path"] == spec['buildEntrypoints'][0])), "build entrypoint missing"),
        ("unsafe-source-path", lambda value: value["files"][0].update(path="../outside.h"), "Invalid path"),
        ("unknown-source-hash", lambda value: value["files"][0].update(sha256="0" * 64), "outside pinned source inventory"),
        ("case-duplicate", lambda value: value["files"].append(value["files"][0].copy()), "Duplicate SDK material path"),
        ("notice-omitted", lambda value: value["materials"][0].update(source="missing/LICENSE"), "notice source missing"),
    ):
        value = json.loads(json.dumps(spec))
        edit(value)
        modified = proof / (name + ".json")
        write_json(modified, value)
        reject(name, lambda: run(private, modified), expected)
    restored, _ = run(private)
    accept("restored-private-source", restored == info)
    report = {"status": "passed", "utc": datetime.now(timezone.utc).isoformat(), "proof": str(proof),
              "sourceRoot": str(root), "descriptorSha256": sha(descriptor), "sdkMaterials": info,
              "checks": len(results), "cases": results, "inputs": inputs, "formalRelease": False}
    write_json(proof / "evidence.json", report)
    print(json.dumps({"path": str(proof / "evidence.json"), "checks": len(results)}, ensure_ascii=False))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-source-root", required=True)
    check(parser.parse_args().sdk_source_root)
