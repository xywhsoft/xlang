"""Windows native loaded-source report contract gate; standard library only."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import uuid


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--compiler", type=Path)
    parser.add_argument("--gcc", default="gcc")
    args = parser.parse_args()
    if os.name != "nt":
        raise RuntimeError("This gate requires the Windows native DLL and Unicode contract")
    root = args.root.resolve()
    compiler = (args.compiler or root / "release/xl.exe").resolve()
    gcc = shutil.which(args.gcc)
    if not gcc:
        raise RuntimeError("MinGW GCC is required")
    work = root / "build/test" / ("source-report-" + str(uuid.uuid4()))
    src, snapshots, mutations = work / "源码 Ω", work / "snapshots", work / "mutations"
    src.mkdir(parents=True); snapshots.mkdir(); mutations.mkdir()
    entry = "import dep\nfunc main(...) as int\n    return dep.value()\nend\n"
    good = "export func value() as int\n    return 7\nend\n"
    bad = "export func value() as int\n    return \"bad\"\nend\n"
    recursive = "import leaf\nexport func value() as int\n    return leaf.value()\nend\n"
    (src / "main.xl").write_bytes(b"\xef\xbb\xbf" + entry.replace("\n", "\r\n").encode())
    (src / "dep.xl").write_bytes(good.encode())
    (src / "leaf.xl").write_bytes(good.encode())
    core = [compiler, compiler.with_name("xlc.dll"), compiler.with_name("xl.dll")]
    protected = [src / "main.xl", src / "dep.xl", src / "leaf.xl", *core]
    before = {str(p): digest(p) for p in protected}
    results = []
    environment = dict(os.environ, PATH=str(compiler.parent) + os.pathsep + os.environ.get("PATH", ""))
    started = datetime.datetime.now(datetime.timezone.utc).isoformat()
    status = "failed"

    def run(name, command, expected=0, cwd=src, env=environment):
        completed = subprocess.run([str(x) for x in command], cwd=cwd, env=env,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=90)
        (work / (name + ".stdout")).write_bytes(completed.stdout)
        (work / (name + ".stderr")).write_bytes(completed.stderr)
        results.append({"name": name, "exitCode": completed.returncode,
                        "stdoutBytes": len(completed.stdout), "stderrBytes": len(completed.stderr)})
        if completed.returncode != expected:
            raise RuntimeError(f"{name}: exit {completed.returncode}, expected {expected}\n"
                               + completed.stderr.decode("utf-8", errors="replace"))
        return completed

    def report(parsed, expected, complete=True):
        value = parsed["sourceReport"]
        assert value["schema"] == 2 and value["scope"] == "loaded-sources-and-manifests"
        assert value["complete"] is complete and isinstance(value["error"], str)
        if not complete:
            assert value["error"] and value["errorFile"]
            return value
        assert not value["error"] and not value["errorFile"]
        assert len(value["files"]) == len(expected)
        actual = {Path(item["path"]).resolve(): item for item in value["files"]}
        assert len(actual) == len(expected) and set(actual) == set(expected)
        for path, (origin, kind, raw, text) in expected.items():
            item = actual[path]
            assert Path(item["path"]).is_absolute() and item["origin"] == origin and item["kind"] == kind
            assert item["rawSize"] == len(raw) and item["textSize"] == len(text)
            assert item["rawSha256"] == hashlib.sha256(raw).hexdigest()
            assert item["textSha256"] == hashlib.sha256(text).hexdigest()
            metadata = item["metadata"]
            assert len(metadata["device"]) == 16 and len(metadata["identity"]) == 16
            int(metadata["device"], 16); int(metadata["identity"], 16)
            int(metadata["modifiedMs"]); int(metadata["changedMs"])
            if origin == "disk":
                assert metadata["available"] & 0x49 == 0x49 and int(metadata["identity"], 16)
            else:
                assert metadata["available"] == 0 and int(metadata["identity"], 16) == 0
        return value

    def disk(path, kind=1):
        raw = path.read_bytes()
        return ("disk", kind, raw, raw.removeprefix(b"\xef\xbb\xbf"))

    def overlay(text):
        data = text.encode()
        return ("overlay", 1, data, data)

    def check(name, path, extra=(), expected=0, env=environment):
        completed = run(name, [compiler, "check", path, "--json", "--source-report", *extra], expected, env=env)
        parsed = json.loads(completed.stdout.decode("utf-8"))
        assert parsed["ok"] is (expected == 0) and isinstance(parsed["diagnostics"], list)
        return parsed

    try:
        flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-municode",
                 "-finput-charset=UTF-8", "-fexec-charset=UTF-8", "-I" + str(root), "-I" + str(root / "release")]
        run("compile-snapshot-host", [gcc, *flags, root / "test/cases/source_report_host.c",
                                     root / "src/compile/xl6_source_report.c", compiler.with_name("xl.dll"),
                                     "-o", work / "source_report_host.exe"], cwd=root)
        run("native-snapshot-contract", [work / "source_report_host.exe", mutations])
        run("compile-api-host", [gcc, *flags, root / "test/cases/source_report_api_host.c",
                                "-o", work / "source_report_api_host.exe"], cwd=root)
        run("native-public-api", [work / "source_report_api_host.exe", compiler.with_name("xlc.dll"), src])
        help_result = run("help", [compiler, "--help"])
        assert b"--source-report" in help_result.stdout
        disk_expected = {(src / "main.xl").resolve(): disk(src / "main.xl"),
                         (src / "dep.xl").resolve(): disk(src / "dep.xl")}
        report(json.loads((src / "api-disk.json").read_bytes()), disk_expected)
        memory_expected = {(src / "memory-main.xl").resolve(): ("memory", 1, entry.encode(), entry.encode()),
                           (src / "dep.xl").resolve(): disk(src / "dep.xl")}
        report(json.loads((src / "api-memory.json").read_bytes()), memory_expected)
        changed = dict(memory_expected); changed[(src / "dep.xl").resolve()] = overlay(bad)
        report(json.loads((src / "api-overlay-bad.json").read_bytes()), changed)
        report(json.loads((src / "api-isolated.json").read_bytes()), disk_expected)
        report(json.loads((src / "api-thread-a.json").read_bytes()), changed)
        report(json.loads((src / "api-thread-b.json").read_bytes()), disk_expected)
        report(json.loads((src / "api-clear.json").read_bytes()), memory_expected)
        assert "sourceReport" not in json.loads((src / "api-original.json").read_bytes())
        report(check("cli-disk-bom-crlf", "main.xl"), disk_expected)
        old = json.loads(run("cli-original-json", [compiler, "check", "main.xl", "--json"]).stdout)
        assert set(old) == {"ok", "phase", "diagnostics"}
        for name, text in [("entry", entry), ("dep", recursive), ("leaf", bad), ("unused", "ignored")]:
            (snapshots / (name + ".utf8")).write_bytes(text.encode())
        extra = ["--source-overlay", "virtual.xl", snapshots / "entry.utf8",
                 "--source-overlay", "dep.xl", snapshots / "dep.utf8",
                 "--source-overlay", "unused.xl", snapshots / "unused.utf8"]
        virtual_expected = {(src / "virtual.xl").resolve(): overlay(entry),
                            (src / "dep.xl").resolve(): overlay(recursive),
                            (src / "leaf.xl").resolve(): disk(src / "leaf.xl")}
        report(check("cli-recursive-mixed", "virtual.xl", extra), virtual_expected)
        extra += ["--source-overlay", "leaf.xl", snapshots / "leaf.utf8"]
        virtual_expected[(src / "leaf.xl").resolve()] = overlay(bad)
        parsed = check("cli-recursive-sem-error", "virtual.xl", extra, 1)
        report(parsed, virtual_expected)
        assert parsed["diagnostics"] and all(Path(x["file"]).name.lower() == "leaf.xl" for x in parsed["diagnostics"])
        (snapshots / "leaf.utf8").write_bytes(good.encode())
        virtual_expected[(src / "leaf.xl").resolve()] = overlay(good)
        alias = extra + ["--source-overlay", "LEAF.XL", snapshots / "leaf.utf8"]
        report(check("cli-overlay-case-alias", "virtual.xl", alias), virtual_expected)
        assert not (src / "virtual.xl").exists() and not (src / "unused.xl").exists()
        parse_text = "func main(\n"
        (snapshots / "parse.utf8").write_bytes(parse_text.encode())
        parsed = check("cli-parse-error", "parse.xl", ["--source-overlay", "parse.xl", snapshots / "parse.utf8"], 1)
        assert parsed["phase"] == "parse"
        report(parsed, {(src / "parse.xl").resolve(): overlay(parse_text)})
        for name, data in [("bad-utf8", b"\xed\xa0\x80"), ("nul", b"a\0b"),
                           ("utf16", b"\xff\xfea\0"), ("oversize", b" " * (8*1024*1024+1))]:
            path = mutations / (name + ".xl"); path.write_bytes(data)
            parsed = check("cli-" + name, path, expected=1)
            assert parsed["phase"] == "source-report"
            report(parsed, {}, False)
        # Both repeated manifest loads and recursive dependency resolution
        # must use one report and one immutable manifest snapshot.
        for name in ["pkg", "pkg2"]:
            package = src / name; (package / "src").mkdir(parents=True)
            manifest = {"format": "xlang-package-1", "name": name, "version": "1.0.0", "abiVersion": "1",
                        "kind": "source", "artifact": {"kind": "source", "xlSources": ["src/api.xl"]},
                        "dependencies": [], "links": [], "loadlibs": []}
            if name == "pkg":
                manifest["dependencies"] = [{"name": "pkg2", "version": "1.0.0", "abiVersion": "1"}]
            (package / "xlang.package.json").write_bytes(json.dumps(manifest).encode())
        package_entry = "import pkg\nfunc main(...) as int\n    return pkg.value()\nend\n"
        package_parent = "import pkg2\nexport func value() as int\n    return pkg2.value()\nend\n"
        for name, text in [("package-entry", package_entry), ("package-parent", package_parent)]:
            (snapshots / (name + ".utf8")).write_bytes(text.encode())
        package_extra = ["--source-overlay", "package-entry.xl", snapshots / "package-entry.utf8",
                         "--source-overlay", "pkg/src/api.xl", snapshots / "package-parent.utf8",
                         "--source-overlay", "pkg2/src/api.xl", snapshots / "leaf.utf8"]
        package_expected = {(src / "package-entry.xl").resolve(): overlay(package_entry),
                            (src / "pkg/src/api.xl").resolve(): overlay(package_parent),
                            (src / "pkg2/src/api.xl").resolve(): overlay(good)}
        for name in ["pkg", "pkg2"]:
            p = src / name / "xlang.package.json"; package_expected[p.resolve()] = disk(p, 2)
        package_env = dict(environment, XLANG_PACKAGE_PATH=str(src))
        report(check("cli-recursive-package-manifests", "package-entry.xl", package_extra, env=package_env), package_expected)
        (snapshots / "leaf.utf8").write_bytes(bad.encode())
        package_expected[(src / "pkg2/src/api.xl").resolve()] = overlay(bad)
        parsed = check("cli-package-sem-error", "package-entry.xl", package_extra, 1, package_env)
        report(parsed, package_expected)
        assert all(Path(x["file"]).resolve() == (src / "pkg2/src/api.xl").resolve() for x in parsed["diagnostics"])
        rejected = run("cli-report-requires-json", [compiler, "check", "main.xl", "--source-report"], 1)
        assert not rejected.stdout and b"requires" in rejected.stderr
        rejected = run("cli-report-only-check", [compiler, "run", "main.xl", "--source-report"], 1)
        assert b"only" in rejected.stderr
        (src / "--source-report").write_bytes(b"func main(...) as int\n    return 0\nend\n")
        run("cli-snapshot-report-named-value", [compiler, "run", "main.xl", "--source-overlay", "main.xl", "--source-report"])
        (src / "--json").write_bytes(entry.encode())
        rejected = run("cli-snapshot-json-is-not-flag", [compiler, "check", "virtual.xl", "--source-report",
                       "--source-overlay", "virtual.xl", "--json"], 1)
        assert not rejected.stdout and b"requires" in rejected.stderr
        ignored = json.loads(run("cli-report-after-delimiter", [compiler, "check", "main.xl", "--json", "--", "--source-report"]).stdout)
        assert "sourceReport" not in ignored
        status = "passed"
    finally:
        after = {str(p): digest(p) for p in protected}
        evidence = {"status": status, "startedUtc": started,
                    "completedUtc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                    "work": str(work), "compiler": str(compiler), "results": results,
                    "protectedBefore": before, "protectedAfter": after, "unchanged": before == after,
                    "scope": "loaded-sources-and-manifests", "guiUnsavedDependencies": False}
        (work / "evidence.json").write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({"status": status, "evidence": str(work / "evidence.json"),
                          "cases": len(results), "unchanged": before == after}, ensure_ascii=False))
        assert before == after
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
