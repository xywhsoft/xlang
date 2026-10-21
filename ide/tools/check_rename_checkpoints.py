"""Interrupt owned native rename fixtures and inspect actual session recovery."""
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


def check(fixture, baseline=False, output=None):
    ide = Path(__file__).resolve().parents[1]
    fixture = Path(fixture).resolve(strict=True)
    proof = Path(output).absolute() if output else ide / "build/tests/rename-checkpoints" / uuid.uuid4().hex
    if not proof.resolve().is_relative_to((ide / "build").resolve()):
        raise ValueError("Rename checkpoint evidence must stay inside ide/build")
    proof.mkdir(parents=True)
    cases = []
    def invoke(root, mode, barrier=False):
        out = root / (mode + ".stdout")
        err = root / (mode + ".stderr")
        args = [str(fixture), str(root), mode]
        stopped = False
        with out.open("xb") as stdout, err.open("xb") as stderr:
            process = subprocess.Popen(args, stdout=stdout, stderr=stderr)
            try:
                end = time.monotonic() + 40
                marker = root / "barrier.json"
                while process.poll() is None and time.monotonic() < end:
                    if barrier and marker.is_file():
                        value = load(marker)
                        assert value["pid"] == process.pid and value["phase"] == mode
                        # The durable marker comes from the test-only barrier in
                        # the real production path. Terminate only our Popen handle.
                        process.terminate()
                        stopped = True
                        break
                    time.sleep(0.04)
                if process.poll() is None:
                    process.wait(timeout=max(1, end - time.monotonic()))
                assert stopped if barrier else process.returncode == 0, (mode, process.returncode, err.read_text(errors="replace"))
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=10)
        return {"arguments": args, "exitCode": process.returncode, "terminatedAtBarrier": stopped,
                "stdout": str(out), "stdoutSha256": sha(out), "stderr": str(err), "stderrSha256": sha(err)}
    modes = ("pre-conflict", "physical") if baseline else ("before", "physical", "after", "success", "pending", "pre-conflict", "post-conflict", "noop", "no-session")
    for mode in modes:
        root = proof / mode
        root.mkdir()
        command = invoke(root, mode, mode in ("before", "physical", "after"))
        value = load(root / ("barrier.json" if command["terminatedAtBarrier"] else "result.json"))
        row = {"case": mode, "command": command, "result": value}
        if baseline:
            assert value["checkpointCalls"] == 0
            assert value["sourceExists"] == 0 and value["targetExists"] == 1
            if mode == "pre-conflict":
                assert value["applied"] == 1
                assert (Path(value["sessionDirectory"]) / "session.json").read_bytes() == b"{conflict"
            else:
                row["inspectionCommand"] = invoke(root, "inspect")
                recovered = load(root / "inspection.json")
                assert recovered["xlangBody"] == recovered["cBody"] == recovered["untitledBody"] == 0
                row["inspection"] = recovered
        elif mode == "pre-conflict":
            assert value["applied"] == 0 and value["sourceExists"] == 1 and value["targetExists"] == 0
            assert value["checkpointCalls"] == 1 and value["xlangBody"] == value["cBody"] == value["untitledBody"] == 1
            assert (Path(value["sessionDirectory"]) / "session.json").read_bytes() == b"{conflict"
        elif mode == "post-conflict":
            assert value["applied"] == 1 and value["targetExists"] == 1 and value["sourceExists"] == 0
            assert value["checkpointCalls"] == 2 and "会话" in value["status"] and "已改名" in value["status"]
            assert (Path(value["sessionDirectory"]) / "session.json").read_bytes() == b"{conflict"
        elif mode in ("noop", "no-session"):
            assert value["applied"] == 1 and value["xlangBody"] == value["cBody"] == value["untitledBody"] == 1
            assert (value["sourceExists"], value["targetExists"]) == ((1, 0) if mode == "noop" else (0, 1))
            if mode == "noop":
                assert value["checkpointCalls"] == 0
            else:
                assert not value["sessionDirectory"] and not (root / "sessions").exists()
        else:
            moved = mode != "before"
            assert (value["sourceExists"], value["targetExists"]) == ((0, 1) if moved else (1, 0))
            row["inspectionCommand"] = invoke(root, "inspect")
            recovered = load(root / "inspection.json")
            assert recovered["xlangBody"] == recovered["cBody"] == recovered["untitledBody"] == 1
            assert recovered["resourcePatched"] == moved
            assert recovered["resourceBefore"] == (mode == "before")
            assert recovered["workspaceRenamed"] == moved
            assert recovered["activeC"] == 1
            if mode == "pending":
                assert value["applied"] == 0 and value["pending"] == 1 and value["checkpointCalls"] == 2
            if mode in ("after", "success"):
                assert value["checkpointCalls"] == 2
            row["inspection"] = recovered
            # Recovery restores unsaved references in memory, preserving the
            # original disk manifest until the user explicitly saves it.
            if moved:
                workspace = load(root / "work.xlw")
                assert any(item["path"] == "old.ico" for item in workspace["files"])
                row["crossFileReferencesRecovered"] = bool(recovered["workspaceRenamed"])
        row["status"] = "passed"
        cases.append(row)
        print(mode, "passed", flush=True)
    report = {"status": "original-checkpoint-loss-reproduced" if baseline else "passed",
              "utc": datetime.now(timezone.utc).isoformat(), "proof": str(proof), "fixture": str(fixture),
              "fixtureSha256": sha(fixture), "checks": len(cases), "cases": cases,
              "filenameTransactionRecoveryComplete": not baseline,
              "projectMigrationTransactionRecoveryComplete": False, "formalRelease": False}
    path = proof / "evidence.json"
    path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"path": str(path), "sha256": sha(path), "checks": len(cases)}, ensure_ascii=False), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", required=True)
    parser.add_argument("--baseline", action="store_true")
    parser.add_argument("--output", help="Fresh evidence directory inside ide/build")
    args = parser.parse_args()
    check(args.fixture, args.baseline, args.output)
