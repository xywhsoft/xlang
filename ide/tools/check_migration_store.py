"""Verify retired migration body maintenance through real native IDE entry points."""
import argparse
import ctypes
from ctypes import wintypes
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


def age_store(journal):
    """Use real Windows file times; preserve bytes, identity and all user sources."""
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                  wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    kernel.CreateFileW.restype = wintypes.HANDLE
    kernel.SetFileTime.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.FILETIME),
                                  ctypes.POINTER(wintypes.FILETIME), ctypes.POINTER(wintypes.FILETIME)]
    kernel.SetFileTime.restype = wintypes.BOOL
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    value = load(journal)
    assert value["state"] == "retired", value["state"]
    stamp = int((time.time() - 20 * 86400 + 11644473600) * 10000000)
    native_time = wintypes.FILETIME(stamp & 0xffffffff, stamp >> 32)
    rows = value.get("ownedBodies", value["blobs"])
    assert rows
    for row in rows:
        path = journal.parent / row["ref"]
        assert path.is_file() and sha(path) == row["sha256"]
        handle = kernel.CreateFileW(str(path), 0x100, 7, None, 3, 0x200000, None)
        assert handle != wintypes.HANDLE(-1).value, ctypes.get_last_error()
        try:
            assert kernel.SetFileTime(handle, ctypes.byref(native_time), None,
                                      ctypes.byref(native_time)), ctypes.get_last_error()
        finally:
            kernel.CloseHandle(handle)
        row["lastWrite"] = str(stamp)
    times = {row["ref"]: row["lastWrite"] for row in rows}
    for row in value["blobs"]:
        row["lastWrite"] = times[row["ref"]]
    value["retiredAt"] = str(stamp)
    journal.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf8")
    return {"retiredAt": str(stamp), "bodies": len(rows), "markerSha256": sha(journal)}


def check(fixture, output=None, baseline=False):
    ide = Path(__file__).resolve().parents[1]
    fixture = Path(fixture).resolve(strict=True)
    proof = Path(output).absolute() if output else ide / "build/tests/migration-storage" / uuid.uuid4().hex
    assert proof.resolve().is_relative_to((ide / "build").resolve())
    proof.mkdir(parents=True)
    cases = []

    def invoke(root, operation):
        args = [str(fixture), str(root), "dir-internal-saved", operation]
        stdout, stderr = root / (operation + ".stdout"), root / (operation + ".stderr")
        with stdout.open("xb") as out, stderr.open("xb") as err:
            process = subprocess.Popen(args, stdout=out, stderr=err)
            try:
                code = process.wait(timeout=45)
            finally:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=10)
        assert code == 0, (operation, code, stderr.read_text(errors="replace"))
        return {"arguments": args, "pid": process.pid, "exit": code, "terminated": False,
                "stdout": str(stdout), "stdoutSha256": sha(stdout),
                "stderr": str(stderr), "stderrSha256": sha(stderr)}

    entries = ["maintain"] if baseline else ["maintain", "auto", "view"]
    for entry in entries:
        root = proof / entry
        root.mkdir()
        calls = [invoke(root, "operate")]
        journal = next((root / "sessions/migrations").glob("*/migration-journal.json"))
        aged = age_store(journal)
        before = {str(p): sha(p) for p in root.rglob("*")
                  if p.is_file() and p.suffix in (".c", ".xl", ".xlp", ".xlw")}
        copies = {str(p): sha(p) for p in (root / "sessions").rglob("recovery/*")
                  if p.is_file() and not p.is_relative_to(journal.parent)}
        request = load(root / "request.json")
        operation = "inspect-store-" + entry
        calls.append(invoke(root, operation))
        state = load(root / (operation + ".json"))
        assert state["storeBefore"] == aged["bodies"] and state["storeBefore"] > 0
        assert state["storeAfter"] == (state["storeBefore"] if baseline else 0), state
        for key in ("xlangBody", "cBody", "outsideBody", "untitledBody", "property",
                    "cLanguage", "xlangLanguage", "cOverride", "selection"):
            assert state[key] == 1, (entry, key, state)
        assert state["projectId"] == request["projectId"] and state["cId"] == request["cId"]
        assert state["activeId"] == request["cId"] and not state["issue"]
        after = load(journal)
        assert after["state"] == "retired"
        if not baseline:
            assert after["ownedBodies"] == after["blobs"] == []
        for path, digest in {**before, **copies}.items():
            assert sha(path) == digest, path
        cases.append({"entry": entry, "calls": calls, "aged": aged, "recovered": state,
                      "unchangedUserFiles": before, "unchangedMainCopies": copies})
        print("Passed migration storage", entry, "baseline" if baseline else "current", flush=True)
    result = {"utc": datetime.now(timezone.utc).isoformat(), "fixture": str(fixture),
              "fixtureSha256": sha(fixture), "checks": len(cases), "cases": cases,
              "baseline": baseline, "retiredBodyMaintenanceVerified": not baseline,
              "oldRetiredBodyLeakReproduced": baseline, "formalRelease": False, "goalStatus": "active"}
    result["files"] = [{"path": str(p), "sha256": sha(p)} for p in sorted(proof.rglob("*")) if p.is_file()]
    (proof / "evidence.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf8")
    print("Migration storage evidence", proof / "evidence.json", flush=True)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture", required=True)
    parser.add_argument("--output")
    parser.add_argument("--baseline", action="store_true")
    args = parser.parse_args()
    check(args.fixture, args.output, args.baseline)
