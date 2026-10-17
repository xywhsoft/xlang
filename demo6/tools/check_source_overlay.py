"""Windows native source overlay API and CLI contract gate (no third-party modules)."""
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
        raise RuntimeError("This gate requires Windows for the native DLL/Unicode path contract")
    root = args.root.resolve()
    compiler = (args.compiler or root / "release/xl.exe").resolve()
    gcc = shutil.which(args.gcc)
    if not gcc:
        raise RuntimeError("MinGW GCC is required")
    work = root / "build/test" / ("source-overlay-" + str(uuid.uuid4()))
    a, b = work / "覆盖 Ω", work / "other"
    a.mkdir(parents=True); b.mkdir()
    entry = "import dep\nfunc main(...) as int\n    return dep.value()\nend\n"
    good = "export func value() as int\n    return 7\nend\n"
    bad = "export func value() as int\n    return \"bad\"\nend\n"
    for directory in (a, b):
        (directory / "main.xl").write_text(entry, encoding="utf-8")
        (directory / "dep.xl").write_text(bad, encoding="utf-8")
    source_files = [a / "main.xl", a / "dep.xl", b / "main.xl", b / "dep.xl"]
    before = {str(path): digest(path) for path in source_files}
    core = [compiler, compiler.with_name("xlc.dll"), compiler.with_name("xl.dll")]
    tool_before = {str(path): digest(path) for path in core}
    results = []

    def run(name, command, cwd=root, expected=0, env=None):
        completed = subprocess.run([str(x) for x in command], cwd=cwd,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=90, env=env)
        (work / (name + ".stdout")).write_bytes(completed.stdout)
        (work / (name + ".stderr")).write_bytes(completed.stderr)
        results.append({"name": name, "exitCode": completed.returncode,
                        "stdoutBytes": len(completed.stdout), "stderrBytes": len(completed.stderr)})
        if completed.returncode != expected:
            raise RuntimeError(f"{name}: exit {completed.returncode}, expected {expected}\n"
                               + completed.stderr.decode("utf-8", errors="replace"))
        return completed

    started = datetime.datetime.now(datetime.timezone.utc).isoformat()
    status = "failed"
    try:
        native = work / "source_overlay_host.exe"
        run("compile-host", [gcc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-municode",
                             "-finput-charset=UTF-8", "-fexec-charset=UTF-8", "-I" + str(root / "release"),
                             root / "test/cases/source_overlay_host.c", "-o", native])
        run("native-api", [native, compiler.with_name("xlc.dll"), a, b])
        help_result = run("help", [compiler, "--help"])
        assert b"--source-overlay" in help_result.stdout
        snapshots = work / "snapshots"
        snapshots.mkdir()
        entry_snapshot = snapshots / "entry.utf8"
        dep_snapshot = snapshots / "dep.utf8"
        leaf_snapshot = snapshots / "leaf.utf8"
        entry_snapshot.write_text(entry, encoding="utf-8")
        dep_snapshot.write_text("import leaf\nexport func value() as int\n    return leaf.value()\nend\n", encoding="utf-8")
        leaf_snapshot.write_text(good, encoding="utf-8")
        arguments = ["--source-overlay", "virtual.xl", entry_snapshot,
                     "--source-overlay", "dep.xl", dep_snapshot,
                     "--source-overlay", "leaf.xl", leaf_snapshot]
        success = run("cli-recursive-virtual", [compiler, "check", "virtual.xl", "--json", *arguments], a)
        parsed = json.loads(success.stdout.decode("utf-8"))
        assert parsed["ok"] and not parsed["diagnostics"]
        assert not (a / "virtual.xl").exists() and not (a / "leaf.xl").exists()
        leaf_snapshot.write_text(bad, encoding="utf-8")
        failure = run("cli-leaf-error", [compiler, "check", "virtual.xl", "--json", *arguments], a, 1)
        parsed = json.loads(failure.stdout.decode("utf-8"))
        assert not parsed["ok"] and parsed["phase"] == "sem"
        assert parsed["diagnostics"] and all((a / x["file"]).resolve() == (a / "leaf.xl").resolve()
                                              for x in parsed["diagnostics"])
        assert parsed["diagnostics"][0]["line"] == 2
        leaf_snapshot.write_bytes(b"\xed\xa0\x80")
        invalid = run("cli-invalid-utf8", [compiler, "check", "virtual.xl", "--json", *arguments], a, 1)
        assert not invalid.stdout
        leaf_snapshot.write_bytes(b"a\x00b")
        nul = run("cli-nul", [compiler, "check", "virtual.xl", "--json", *arguments], a, 1)
        assert not nul.stdout
        run("cli-missing-value", [compiler, "check", "main.xl", "--source-overlay", "dep.xl"], a, 1)
        leaf_snapshot.write_bytes(b" " * (8 * 1024 * 1024 + 1))
        run("cli-oversize", [compiler, "check", "virtual.xl", "--json", *arguments], a, 1)
        leaf_snapshot.write_text(good, encoding="utf-8")
        duplicate = ["--source-overlay", "dep.xl", leaf_snapshot,
                     "--source-overlay", "DEP.XL", dep_snapshot,
                     "--source-overlay", "leaf.xl", leaf_snapshot]
        run("cli-alias-update", [compiler, "check", "main.xl", "--json", *duplicate], a)
        # A package's missing xlSources is provided by the same overlay table.
        package = a / "pkg"; package.mkdir()
        (package / "xlang.package.json").write_text(json.dumps({
            "format": "xlang-package-1", "name": "pkg", "version": "1.0.0", "abiVersion": "1",
            "kind": "source", "artifact": {"kind": "source", "xlSources": ["src/api.xl"]},
            "dependencies": [], "links": [], "loadlibs": []}), encoding="utf-8")
        package_entry = snapshots / "package-entry.utf8"
        package_entry.write_text("import pkg\nfunc main(...) as int\n    return pkg.value()\nend\n", encoding="utf-8")
        package_args = ["--source-overlay", "package-entry.xl", package_entry,
                        "--source-overlay", "pkg/src/api.xl", leaf_snapshot]
        run("cli-package-virtual-source", [compiler, "check", "package-entry.xl", "--json", *package_args], a)
        assert not (package / "src/api.xl").exists()
        leaf_snapshot.write_text(bad, encoding="utf-8")
        failure = run("cli-package-error", [compiler, "check", "package-entry.xl", "--json", *package_args], a, 1)
        parsed = json.loads(failure.stdout.decode("utf-8"))
        assert parsed["phase"] == "sem" and parsed["diagnostics"]
        assert all((a / x["file"]).resolve() == (package / "src/api.xl").resolve()
                   for x in parsed["diagnostics"])
        # Same positions/messages in independent files must remain two diagnostics.
        duplicate_entry = snapshots / "two-errors.utf8"
        duplicate_entry.write_text("import left\nimport right\nfunc main(...) as int\n    return left.value() + right.value()\nend\n", encoding="utf-8")
        duplicate_args = ["--source-overlay", "two-errors.xl", duplicate_entry,
                          "--source-overlay", "left.xl", leaf_snapshot,
                          "--source-overlay", "right.xl", leaf_snapshot]
        duplicate_result = run("cli-two-file-diagnostics", [compiler, "check", "two-errors.xl", "--json", *duplicate_args], a, 1)
        duplicate_diags = json.loads(duplicate_result.stdout.decode("utf-8"))["diagnostics"]
        assert len(duplicate_diags) == 2
        assert {Path(x["file"]).name.lower() for x in duplicate_diags} == {"left.xl", "right.xl"}
        # Nested source packages use one overlay throughout manifest dependency expansion.
        package2 = a / "pkg2"; package2.mkdir()
        package2_manifest = {"format": "xlang-package-1", "name": "pkg2", "version": "1.0.0", "abiVersion": "1",
                             "kind": "source", "artifact": {"kind": "source", "xlSources": ["src/api.xl"]},
                             "dependencies": [], "links": [], "loadlibs": []}
        (package2 / "xlang.package.json").write_text(json.dumps(package2_manifest), encoding="utf-8")
        manifest = json.loads((package / "xlang.package.json").read_text())
        manifest["dependencies"] = [{"name": "pkg2", "version": "1.0.0", "abiVersion": "1"}]
        (package / "xlang.package.json").write_text(json.dumps(manifest), encoding="utf-8")
        parent_snapshot = snapshots / "package-parent.utf8"
        parent_snapshot.write_text("import pkg2\nexport func value() as int\n    return pkg2.value()\nend\n", encoding="utf-8")
        leaf_snapshot.write_text(good, encoding="utf-8")
        graph_args = ["--source-overlay", "package-entry.xl", package_entry,
                      "--source-overlay", "pkg/src/api.xl", parent_snapshot,
                      "--source-overlay", "pkg2/src/api.xl", leaf_snapshot]
        package_env = dict(os.environ, XLANG_PACKAGE_PATH=str(a))
        run("cli-package-recursive-overlay", [compiler, "check", "package-entry.xl", "--json", *graph_args], a, env=package_env)
        assert not (package2 / "src/api.xl").exists()
        leaf_snapshot.write_text(bad, encoding="utf-8")
        graph_failure = run("cli-package-recursive-error", [compiler, "check", "package-entry.xl", "--json", *graph_args], a, 1, package_env)
        graph_diags = json.loads(graph_failure.stdout.decode("utf-8"))["diagnostics"]
        assert graph_diags and all((a / x["file"]).resolve() == (package2 / "src/api.xl").resolve() for x in graph_diags)
        # Compilation and generated output consume the same virtual entry/import text.
        leaf_snapshot.write_text(good, encoding="utf-8")
        entry_snapshot.write_text("import dep\nfunc main(...) as int\n    return dep.value() - 7\nend\n", encoding="utf-8")
        run("cli-cgen-overlay", [compiler, "cgen", "virtual.xl", "--emit", work / "virtual.c", *arguments], a)
        assert (work / "virtual.c").stat().st_size > 0
        run("cli-build-overlay", [compiler, "build", "virtual.xl", "-o", work / "virtual.exe", *arguments], a)
        run("cli-built-overlay-executable", [work / "virtual.exe"], a)
        run("cli-run-overlay", [compiler, "run", "virtual.xl", *arguments], a)
        # Restored disk errors also retain actual imported-file labels.
        disk = run("cli-disk-fallback", [compiler, "check", "main.xl", "--json"], a, 1)
        assert all((a / x["file"]).resolve() == (a / "dep.xl").resolve()
                   for x in json.loads(disk.stdout.decode("utf-8"))["diagnostics"])
        status = "passed"
    finally:
        after = {str(path): digest(path) for path in source_files}
        tool_after = {str(path): digest(path) for path in core}
        unchanged = before == after and tool_before == tool_after
        evidence = {"status": status, "startedUtc": started,
                    "completedUtc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                    "work": str(work), "compiler": str(compiler), "results": results,
                    "sourceBefore": before, "sourceAfter": after,
                    "toolchainBefore": tool_before, "toolchainAfter": tool_after,
                    "unchanged": unchanged}
        (work / "evidence.json").write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({"status": status, "evidence": str(work / "evidence.json"),
                          "cases": len(results), "unchanged": unchanged}, ensure_ascii=False))
        assert unchanged
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
