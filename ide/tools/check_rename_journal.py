"""Prove durable filename transactions using owned, interrupted native processes."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
import uuid

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def load(path): return json.loads(Path(path).read_text(encoding="utf-8-sig"))

def check(fixture, output=None):
    ide = Path(__file__).resolve().parents[1]
    fixture = Path(fixture).resolve(strict=True)
    proof = Path(output).absolute() if output else ide / "build/tests/rename-journal" / uuid.uuid4().hex
    assert proof.resolve().is_relative_to((ide / "build").resolve())
    proof.mkdir(parents=True)
    cases = []
    def invoke(root, mode, barrier=None):
        out, err = root / (mode + ".stdout"), root / (mode + ".stderr")
        args = [str(fixture), str(root), mode]
        stopped = False
        with out.open("xb") as stdout, err.open("xb") as stderr:
            process = subprocess.Popen(args, stdout=stdout, stderr=stderr)
            try:
                end = time.monotonic() + 40
                marker = root / "barrier.json"
                while process.poll() is None and time.monotonic() < end:
                    if barrier and marker.is_file():
                        row = load(marker)
                        assert row["pid"] == process.pid and row["phase"] == barrier
                        process.terminate()
                        stopped = True
                        break
                    time.sleep(0.04)
                if process.poll() is None: process.wait(timeout=max(1, end - time.monotonic()))
                assert stopped if barrier else process.returncode == 0, (mode, process.returncode, err.read_text(errors="replace"))
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=10)
        return {"arguments": args, "exitCode": process.returncode, "terminatedAtBarrier": stopped,
                "stdout": str(out), "stdoutSha256": sha(out), "stderr": str(err), "stderrSha256": sha(err)}
    modes = ("journal", "chain-before", "chain-physical", "case-physical", "edit-after", "undo", "saved-references", "save-all", "post-conflict", "newer-checkpoint", "newer-undo", "newer-pending", "finish-failure-undo")
    for mode in modes:
        root = proof / mode
        root.mkdir()
        command = invoke(root, mode, mode if mode in ("journal", "chain-before", "chain-physical", "case-physical") else None)
        original = load(root / ("barrier.json" if command["terminatedAtBarrier"] else "result.json"))
        marker = Path(original["sessionDirectory"]) / "rename-journal.json"
        saved_marker = sha(marker)
        inspection = invoke(root, "inspect")
        recovered = load(root / "inspection.json")
        assert recovered["xlangBody"] == recovered["untitledBody"] == 1
        if mode in ("edit-after", "newer-checkpoint"): assert recovered["cLater"] == 1
        else: assert recovered["cBody"] == recovered["activeC"] == 1
        if mode in ("edit-after", "newer-checkpoint"): assert recovered["resourceLater"] == 1 and recovered["cBody"] == 0
        elif mode == "journal": assert recovered["workspaceRenamed"] == 0 and recovered["resourceBefore"] == 1 and recovered["sourceExists"] == 1
        elif mode == "chain-before": assert recovered["workspaceFirst"] == recovered["resourceFirst"] == recovered["unsavedProperty"] == 1 and recovered["targetExists"] == 0
        elif mode == "case-physical": assert recovered["workspaceUpper"] == recovered["resourceUpper"] == recovered["workspaceDirty"] == recovered["projectDirty"] == 1
        elif mode in ("undo", "newer-undo", "finish-failure-undo"): assert recovered["workspaceRenamed"] == recovered["projectRenamed"] == recovered["resourceBefore"] == 1 and recovered["resourcePatched"] == 0
        else: assert recovered["workspaceRenamed"] == recovered["projectRenamed"] == recovered["resourcePatched"] == recovered["unsavedProperty"] == 1
        if mode in ("saved-references", "save-all"): assert recovered["workspaceDirty"] == recovered["projectDirty"] == 0
        if mode == "save-all": assert load(marker)["state"] == "retired"
        if mode == "post-conflict": assert (marker.parent / "session.json").read_bytes() == b"{conflict" and recovered["sessionIssue"]
        if mode.startswith("newer-"): assert original["errorCode"] and saved_marker == sha(marker)
        if mode == "finish-failure-undo": assert load(marker)["state"] == "prepared" and saved_marker == sha(marker)
        row = {"case": mode, "command": command, "before": original, "inspectionCommand": inspection,
               "inspection": recovered, "journalBeforeSha256": saved_marker, "journalAfterSha256": sha(marker)}
        if mode == "chain-physical":
            row["repeatCommand"] = invoke(root, "inspect-repeat")
            again = load(root / "inspect-repeat.json")
            assert again["workspaceRenamed"] == again["projectRenamed"] == again["resourcePatched"] == 1
            before_files = {str(p): sha(p) for p in marker.parent.rglob("*") if p.is_file()}
            row["maintenanceCommand"] = invoke(root, "inspect-maintain")
            assert load(root / "inspect-maintain.json")["errorCode"] != 0
            assert before_files == {str(p): sha(p) for p in marker.parent.rglob("*") if p.is_file()}
            row["undoCommand"] = invoke(root, "inspect-undo")
            assert load(root / "inspect-undo.json")["resourceFirst"] == 1
        row["status"] = "passed"
        cases.append(row)
        print(mode, "passed", flush=True)
    for mode in ("journal-damaged", "blob-damaged", "target-replaced", "workspace-changed", "project-changed", "resource-changed", "owner-changed"):
        root = proof / mode
        root.mkdir()
        command = invoke(root, "physical", "physical")
        value = load(root / "barrier.json")
        marker = Path(value["sessionDirectory"]) / "rename-journal.json"
        assert marker.resolve().is_relative_to(proof.resolve())
        journal = load(marker)
        if mode == "journal-damaged": marker.write_bytes(b"{invalid-journal")
        elif mode == "blob-damaged":
            body = marker.parent / journal["resources"][0]["desired"]
            body.write_bytes(body.read_bytes() + b" ")
        elif mode == "target-replaced":
            target = root / "new😀.ico"
            saved = root / "retained-target.ico"
            assert target.resolve().parent == root.resolve() and saved.resolve().parent == root.resolve()
            os.rename(target, saved)
            target.write_bytes(saved.read_bytes())  # same bytes, different physical file
        elif mode in ("workspace-changed", "project-changed"):
            path = root / ("work.xlw" if mode == "workspace-changed" else "app.xlp")
            document = load(path)
            document["foreign_change"] = "必须保留 😀"
            path.write_text(json.dumps(document, ensure_ascii=False), encoding="utf-8")
        elif mode == "resource-changed": (root / "app.res.json").write_text('{"icon":"old.ico","external":"必须保留 😀"}', encoding="utf-8")
        elif mode == "owner-changed":
            journal["owner"] += ".foreign"
            marker.write_text(json.dumps(journal, ensure_ascii=False), encoding="utf-8")
        retained = {str(p): sha(p) for p in root.rglob("*") if p.is_file()}
        inspection = invoke(root, "inspect")
        recovered = load(root / "inspection.json")
        assert recovered["workspaceRenamed"] == recovered["projectRenamed"] == 0 and recovered["sessionIssue"]
        assert recovered["xlangBody"] == recovered["cBody"] == recovered["untitledBody"] == 1
        for path, digest in retained.items(): assert sha(path) == digest, path
        cases.append({"case": mode, "command": command, "inspectionCommand": inspection, "inspection": recovered,
                      "preservedFiles": retained, "status": "passed"})
        print(mode, "passed", flush=True)
    result = {"status": "passed", "utc": datetime.now(timezone.utc).isoformat(), "proof": str(proof),
              "fixture": str(fixture), "fixtureSha256": sha(fixture), "cases": cases, "checks": len(cases),
              "filenameTransactionRecoveryComplete": True, "projectMigrationTransactionRecoveryComplete": False,
              "formalRelease": False}
    path = proof / "evidence.json"
    path.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"path": str(path), "sha256": sha(path), "checks": len(cases)}, ensure_ascii=False), flush=True)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", required=True)
    parser.add_argument("--output")
    args = parser.parse_args()
    check(args.fixture, args.output)
