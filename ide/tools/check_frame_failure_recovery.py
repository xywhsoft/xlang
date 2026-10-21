"""Verify handled native frame failures and recovery in separate processes."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import uuid


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def check(fixture, output):
    fixture = Path(fixture).resolve(strict=True)
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    proof = output / ("frame-failure-" + uuid.uuid4().hex)
    proof.mkdir()
    inputs = {str(p): sha(p) for p in (fixture, fixture.parent / "xge.dll")}
    calls = []
    checks = 0

    def invoke(name, arguments):
        nonlocal checks
        stdout = proof / (name + ".stdout")
        stderr = proof / (name + ".stderr")
        command = [str(fixture), *map(str, arguments)]
        started = datetime.now(timezone.utc).isoformat()
        with stdout.open("wb") as out, stderr.open("wb") as err:
            process = subprocess.Popen(command, stdout=out, stderr=err, cwd=fixture.parent)
            pid = process.pid
            try:
                code = process.wait(timeout=75)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                (proof / (name + ".timeout")).write_text("Owned fixture exceeded 75 seconds", encoding="utf8")
                raise
        text = stdout.read_text(encoding="utf8")
        record = {"name": name, "command": command, "pid": pid, "startedUtc": started,
                  "exit": code, "stdout": str(stdout), "stdoutSha256": sha(stdout),
                  "stderr": str(stderr), "stderrSha256": sha(stderr)}
        calls.append(record)
        (proof / (name + ".invocation.json")).write_text(json.dumps(record, indent=2), encoding="utf8")
        assert code == 0, (name, code, str(stderr))
        counts = re.findall(r"frame_failure_test (?:failure|restart) passed: (\d+) checks", text)
        assert len(counts) == 1, (name, text)
        checks += int(counts[0])
        return text, stderr.read_text(encoding="utf8")

    for name, option, error, previous in (
        ("gpu", [], -9, False),
        ("memory", ["--oom"], -5, False),
        ("locked", ["--locked"], -9, True),
        ("xge-begin", ["--xge-begin"], -8, False),
        ("xge-end", ["--xge-end"], -15, False),
    ):
        directory = proof / name
        text, errors = invoke(name + "-failure", [directory, *option])
        assert "code=" + str(error) in text
        assert "domain=" + ("XGE" if name.startswith("xge-") else "XUI") + " recovery=" in text
        ids = re.search(r"Saved fixture IDs (\S+) (\S+)", text)
        assert ids, text
        if previous:
            assert "recovery checkpoint after frame failure:" in errors
        restored, _ = invoke(name + "-restart", [directory, "--restore-previous" if previous else "--restore", *ids.groups()])
        assert "restoredPrevious=" + str(int(previous)) in restored
        print("Passed native frame failure and separate-process restart", name, flush=True)

    _, errors = invoke("protected", [proof / "protected", "--protected"])
    assert "recovery checkpoint after frame failure:" in errors
    text, _ = invoke("disabled", [proof / "disabled", "--no-session"])
    assert "noSession=1" in text and not (proof / "disabled" / "sessions").exists()
    for path, digest in inputs.items():
        assert sha(path) == digest, path
    images = []
    for path in sorted(proof.rglob("*.png")):
        raw = path.read_bytes()
        assert raw[:8] == b"\x89PNG\r\n\x1a\n"
        width, height = struct.unpack_from(">II", raw, 16)
        assert width >= 640 and height >= 480
        images.append({"path": str(path), "sha256": sha(path), "width": width, "height": height})
    assert len(images) == 5
    result = {"status": "passed", "utc": datetime.now(timezone.utc).isoformat(),
              "fixture": str(fixture), "fixtureSha256": inputs[str(fixture)],
              "checks": checks, "cases": 7, "actualProcesses": len(calls),
              "separateProcessRestarts": 5, "injectedFailure": True,
              "originalHistoricalFailureCauseResolved": False,
              "userSourcesSaved": False, "formalRelease": False, "goalStatus": "active",
              "inputs": inputs, "calls": calls, "images": images}
    result["files"] = [{"path": str(p), "sha256": sha(p)} for p in sorted(proof.rglob("*")) if p.is_file()]
    (proof / "evidence.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf8")
    print("Native frame failure recovery:", checks, "checks, 12 processes, 5 restarts;", proof / "evidence.json", flush=True)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    check(args.fixture, args.output)
