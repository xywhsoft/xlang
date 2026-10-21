"""Terminate owned native migration processes and verify real startup recovery."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import time
import uuid

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def load(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))

def check(fixture, output=None, baseline=False):
    ide = Path(__file__).resolve().parents[1]
    fixture = Path(fixture).resolve(strict=True)
    proof = Path(output).absolute() if output else ide / "build/tests/migration-journal" / uuid.uuid4().hex
    if not proof.resolve().is_relative_to((ide / "build").resolve()):
        raise ValueError("Migration evidence must stay in ide/build")
    proof.mkdir(parents=True)
    cases = []

    def invoke(root, scenario, operation, phase=None, reject=False):
        label = operation + ("-rejected" if reject else "")
        out, err = root / (label + ".stdout"), root / (label + ".stderr")
        args = [str(fixture), str(root), scenario, operation]
        stopped = False
        with out.open("xb") as stdout, err.open("xb") as stderr:
            process = subprocess.Popen(args, stdout=stdout, stderr=stderr)
            try:
                deadline = time.monotonic() + 45
                while process.poll() is None and time.monotonic() < deadline:
                    marker = root / "barrier.json"
                    if phase and marker.is_file():
                        value = load(marker)
                        assert value["pid"] == process.pid and value["phase"] == phase
                        process.terminate()
                        stopped = True
                        break
                    time.sleep(0.04)
                process.wait(timeout=max(1, deadline - time.monotonic()))
                assert stopped if phase else ((process.returncode != 0) if reject else (process.returncode == 0)), (scenario, operation, process.returncode, err.read_text(errors="replace"))
            finally:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=10)
        return {"arguments": args, "pid": process.pid, "exit": process.returncode, "terminated": stopped,
                "stdout": str(out), "stdoutSha256": sha(out), "stderr": str(err), "stderrSha256": sha(err)}

    positives = [
        "dir-internal-prepared", "dir-internal-physical", "dir-internal-after",
        "dir-external-physical", "dir-solo-physical", "manifest-external-prepared",
        "manifest-external-physical", "manifest-solo-physical", "manifest-solo-after",
        "dir-internal-default-physical", "dir-solo-default-physical",
        "manifest-solo-default-physical", "dir-internal-case-only-physical",
        "dir-external-chain-prepared", "dir-external-chain-physical",
        "dir-internal-later", "manifest-external-later", "dir-internal-saved",
        "dir-internal-newer-later", "manifest-solo-newer-later",
        "dir-unnamed-prepared", "dir-unnamed-physical", "dir-unnamed-after",
        "manifest-unnamed-prepared", "manifest-unnamed-physical", "manifest-unnamed-after",
        "dir-unnamed-default-physical", "manifest-unnamed-default-physical",
        "dir-unnamed-newer-later", "manifest-unnamed-newer-later",
        "dir-unnamed-store-later", "manifest-unnamed-store-later",
        "dir-unnamed-quota-later", "manifest-unnamed-quota-later",
    ]
    if baseline:
        positives = ["dir-internal-physical", "manifest-external-physical"]
    for scenario in positives:
        root = proof / scenario
        root.mkdir()
        phase = next((p for p in ("prepared", "physical", "after") if p in scenario), None)
        calls = [invoke(root, scenario, "operate", phase)]
        request = load(root / "request.json")
        if "store" in scenario or "quota" in scenario:
            completed = load(root / "result.json")
            assert completed["storeBefore"] > 0 and completed["storeBefore"] == completed["storeRepeated"], (scenario, completed)
            assert completed["storeChanged"] == completed["storeBefore"] + 1, (scenario, completed)
            if "store" in scenario:
                assert completed["storeRepeatedChanged"] == completed["storeChanged"], (scenario, completed)
            else:
                unknown = next((root / "sessions/migrations").glob("*/recovery/unknown-retained.bin"))
                assert unknown.stat().st_size == 512 * 1024 * 1024
        if baseline:
            calls.append(invoke(root, scenario, "inspect-new"))
            state = load(root / "inspect-new.json")
            assert state["property"] == 0
            cases.append({"scenario": scenario, "calls": calls, "recovered": state})
            continue
        # Recovery reads user source/manifests; it must not implicitly save them.
        before = {str(p): sha(p) for p in root.rglob("*") if p.is_file() and p.suffix in (".xlp", ".xlw", ".xl", ".c")}
        inspection = "inspect-new" if "saved" in scenario else "inspect-old"
        calls.append(invoke(root, scenario, inspection))
        state = load(root / (inspection + ".json"))
        for key in ("xlangBody", "outsideBody", "untitledBody", "property", "cLanguage", "xlangLanguage", "cOverride"):
            assert state[key] == 1, (scenario, key, state)
        assert state["laterBody" if "later" in scenario else "cBody"] == 1, (scenario, state)
        assert state["projectId"] == request["projectId"] and state["cId"] == request["cId"]
        if "later" not in scenario:
            assert state["selection"] == 1 and state["activeId"] == request["cId"], (scenario, state)
        if "prepared" not in scenario or "chain" in scenario:
            expected = request["newManifest"]
            if "chain-physical" in scenario:
                expected = str(root / "Second项目😀.xlp").replace("\\", "/")
            assert state["manifest"] == expected, (scenario, expected, state)
        else:
            assert state["manifest"] == request["oldManifest"]
        assert not state["issue"], (scenario, state)
        calls.append(invoke(root, scenario, "inspect-recent"))
        recent = load(root / "inspect-recent.json")
        for key in ("xlangBody", "outsideBody", "untitledBody", "property", "cLanguage", "xlangLanguage", "cOverride"):
            assert recent[key] == 1, (scenario, key, recent)
        assert recent["laterBody" if "later" in scenario else "cBody"] == 1, (scenario, recent)
        assert recent["projectId"] == request["projectId"] and recent["cId"] == request["cId"]
        if scenario == "dir-internal-physical":
            calls.append(invoke(root, scenario, "inspect-edit"))
            calls.append(invoke(root, scenario, "inspect-maintain"))
        if "unnamed" in scenario:
            assert state["workspace"] == state["projectOwner"] == "" and state["workspaceDirty"] == 1
        for path, digest in before.items():
            assert sha(path) == digest, path
        cases.append({"scenario": scenario, "calls": calls, "recovered": state, "unchangedUserFiles": before})
        print("Passed migration", scenario, flush=True)

    if not baseline:
        root = proof / "reject-live-writer"
        root.mkdir()
        scenario = "dir-internal-prepared"
        out, err = root / "operate.stdout", root / "operate.stderr"
        args = [str(fixture), str(root), scenario, "operate"]
        with out.open("xb") as stdout, err.open("xb") as stderr:
            process = subprocess.Popen(args, stdout=stdout, stderr=stderr)
            try:
                deadline = time.monotonic() + 40
                while process.poll() is None and time.monotonic() < deadline and not (root / "barrier.json").is_file():
                    time.sleep(0.04)
                assert process.poll() is None and load(root / "barrier.json")["pid"] == process.pid
                retained = {str(p): sha(p) for p in root.rglob("*") if p.is_file() and not p.name.endswith((".stdout", ".stderr"))}
                inspected = invoke(root, scenario, "inspect-old", reject=True)
                assert process.poll() is None
                for path, digest in retained.items():
                    assert sha(path) == digest
            finally:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=10)
        cases.append({"scenario": "reject-live-writer", "calls": [
            {"arguments": args, "pid": process.pid, "exit": process.returncode, "terminated": True,
             "stdout": str(out), "stdoutSha256": sha(out), "stderr": str(err), "stderrSha256": sha(err)}, inspected], "preservedFiles": retained})
        print("Passed migration reject live writer", flush=True)
        for mutation in ("marker", "blob", "target-identity", "workspace", "project", "wrong-owner"):
            scenario = "dir-internal-physical"
            root = proof / ("reject-" + mutation)
            root.mkdir()
            calls = [invoke(root, scenario, "operate", "physical")]
            journal = next((root / "sessions/migrations").glob("*/migration-journal.json"))
            value = load(journal)
            if mutation == "marker":
                changed = journal
                changed.write_text("{damaged", encoding="utf8")
            elif mutation == "blob":
                changed = journal.parent / value["blobs"][0]["ref"]
                changed.write_bytes(b"damaged-body")
            elif mutation == "target-identity":
                changed = Path(value["target"])
                original = changed.with_name(changed.name + "-original")
                changed.rename(original)
                changed.mkdir()
            elif mutation == "workspace":
                changed = Path(value["model"]["manifest"])
                changed.write_text('{"format":"xlang-workspace-1","name":"external-change"}', encoding="utf8")
            elif mutation == "project":
                changed = Path(value["model"]["projects"][0]["manifest"])
                changed.write_text('{"format":"xlang-project-1","name":"external-change"}', encoding="utf8")
            else:
                changed = journal
                value["ownerAfter"] = str(root / "unrelated.xlw").replace("\\", "/")
                value["owners"] = [value["ownerAfter"]]
                journal.write_text(json.dumps(value, ensure_ascii=False), encoding="utf8")
            retained = {str(p): sha(p) for p in root.rglob("*") if p.is_file() and not p.name.endswith((".stdout", ".stderr"))}
            calls.append(invoke(root, scenario, "inspect-old", reject=True))
            for path, digest in retained.items():
                assert sha(path) == digest, path
            cases.append({"scenario": "reject-" + mutation, "calls": calls, "preservedFiles": retained})
            print("Passed migration reject", mutation, flush=True)
    result = {"utc": datetime.now(timezone.utc).isoformat(), "fixture": str(fixture), "fixtureSha256": sha(fixture),
              "baseline": baseline, "checks": len(cases), "cases": cases,
              "terminatedProcesses": sum(c["terminated"] for row in cases for c in row["calls"]),
              "projectMigrationTransactionRecoveryComplete": not baseline, "formalRelease": False, "goalStatus": "active"}
    result["files"] = [{"path": str(p), "sha256": sha(p)} for p in sorted(proof.rglob("*")) if p.is_file()]
    (proof / "evidence.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf8")
    print("Migration journal evidence", proof / "evidence.json", flush=True)
    return result

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture", required=True)
    parser.add_argument("--output")
    parser.add_argument("--baseline", action="store_true")
    args = parser.parse_args()
    check(args.fixture, args.output, args.baseline)
