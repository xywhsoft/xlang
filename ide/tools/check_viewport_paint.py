"""Compare full-scan and bounded-replay GPU pixels on a private SDK build."""
import argparse
import ctypes as c
from ctypes import wintypes as w
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
import uuid
from native_desktop import NativeDesktopProcess

IDE = Path(__file__).resolve().parents[1]


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def write(path, value):
    temp = path.with_suffix(".tmp")
    temp.write_text(json.dumps(value, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    os.replace(temp, path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-build", type=Path, required=True)
    parser.add_argument("--geometry-fixture", action="store_true", help="Also seed painting from independently retained geometry")
    args = parser.parse_args()
    receipt = args.sdk_build.resolve()
    build = read(receipt)
    assert build["exitCode"] == 0 and build["wholeSdkWerror"] and build["sourceUnchanged"]
    manifest = Path(build["sourceManifest"])
    assert sha(manifest) == build["sourceManifestSha256"]
    root = IDE/"build"/("viewport-paint-oracle-"+uuid.uuid4().hex[:12])
    root.mkdir()
    protected = {receipt: sha(receipt), manifest: sha(manifest), Path(__file__): sha(__file__)}
    for row in read(manifest)["files"]:
        protected[Path(build["source"])/row["path"]] = row["sha256"]
    outputs = {}
    for row in build["outputs"]:
        path = Path(row["path"])
        protected[path] = row["sha256"]
        outputs[path.name] = path
    fixture = IDE/("third_party/sdk_patches/geometry-paint-v1/tests/pixel_oracle.c" if args.geometry_fixture else "third_party/sdk_patches/viewport-paint-v1/tests/pixel_oracle.c")
    protected[fixture] = sha(fixture)
    shutil.copy2(fixture, root/fixture.name)
    shutil.copy2(__file__, root/Path(__file__).name)
    shutil.copy2(outputs["xge.dll"], root/"xge.dll")
    fonts = []
    for name in ("latin.ttf", "cjk.otf", "kern.ttf"):
        path = IDE/"build/editor-paint-stream-fixture-v1"/name
        protected[path] = sha(path)
        shutil.copy2(path, root/name)
        fonts.append(str(root/name))
    headers = Path(build["source"])
    executable = root/"pixel_oracle.exe"
    command = [r"E:\software\w64devkit\bin\gcc.exe", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
               "--param=ggc-min-expand=1", "--param=ggc-min-heapsize=1024", "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-cast-function-type",
               "-DXGE_XRT_PROFILE_IDE", "-DXGE_DEBUGMODE=0", "-finput-charset=UTF-8", "-fexec-charset=UTF-8",
               "-I"+str(headers), "-I"+str(headers/"lib/xrt"), str(root/fixture.name),
               "-lm", "-lws2_32", "-liphlpapi", "-lgdi32", "-luser32", "-lshell32", "-lole32", "-loleaut32", "-luuid", "-limm32", "-lwinmm", "-lavrt", "-o", str(executable)]
    evidence = root/"evidence.json"
    record = {"status": "linking", "utc": datetime.now(timezone.utc).isoformat(), "root": str(root),
              "sdkBuild": str(receipt), "sdkBuildSha256": sha(receipt), "sdkSha256": sha(outputs["xge.dll"]),
              "arguments": command, "sourceSha256": sha(fixture), "fonts": {p: sha(p) for p in fonts},
              "neverSwitchedDesktop": True, "productionChanged": False, "formalRelease": False, "goalStatus": "active"}
    record["wholeEngineFixture"] = True
    write(evidence, record)
    print(root, flush=True)

    def verify():
        for path, digest in protected.items():
            assert sha(path) == digest, str(path)

    verify()
    with (root/"link.log").open("xb") as stream:
        result = subprocess.run(command, stdout=stream, stderr=stream)
    record["linkExitCode"] = result.returncode
    record["linkLogSha256"] = sha(root/"link.log")
    if result.returncode:
        record["status"] = "strict-fixture-link-failed"
        write(evidence, record)
        print((root/"link.log").read_text(encoding="utf-8", errors="replace"), flush=True)
        return result.returncode
    record["executableSha256"] = sha(executable)
    user = c.WinDLL("user32", use_last_error=True)
    user.CreateDesktopW.restype = w.HANDLE
    user.CreateDesktopW.argtypes = [w.LPCWSTR, w.LPCWSTR, c.c_void_p, w.DWORD, w.DWORD, c.c_void_p]
    user.CloseDesktop.restype = w.BOOL
    user.CloseDesktop.argtypes = [w.HANDLE]
    name = "XlangViewportPaint-"+uuid.uuid4().hex
    desktop = user.CreateDesktopW(name, None, None, 0, 0x10000000, None)
    assert desktop, c.get_last_error()
    log = root/"native.log"
    child = None
    started = time.monotonic()
    record["status"] = "running"
    write(evidence, record)
    try:
        with log.open("xb") as stream:
            child = NativeDesktopProcess([str(executable), *fonts], stream, stream, name, cwd=str(root), env=dict(os.environ))
            record["pid"] = child.pid
            while child.poll() is None:
                record["elapsedSeconds"] = round(time.monotonic()-started, 3)
                record["lastConfirmedLiveUtc"] = datetime.now(timezone.utc).isoformat()
                write(evidence, record)
                if time.monotonic()-started > 180:
                    record["deadlineExceeded"] = True
                    child.kill()
                    record["terminatedOnlyOwnedJob"] = True
                time.sleep(.25)
            record["exitCode"] = child.wait()
        completed = []
        for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith("{"):
                value = json.loads(line)
                if value.get("event") == "completed":
                    completed.append(value)
        record["completed"] = completed
        assert record["exitCode"] == 0 and len(completed) == 1
        record["status"] = "viewport-replay-pixel-oracle-passed"
        return 0
    except BaseException as error:
        record["status"] = "viewport-replay-pixel-oracle-failed"
        record["error"] = str(error)
        raise
    finally:
        if child:
            child.close()
        assert user.CloseDesktop(desktop)
        verify()
        record["protectedInputsUnchanged"] = True
        record.update(elapsedSeconds=round(time.monotonic()-started, 3), logSha256=sha(log),
                      finishedUtc=datetime.now(timezone.utc).isoformat())
        write(evidence, record)
        print(json.dumps({"status": record["status"], "elapsedSeconds": record["elapsedSeconds"], "report": str(evidence)}, ensure_ascii=False), flush=True)


if __name__ == "__main__":
    raise SystemExit(main())
