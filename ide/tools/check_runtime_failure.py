"""Exercise shipped GUI failures on a private, never switched Windows desktop."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import uuid
from PIL import Image


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def check(fixture, gui, console, output):
    fixture, gui, console = (Path(p).resolve(strict=True) for p in (fixture, gui, console))
    proof = Path(output).resolve() / ("native-failures-" + uuid.uuid4().hex)
    proof.mkdir(parents=True)
    originals = [fixture, gui, console, gui.parent / "xge.dll", gui.parent / "data/commands.json"]
    inputs = {str(p): sha(p) for p in originals}
    calls, images, seeds = [], [], []
    checks = 0

    def invoke(name, arguments):
        nonlocal checks
        stdout, stderr = proof / (name + ".stdout"), proof / (name + ".stderr")
        started = datetime.now(timezone.utc).isoformat()
        command = [str(fixture), *map(str, arguments)]
        with stdout.open("xb") as out, stderr.open("xb") as err:
            process = subprocess.Popen(command, stdout=out, stderr=err, cwd=fixture.parent)
            try:
                code = process.wait(timeout=45)
            except subprocess.TimeoutExpired:
                # Only this fixture is owned; its child is in a kill-on-close job.
                process.kill()
                process.wait()
                (proof / (name + ".timeout")).write_text("Owned fixture exceeded 45 seconds", encoding="utf8")
                raise
        row = {"name": name, "command": command, "pid": process.pid, "startedUtc": started,
               "exit": code, "stdout": str(stdout), "stdoutSha256": sha(stdout),
               "stderr": str(stderr), "stderrSha256": sha(stderr)}
        calls.append(row)
        text = stdout.read_text(encoding="utf8")
        startedChild = re.search(r"runtime failure owned child: pid=(\d+)", text)
        if startedChild:
            row["childPid"] = int(startedChild[1])
        (proof / (name + ".invocation.json")).write_text(json.dumps(row, ensure_ascii=False, indent=2), encoding="utf8")
        assert code == 0, (name, code, str(stderr))
        counts = re.findall(r"runtime_failure_test (?:storage|launch) passed: (\d+) checks", text)
        assert len(counts) == 1, (name, text)
        checks += int(counts[0])
        child = re.search(r"pid=(\d+) exit=(\d+) dialogs=(\d+) owned-live=0", text)
        if child:
            row.update(childPid=int(child[1]), childExit=int(child[2]), dialogs=int(child[3]))
        return row

    status = "failed"
    try:
        invoke("storage", ["--storage", proof / "storage"])
        for mode in ("missing", "session", "arguments", "hidden", "console", "blocked", "default", "fallback", "quota"):
            directory = proof / mode
            directory.mkdir()
            executable = console if mode == "console" else gui
            copy = directory / executable.name
            shutil.copy2(executable, copy)
            shutil.copy2(gui.parent / "xge.dll", directory / "xge.dll")
            (directory / "data").mkdir()
            shutil.copy2(gui.parent / "data/commands.json", directory / "data/commands.json")
            root = directory / "诊断 文件 😀"
            if mode == "blocked":
                root.write_bytes(b"preserve existing root file")
                seeds.append({"path": str(root), "sha256": sha(root)})
            elif mode == "fallback":
                (directory / "option").write_bytes(b"preserve existing option file")
                seeds.append({"path": str(directory / "option"), "sha256": sha(directory / "option")})
            elif mode == "quota":
                root.mkdir()
                for i in range(64):
                    p = root / (str(i) + ".txt")
                    p.write_bytes(b"preserve existing diagnostic content")
                    seeds.append({"path": str(p), "sha256": sha(p)})
            row = invoke(mode, ["--launch", copy, directory, mode])
            assert row["childExit"] == (2 if mode == "arguments" else 1)
            assert row["dialogs"] == (0 if mode in ("hidden", "console") else 1)
            errors = (directory / "stderr.log").read_text(encoding="utf8")
            assert "xlang IDE failure:" in errors
            if mode != "arguments":
                assert "stage=4 domain=model code=2" in errors, (mode, errors)
                assert "缺失 工作空间 😀.xlw" in errors
                assert "recovery=0" in errors if mode == "session" else "recovery=1" in errors
            if mode in ("default", "fallback"):
                root = directory / ("xlang-ide/diagnostics" if mode == "fallback" else "option/diagnostics")
            reports = sorted(root.glob("xlang-ide-*.log")) if root.is_dir() else []
            if mode in ("blocked", "quota"):
                assert not reports
                assert "diagnostic could not be written:" in errors
            else:
                assert len(reports) == 1, (mode, reports)
                body = reports[0].read_text(encoding="utf8")
                assert body.startswith("xlang-ide-runtime-failure-1\n")
                assert len(reports[0].read_bytes()) <= 8192
                assert "stage=" + ("arguments" if mode == "arguments" else "workspace") + "\n" in body
                assert "recovery=0\n" in body if mode in ("session", "arguments") else "recovery=1\n" in body
                assert "--no-session" not in body and "--diagnostic-root" not in body
            if row["dialogs"]:
                text = (directory / "dialog.txt").read_text(encoding="utf8")
                assert "xideWorkspaceOpen" not in text
                if mode != "arguments":
                    assert "缺失 工作空间 😀.xlw" in text
                with Image.open(directory / "dialog.bmp") as image:
                    image = image.convert("RGB")
                    assert image.width >= 200 and image.height >= 150
                    assert max(hi - lo for lo, hi in image.getextrema()) >= 30
                    body = image.crop((20, 35, image.width - 20, image.height - 65))
                    assert max(hi - lo for lo, hi in body.getextrema()) >= 30, (mode, "native dialog body was not painted")
                    png = directory / "dialog.png"
                    image.save(png)
                    images.append({"path": str(png), "sha256": sha(png), "width": image.width, "height": image.height})
            print("Passed real native failure", mode, "dialogs=" + str(row["dialogs"]), flush=True)
        assert len(calls) == 10 and len(images) == 7
        for row in seeds:
            assert sha(row["path"]) == row["sha256"], row["path"]
        for path, digest in inputs.items():
            assert sha(path) == digest, path
        status = "passed"
    finally:
        result = {"status": status, "utc": datetime.now(timezone.utc).isoformat(), "inputs": inputs,
                  "checks": checks, "fixtureProcesses": len(calls), "actualGuiOrConsoleChildren": sum("childPid" in row for row in calls),
                  "privateDesktopNeverSwitched": True, "boundedFiles": 64, "boundedBytes": 524288,
                  "noAutomaticDeletion": True, "userSourcesSaved": False, "cases": 9,
                  "calls": calls, "images": images, "protectedFiles": seeds,
                  "formalRelease": False, "goalStatus": "active"}
        result["files"] = [{"path": str(p), "sha256": sha(p)} for p in sorted(proof.rglob("*")) if p.is_file()]
        (proof / "evidence.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf8")
    print("Native GUI error visibility:", checks, "checks, 9 real launches, 7 native dialog images;", proof / "evidence.json", flush=True)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture", required=True)
    parser.add_argument("--gui", required=True)
    parser.add_argument("--console", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    check(args.fixture, args.gui, args.console, args.output)
