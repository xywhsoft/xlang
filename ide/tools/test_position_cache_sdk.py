"""Compare the original and cached Document models against a private SDK build."""
import argparse
import json
import shutil
import subprocess
from pathlib import Path

from prepare_large_source_sdk import ancestors, name, plain, read, sha

IDE = Path(__file__).resolve().parents[1]
BUNDLE = IDE / "third_party/sdk_patches/large-source-experiment-v3"


def run(build_report, output, gcc):
    build_report = build_report.absolute()
    output = output.absolute()
    build = read(build_report)
    if build["status"] != "sdk-build-finished" or build["exitCode"] != 0:
        raise ValueError("A completed private SDK build is required")
    source = Path(build["source"]).absolute()
    sdk = source / "build/ide"
    outputs = {Path(row["path"]).absolute(): row["sha256"] for row in build["outputs"]}
    if any(path not in outputs for path in (sdk / "xge.dll", sdk / "xge.lib")):
        raise ValueError("Build report must bind this SDK's DLL and import library")
    ancestors(source)
    ancestors(BUNDLE)
    ancestors(output.parent)
    if output.exists() or output.is_relative_to(source) or output.is_relative_to(BUNDLE):
        raise ValueError("Use a new output directory outside both input trees")
    manifest = read(BUNDLE / "patch-manifest.json")
    descriptor = BUNDLE / "source-materials-before.json"
    if sha(descriptor) != manifest["beforeDescriptorSha256"]:
        raise ValueError("Original source descriptor differs")
    materials = {row["path"]: row["sha256"] for row in read(descriptor)["files"]}
    materials.update({row["path"]: row["afterSha256"] for row in manifest["changes"]})
    tests = manifest["equivalenceTestMaterials"]

    def verify():
        for relative, digest in materials.items():
            path = source / name(relative)
            plain(path)
            if sha(path) != digest:
                raise ValueError(f"Private source differs: {relative}")
        for row in tests:
            path = BUNDLE / name(row["path"])
            plain(path)
            if sha(path) != row["sha256"]:
                raise ValueError(f"Test material differs: {row['path']}")
        for row in build["outputs"]:
            path = Path(row["path"])
            plain(path)
            if sha(path) != row["sha256"]:
                raise ValueError(f"Private build output differs: {path}")

    verify()
    output.mkdir()
    for row in tests:
        target = output / Path(row["path"]).name
        shutil.copy2(BUNDLE / row["path"], target)
    shutil.copy2(sdk / "xge.dll", output / "xge.dll")
    executable = output / "position_test.exe"
    arguments = [str(gcc), "-O2", "-Wall", "-Wextra", "-Werror",
                 "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-cast-function-type",
                 "-DXGE_DLL", "-DXGE_DEBUGMODE=0", "-DXGE_XRT_PROFILE_IDE",
                 "-I" + str(source), "-I" + str(source / "src"),
                 str(output / "position_test.c"), str(output / "old_model.c"),
                 str(output / "new_model.c"), str(source / "src/xui_unicode.c"),
                 str(sdk / "xge.lib"), "-lm", "-lws2_32", "-liphlpapi", "-lgdi32",
                 "-luser32", "-lshell32", "-lopengl32", "-lole32", "-lwinmm", "-lavrt",
                 "-o", str(executable)]
    report = {"status": "running", "buildReport": str(build_report),
              "buildReportSha256": sha(build_report), "patchManifestSha256": sha(BUNDLE / "patch-manifest.json"),
              "arguments": arguments, "root": str(output), "sourceFiles": len(materials),
              "testMaterials": tests, "sdkDllSha256": sha(output / "xge.dll"),
              "oneXrtOwner": "private xge.dll", "sdkInstalled": False, "productionChanged": False}
    pointer = output / "result.json"

    def save():
        pointer.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf8")

    save()
    try:
        link_log = output / "strict-link.log"
        with link_log.open("xb") as stream:
            linked = subprocess.run(arguments, stdout=stream, stderr=stream, timeout=60)
        report.update(linkExitCode=linked.returncode, linkLogSha256=sha(link_log))
        save()
        if linked.returncode != 0:
            raise RuntimeError("Strict comparison fixture build failed; see strict-link.log")
        log = output / "actual-test.log"
        with log.open("xb") as stream:
            tested = subprocess.run([str(executable)], cwd=output, stdout=stream, stderr=stream, timeout=60)
        report.update(exitCode=tested.returncode, executableSha256=sha(executable), logSha256=sha(log),
                      status="position-cache-equivalence-passed" if tested.returncode == 0 else "position-cache-equivalence-failed")
        save()
        if tested.returncode != 0:
            raise RuntimeError("Model comparison failed; see actual-test.log")
        print(log.read_text(encoding="utf8"), end="")
        return report
    except Exception as error:
        report.update(status="position-cache-equivalence-failed", error=str(error))
        raise
    finally:
        verify()
        report["inputsUnchanged"] = True
        save()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-report", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--gcc", type=Path, default=Path(r"E:\software\w64devkit\bin\gcc.exe"))
    options = parser.parse_args()
    run(options.build_report, options.output, options.gcc)
