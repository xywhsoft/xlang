"""Collect explicit frozen compiler sources without executing build providers."""
from pathlib import Path
import re

SOURCE_FORMAT = "xlang-compiler-source-materials-1"
BUILD_FORMAT = "xlang-compiler-source-build-evidence-1"
BINARIES = ("xl.exe", "xl.dll", "xlc.dll")


def valid_digest(value):
    return isinstance(value, str) and len(value) == 64 and all(c in "0123456789abcdef" for c in value)


def collect(manifest_path, build_report_path, compiler_base, runtime_inputs, *,
            add, load_json, plain, safe_name, sha, require, max_files):
    if not manifest_path:
        require(not build_report_path, "Compiler source build report needs a source manifest")
        return {"status": "explicit-source-manifest-not-supplied", "sourceFiles": 0,
                "rootBuildScripts": 0, "copiedFiles": 0, "relinkReviewComplete": False}
    base = Path(compiler_base).absolute()
    source_path = Path(manifest_path).absolute()
    info = load_json(source_path)
    require(info.get("format") == SOURCE_FORMAT, "Unsupported compiler source manifest")
    require(Path(info.get("directory", "")).resolve() == base.resolve(),
            "Compiler source provider does not match the runtime provider")
    source_files, scripts = info.get("files"), info.get("rootBuildScripts")
    require(isinstance(source_files, list) and source_files and isinstance(scripts, list) and scripts,
            "Compiler source manifest lacks sources or root build scripts")
    require(len(source_files) + len(scripts) < max_files, "Compiler source inventory exceeds budget")
    names, checked_parents, records = set(), set(), []
    for role, entries in (("source", source_files), ("root-build-script", scripts)):
        for row in entries:
            require(isinstance(row, dict), "Invalid compiler source record")
            name = safe_name(row.get("path", ""))
            digest = row.get("sha256")
            require(isinstance(digest, str) and len(digest) == 64 and all(c in "0123456789abcdef" for c in digest),
                    "Invalid compiler source digest")
            require(name.casefold() not in names, "Duplicate compiler source path")
            names.add(name.casefold())
            if role == "root-build-script":
                require("/" not in name and name.endswith(".bat"), "Invalid compiler root build script")
            else:
                require(name.startswith(("src/", "lib/", "res/", "tools/", "cgenrule/", "release/include/", "release/examples/", "test/cases/")),
                        "Compiler source record is outside selected source trees")
            source = base / name
            require(source.resolve().is_relative_to(base.resolve()), "Compiler source escaped provider")
            for parent in source.parents:
                if parent == base.parent:
                    break
                if parent not in checked_parents:
                    plain(parent, True)
                    checked_parents.add(parent)
            plain(source)
            add(source, "sources/xlang/" + name, digest)
            records.append({"path": name, "sha256": digest, "role": role})
    require({"build.bat", "build_dll.bat"}.issubset(names), "Compiler source build entry points are absent")
    original_path = Path(info.get("originalSourceManifest", ""))
    original_hash = info.get("originalSourceManifestSha256")
    require(valid_digest(original_hash), "Original compiler source digest is absent")
    plain(original_path)
    require(sha(original_path) == original_hash, "Original compiler source manifest differs")
    original = load_json(original_path)
    require(original.get("format") in ("xlang-ide-c-native-scalar-source-1", "xlang-ide-c-native-record-source-1") and
            Path(original.get("directory", "")).resolve() == base.resolve(),
            "Original compiler source provider differs")
    require(original.get("rootBuildScripts") == scripts, "Original compiler build scripts differ")
    old = {row["path"]: row["sha256"] for row in original["files"]}
    current = {row["path"]: row["sha256"] for row in source_files}
    require(len(old) == len(original["files"]) and all(current.get(name) == digest for name, digest in old.items()),
            "Original compiler source inventory differs")
    references = sorted(set(name.replace("\\", "/") for name in re.findall(
        r"(?:release\\examples|test\\cases)\\[A-Za-z0-9_]+\.c", (base / "build_dll.bat").read_text(encoding="utf8"))))
    supplemental = sorted(set(references) - set(old))
    require(info.get("supplementalBuildInputs") == supplemental and set(current) == set(old) | set(supplemental),
            "Compiler build dependency sources differ")
    add(original_path, "provenance/compiler-original-source-inputs.json", original_hash)
    manifest_hash = sha(source_path)
    add(source_path, "provenance/compiler-source-inputs.json", manifest_hash)
    modification = None
    if info.get("changes"):
        require(valid_digest(info.get("changesSha256")), "Compiler source changes digest is absent")
        add(Path(info["changes"]), "provenance/compiler-source-changes.json", info["changesSha256"])
        modification = "provenance/compiler-source-changes.json"
    artifacts = {name: runtime_inputs[name] for name in BINARIES}
    verification = {"status": "independent-build-evidence-not-supplied"}
    if build_report_path:
        report_path = Path(build_report_path).absolute()
        report = load_json(report_path)
        require(report.get("format") == BUILD_FORMAT and report.get("status") == "independent-build-passed",
                "Compiler source build evidence is not successful")
        require(report.get("sourceManifestSha256") == manifest_hash,
                "Compiler source build evidence has a different source manifest")
        require(report.get("runtimeArtifacts") == artifacts,
                "Compiler source build evidence does not match runtime artifacts")
        require(report.get("sourceFiles") == len(source_files) and report.get("rootBuildScripts") == len(scripts),
                "Compiler source build evidence inventory differs")
        rebuilt = report.get("rebuiltArtifacts")
        require(isinstance(rebuilt, dict) and set(rebuilt) == set(BINARIES), "Compiler rebuilt artifacts are incomplete")
        for value in rebuilt.values():
            require(isinstance(value, str) and len(value) == 64 and all(c in "0123456789abcdef" for c in value),
                    "Invalid rebuilt compiler digest")
        require(valid_digest(report.get("buildLogSha256")), "Compiler build log digest is absent")
        log = Path(report.get("buildLog", ""))
        require(valid_digest(report.get("actualRunSha256")) and valid_digest(report.get("actualInputsSha256")),
                "Compiler source build raw evidence digests are absent")
        run_path, inputs_path = Path(report.get("actualRun", "")), Path(report.get("actualInputs", ""))
        require(sha(run_path) == report["actualRunSha256"] and sha(inputs_path) == report["actualInputsSha256"],
                "Compiler source build raw evidence differs")
        actual, inputs = load_json(run_path), load_json(inputs_path)
        native_build = actual.get("status") == "full-compiler-build-passed"
        require((native_build and actual.get("actualBuildExitCode") == 0 and
                 actual.get("sourceChangesDuringBuild") == []) or
                (actual.get("status") == "actual-independent-compiler-source-build-passed" and actual.get("exitCode") == 0),
                "Compiler source raw build did not pass")
        require((native_build or actual.get("inputsSha256") == report["actualInputsSha256"]) and
                inputs.get("sourceManifestSha256") == manifest_hash and
                inputs.get("files") == source_files + scripts,
                "Compiler source raw build inputs differ")
        if native_build:
            inventory_path = Path(report.get("compilerSourceInventory", ""))
            require(valid_digest(report.get("compilerSourceInventorySha256")) and
                    sha(inventory_path) == report["compilerSourceInventorySha256"],
                    "Compiler source raw inventory differs")
            inventory = load_json(inventory_path)
            inventory_rows = inventory.get("files", [])
            require(Path(inventory.get("directory", "")).resolve() == base.resolve() and
                    actual.get("root") == str(base) and actual.get("recordsPrepared") == len(records) and
                    len(inventory_rows) == len(records) and
                    {r["path"]: r["sha256"] for r in inventory_rows} == {r["path"]: r["sha256"] for r in records},
                    "Compiler source raw inventory differs")
            add(inventory_path, "provenance/compiler-source-build-inventory.json", report["compilerSourceInventorySha256"])
        require(actual.get("logSha256") == report["buildLogSha256"], "Compiler source raw build log differs")
        outputs = ({str(base / "release" / name): digest for name, digest in actual.get("binaries", {}).items()}
                   if native_build else actual.get("artifacts", {}))
        require(len(outputs) == len(BINARIES) and {Path(p).name: digest for p, digest in outputs.items()} == rebuilt,
                "Compiler source raw build outputs differ")
        for path, digest in outputs.items():
            plain(Path(path))
            require(sha(Path(path)) == digest, "Compiler rebuilt output changed")
        add(run_path, "provenance/compiler-source-build-run.json", report["actualRunSha256"])
        add(inputs_path, "provenance/compiler-source-build-inputs.json", report["actualInputsSha256"])
        add(log, "provenance/compiler-source-build.log", report.get("buildLogSha256"))
        add(report_path, "provenance/compiler-source-build.json", sha(report_path))
        verification = {"status": "recorded-independent-source-build-passed",
                        "report": "provenance/compiler-source-build.json",
                        "log": "provenance/compiler-source-build.log",
                        "rebuiltArtifacts": rebuilt,
                        "byteIdenticalRuntimeArtifacts": rebuilt == artifacts}
    return {"status": "explicit-frozen-compiler-sources-copied", "sourceDirectory": "sources/xlang/",
            "sourceFiles": len(source_files), "rootBuildScripts": len(scripts), "copiedFiles": len(records),
            "sourceManifest": "provenance/compiler-source-inputs.json", "sourceManifestSha256": manifest_hash,
            "originalSourceManifest": "provenance/compiler-original-source-inputs.json",
            "originalSourceManifestSha256": original_hash, "supplementalBuildInputs": supplemental,
            "modificationMetadata": modification, "runtimeArtifacts": artifacts,
            "buildEntry": "sources/xlang/build.bat", "files": records,
            "independentBuild": verification, "relinkReviewComplete": False,
            "legalDistributionReviewComplete": False}


REBUILD_README = """# 随包编译器源码与独立构建

`xlang/` 保留显式冻结清单中的完整源码输入、构建资源、头文件种子及根构建脚本。
`xlang/SOURCE-MATERIALS.json` 将相对源码路径、摘要与随包 xl.exe/xl.dll/xlc.dll 的摘要关联。
`../provenance/compiler-source-inputs.json` 保留原始来源记录；最新修改描述另列于 provenance。
提供了独立构建报告时，报告与原始日志也保存在 provenance；该报告记录实际本机运行，不是发行签名。

构建需要 Windows x64、MinGW GCC 与 PowerShell。将整个 `xlang` 目录复制到新的可写工作目录，
打开该目录的命令行，运行 `cmd /d /c build.bat`。脚本从包含的 TCC 源码引导构建工具，
再生成 xl.dll、xlc.dll 和 xl.exe；生成结果位于该副本的 release，临时对象位于 build。
保持 XL6_* 构建环境覆盖未设置，采用所附脚本默认配置；源码中已有生成的词法/语法文件，
默认构建不要求重新生成语法。仅在源码副本操作，避免覆盖正在使用的随包工具链。

可以在副本中修改 lib/tcc 等源码后按同一入口重建；本材料不把重新编译描述为逐字节复现保证。
runtime 的实际行为与兼容范围仍按 IDE 验收记录核对。TCC 原始 COPYING 及其他已有文本按原字节保留。
这份源码、构建入口和日志提供技术材料；完整来源、分发条款及重链接许可核对仍单独进行，
relinkReviewComplete 与 legalDistributionReviewComplete 保持 false。
"""
