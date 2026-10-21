"""Observe the current native editor on an owned, noninteractive desktop.

Uses the exact current build objects, verifies their input hashes, and retains
every native log and image. Timings are observations, not benchmark thresholds.
"""
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
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    os.replace(temporary, path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bytes", type=int, action="append", dest="sizes")
    parser.add_argument("--language", choices=("c", "xlang"), action="append", dest="languages")
    parser.add_argument("--sdk-build", type=Path, help="Optional private SDK build receipt; does not install it")
    parser.add_argument("--app-build", type=Path, help="Optional verified private DPI-proxy object receipt")
    parser.add_argument("--dpi", type=float, choices=(1.0,1.25,1.5,2.0), default=1.25, help="Logical DPI override for a controlled comparison")
    parser.add_argument("--dpi-cycle", action="store_true", help="Also edit and undo through repeated font/DPI replacement")
    args = parser.parse_args()
    sizes = args.sizes or [8192, 16777216]
    languages = args.languages or ["c", "xlang"]
    if any(size < 16 or size > 16777216 for size in sizes):
        parser.error("Each size must be between 16 and 16777216 UTF-8 bytes")
    pointer_path = IDE/"build/current-build.json"
    pointer = read(pointer_path)
    build_path = Path(pointer["report"])
    build = read(build_path)
    assert pointer["status"] == build["status"] == "current-source-build-passed"
    assert sha(build_path) == pointer["reportSha256"] and build["exitCode"] == 0
    base = Path(build["root"])
    protected = {pointer_path: sha(pointer_path), build_path: sha(build_path)}
    for row in build["inputs"]:
        protected[IDE/row["path"]] = row["sha256"]
        protected[base/row["path"]] = row["sha256"]
    for group in ("objects", "outputs"):
        for name, digest in build[group].items():
            protected[base/name] = digest
    source = IDE/"tests/editor_performance_probe.c"
    protected[source] = sha(source)
    protected[Path(__file__)] = sha(__file__)
    private_receipt = None
    sdk_dll = base/"release/xge.dll"
    if args.sdk_build:
        private_receipt = args.sdk_build.resolve()
        sdk = read(private_receipt)
        assert sdk["exitCode"] == 0 and sdk["wholeSdkWerror"] and sdk["sourceUnchanged"]
        protected[private_receipt] = sha(private_receipt)
        sdk_manifest = Path(sdk["sourceManifest"])
        assert sha(sdk_manifest) == sdk["sourceManifestSha256"]
        protected[sdk_manifest] = sha(sdk_manifest)
        for row in read(sdk_manifest)["files"]:
            protected[Path(sdk["source"])/row["path"]] = row["sha256"]
        for row in sdk["outputs"]:
            protected[Path(row["path"])] = row["sha256"]
            if Path(row["path"]).name == "xge.dll":
                sdk_dll = Path(row["path"])

    object_override = None
    private_app_receipt = None
    if args.app_build:
        assert private_receipt, "The private paint cursor requires its matching private SDK"
        private_app_receipt = args.app_build.resolve()
        app = read(private_app_receipt)
        assert app["status"] == "private-dpi-paint-object-passed" and app["exitCode"] == 0
        assert app["wholeObjectWerror"] and app["paintCursorEnabled"] and not app["productionChanged"]
        assert Path(app["baseBuildReport"]) == build_path and app["baseBuildReportSha256"] == sha(build_path)
        assert Path(app["originalSource"]) == IDE/"src/platform/dpi_proxy.c"
        assert app["originalSourceSha256"] == protected[IDE/"src/platform/dpi_proxy.c"]
        assert app["replaceObject"] == "build/Release/src_platform_dpi_proxy.c.o" and app["replaceObject"] in build["objects"]
        protected[private_app_receipt] = sha(private_app_receipt)
        for path_key, hash_key in (("source", "sourceSha256"), ("object", "objectSha256"), ("log", "logSha256")):
            protected[Path(app[path_key])] = app[hash_key]
        object_override = (app["replaceObject"], Path(app["object"]))

    def verify():
        for path, digest in protected.items():
            assert sha(path) == digest, f"Protected input/output changed: {path}"

    verify()
    root = IDE/"build"/("editor-observation-"+uuid.uuid4().hex[:12])
    root.mkdir()
    release = root/"release"
    release.mkdir()
    for name in ("xge.dll", "xlang-ls.exe"):
        shutil.copy2(sdk_dll if name == "xge.dll" else base/"release"/name, release/name)
    shutil.copytree(base/"release/data", release/"data")
    shutil.copy2(source, root/source.name)
    shutil.copy2(__file__, root/Path(__file__).name)
    flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter", "-DXGE_DLL", "-DXUI_DLL",
             "-DXGE_XRT_PROFILE_IDE", "-DXGE_DEBUGMODE=0", "-DXIDE_STREAM_LAYOUT_ENABLED=1", "-DXIDE_CURSOR_LAYOUT_ENABLED=1",
             "-O2", "-finput-charset=UTF-8", "-fexec-charset=UTF-8",
             "-I"+str(IDE/"src"), "-I"+str(IDE/"third_party/xge_sdk"), "-I"+str(IDE/"third_party/xge_sdk/lib/xrt")]
    executable = release/"editor_performance_probe.exe"
    command = [build["compiler"], *flags, str(root/source.name),
               *[str(object_override[1] if object_override and name == object_override[0] else base/name)
                 for name in sorted(build["objects"])], str(IDE/"third_party/xge_sdk/xge.lib"),
               "-lm", "-lws2_32", "-lwinhttp", "-liphlpapi", "-lgdi32", "-luser32", "-lshell32", "-lopengl32",
               "-lole32", "-lwinmm", "-lavrt", "-lcomdlg32", "-lversion", "-lbcrypt", "-ladvapi32", "-lpsapi",
               "-o", str(executable)]
    record = {"status": "linking", "utc": datetime.now(timezone.utc).isoformat(), "root": str(root),
              "baseBuildReport": str(build_path), "baseBuildReportSha256": sha(build_path), "objects": len(build["objects"]),
              "probeSourceSha256": sha(source), "arguments": command, "sdkSha256": sha(sdk_dll),
              "privateSdkReceipt": str(private_receipt) if private_receipt else None,
              "privateSdkReceiptSha256": sha(private_receipt) if private_receipt else None,
              "privateAppReceipt": str(private_app_receipt) if private_app_receipt else None,
              "privateAppReceiptSha256": sha(private_app_receipt) if private_app_receipt else None,
              "objectOverrides": 1 if object_override else 0,
              "neverSwitchedDesktop": True, "productionChanged": False, "oneObservationPerCase": True,
              "benchmarkTiming": False, "goalStatus": "active", "formalRelease": False, "cases": []}
    record["dpiOverride"] = args.dpi
    record["dpiCycle"] = args.dpi_cycle
    evidence = root/"evidence.json"
    write(evidence, record)
    print(root, flush=True)
    with (root/"link.log").open("xb") as stream:
        result = subprocess.run(command, stdout=stream, stderr=stream)
    record["linkExitCode"] = result.returncode
    record["linkLogSha256"] = sha(root/"link.log")
    write(evidence, record)
    if result.returncode:
        record["status"] = "strict-link-failed"
        write(evidence, record)
        print((root/"link.log").read_text(encoding="utf-8", errors="replace"), flush=True)
        return result.returncode
    record["executableSha256"] = sha(executable)
    protected[executable] = sha(executable)
    protected[release/"xge.dll"] = sha(release/"xge.dll")
    user = c.WinDLL("user32", use_last_error=True)
    user.CreateDesktopW.restype = w.HANDLE
    user.CreateDesktopW.argtypes = [w.LPCWSTR, w.LPCWSTR, c.c_void_p, w.DWORD, w.DWORD, c.c_void_p]
    user.CloseDesktop.restype = w.BOOL
    user.CloseDesktop.argtypes = [w.HANDLE]
    kernel = c.WinDLL("kernel32", use_last_error=True)
    kernel.CreateEventW.restype = w.HANDLE
    kernel.CreateEventW.argtypes = [c.c_void_p, w.BOOL, w.BOOL, w.LPCWSTR]
    kernel.SetEvent.argtypes = [w.HANDLE]
    kernel.CloseHandle.argtypes = [w.HANDLE]
    kernel.SetInformationJobObject.argtypes = [w.HANDLE, c.c_int, c.c_void_p, w.DWORD]
    record["status"] = "running"
    try:
        for language in languages:
            for size in sizes:
                case_root = root/(language+"-"+str(size))
                case_root.mkdir()
                path = case_root/("正文 源文件."+("c" if language == "c" else "xl"))
                motif = "中文😀 é 0123456789 ".encode("utf-8")
                rest = size-6
                text = b"/* "+motif*(rest//len(motif))+b" "*(rest%len(motif))+b" */"
                assert len(text) == size
                text.decode("utf-8")
                path.write_bytes(text)
                case = {"language": language, "bytes": size, "root": str(case_root), "inputSha256": sha(path),
                        "processMemoryLimitBytes": 2147483648, "deadlineSeconds": 180}
                record["cases"].append(case)
                name = "XlangEditorObservation-"+uuid.uuid4().hex
                desktop = user.CreateDesktopW(name, None, None, 0, 0x10000000, None)
                assert desktop, c.get_last_error()
                event_name = "XlangEditorReady-"+uuid.uuid4().hex
                event = kernel.CreateEventW(None, True, False, event_name)
                assert event, c.get_last_error()
                log = case_root/"native.log"
                child = None
                started = time.monotonic()
                try:
                    with log.open("xb") as stream:
                        environment = dict(os.environ)
                        environment["XLANG_PROBE_DPI"] = str(args.dpi)
                        environment.pop("XLANG_PROBE_DPI_CYCLE", None)
                        if args.dpi_cycle:
                            environment["XLANG_PROBE_DPI_CYCLE"] = "1"
                        child = NativeDesktopProcess([str(executable), str(case_root), str(path), language, event_name],
                                                     stream, stream, name, cwd=str(IDE), env=environment)
                        limits = c.create_string_buffer(144)
                        c.c_uint32.from_buffer(limits, 16).value = 0x2000 | 0x100
                        c.c_size_t.from_buffer(limits, 112).value = 2147483648
                        assert kernel.SetInformationJobObject(child.job, 9, limits, 144), c.get_last_error()
                        assert kernel.SetEvent(event)
                        case["pid"] = child.pid
                        write(evidence, record)
                        while child.poll() is None:
                            case["lastConfirmedLiveUtc"] = datetime.now(timezone.utc).isoformat()
                            case["elapsedSeconds"] = round(time.monotonic()-started, 3)
                            write(evidence, record)
                            if time.monotonic()-started > 180:
                                case["deadlineExceeded"] = True
                                child.kill()
                                case["terminatedOnlyOwnedJob"] = True
                            time.sleep(.25)
                        case["exitCode"] = child.wait()
                finally:
                    if child:
                        child.close()
                    assert kernel.CloseHandle(event)
                    assert user.CloseDesktop(desktop)
                    case["elapsedSeconds"] = round(time.monotonic()-started, 3)
                    case["logSha256"] = sha(log)
                    case["inputUnchanged"] = sha(path) == case["inputSha256"]
                    case["measurements"] = []
                    for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
                        if line.startswith("{"):
                            try:
                                case["measurements"].append(json.loads(line))
                            except json.JSONDecodeError:
                                pass
                    case["images"] = {p.name: sha(p) for p in case_root.glob("*.png")}
                    write(evidence, record)
                completed = [row for row in case["measurements"] if row.get("event") == "completed"]
                assert case["exitCode"] == 0 and case["inputUnchanged"] and len(completed) == 1, case
                assert completed[0]["bytes"] == size and completed[0]["language"] == language
                switches = [row for row in case["measurements"] if row.get("label") == "switch-syntax"]
                expected_images = 3+len({100,150,200,round(args.dpi*100)}) if args.dpi_cycle else 3
                assert len(switches) == 2 and len(case["images"]) == expected_images
                print(json.dumps({"language": language, "bytes": size, "checks": completed[0]["checks"],
                                  "elapsedSeconds": case["elapsedSeconds"], "exitCode": case["exitCode"]}), flush=True)
        record["status"] = "current-native-editor-observations-passed"
        return 0
    except BaseException as error:
        record["status"] = "native-observation-failed"
        record["error"] = str(error)
        raise
    finally:
        verify()
        record["protectedInputsUnchanged"] = True
        record["finishedUtc"] = datetime.now(timezone.utc).isoformat()
        write(evidence, record)
        print(json.dumps({"status": record["status"], "evidence": str(evidence)}, ensure_ascii=False), flush=True)


if __name__ == "__main__":
    raise SystemExit(main())
