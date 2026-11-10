"""Build actual IDE-owned semantic color consumers against a qualified XGE SDK.

Runs in the real root runtime via owned executable aliases, without publishing
or replacing user programs. Keeps failed attempts, actual object dependencies
and optional C mutation counterexamples. Not an end-to-end performance gate.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def dependencies(body):
    body = body.replace("\r\n", "\n").replace("\\\n", "").split(":", 1)[1]
    words, current, at = [], [], 0
    while at < len(body):
        char = body[at]
        if char == "\\" and at + 1 < len(body) and body[at + 1] in " \t\\#:":
            at += 1
            current.append(body[at])
        elif char.isspace():
            if current:
                words.append(Path("".join(current)))
                current = []
        else:
            current.append(char)
        at += 1
    if current:
        words.append(Path("".join(current)))
    return words


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--gui-receipt", required=True, type=Path)
    p.add_argument("--runtime", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path)
    p.add_argument("--adversarial", action="store_true")
    args = p.parse_args()
    root = Path(__file__).resolve().parents[2]
    runtime = args.runtime.resolve(strict=True)
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    receipt = json.loads(args.gui_receipt.read_text(encoding="utf-8-sig"))
    assert receipt["candidateOnly"] and not receipt["cLspEnabled"]
    flags = ["-I" + str(root / "ide/src"), *receipt["flags"]]
    libraries = receipt["libraries"]
    compiler = receipt["compiler"]
    protected = {str(runtime / name): sha(runtime / name) for name in
                 ("xlang_ide.exe", "xlang-ls.exe", "xl.exe", "xl.dll", "xlc.dll", "xge.dll")}
    assert sha(Path(libraries[0]).with_name("xge.dll")) == protected[str(runtime / "xge.dll")]
    commands, rows, runs, mutants = [], [], [], []
    report = dict(status="failed", commands=commands, consumers=rows, runs=runs,
                  mutants=mutants, protected=protected, receipt=str(args.gui_receipt.resolve()),
                  receiptSha256=sha(args.gui_receipt), scope=__doc__)
    owned = []

    def command(name, cmd):
        commands.append(cmd)
        began = time.perf_counter()
        value = subprocess.run(cmd, capture_output=True, timeout=120)
        (out / (name + ".log")).write_bytes(value.stdout + value.stderr)
        return value, (time.perf_counter() - began) * 1000

    def compile_source(name, source, extra=()):
        obj = out / (name + ".o")
        probe = [compiler, *extra, *flags, "-MM", "-MT", "colorConsumer", str(source)]
        z, _ = command(name + "-dependencies", probe)
        assert not z.returncode, z.stderr.decode(errors="replace")
        inputs = {str(x.resolve()): sha(x) for x in dependencies(z.stdout.decode())}
        cmd = [compiler, *extra, *flags, "-c", str(source), "-o", str(obj)]
        z, _ = command(name + "-compile", cmd)
        assert not z.returncode, z.stderr.decode(errors="replace")
        for path, h in inputs.items():
            assert sha(path) == h, path
        rows.append(dict(name=name, command=cmd, dependencies=inputs, object=str(obj), sha256=sha(obj)))
        return obj

    def link_and_run(name, objects, expected=True):
        target = out / (name + ".exe")
        cmd = [compiler, *map(str, objects), *libraries, "-o", str(target)]
        z, _ = command(name + "-link", cmd)
        assert not z.returncode, z.stderr.decode(errors="replace")
        alias = runtime / (".xide-" + name + "-" + str(time.time_ns()) + ".exe")
        assert not alias.exists()
        shutil.copy2(target, alias)
        owned.append((alias, sha(target)))
        assert sha(alias) == sha(target)
        z, ms = command(name + "-run", [str(alias)])
        runs.append(dict(name=name, executable=str(target), sha256=sha(target),
                         alias=str(alias), exit=z.returncode, milliseconds=ms,
                         stdout=z.stdout.decode(errors="replace"), expectedPass=expected))
        assert (z.returncode == 0) == expected, (name, z.stdout, z.stderr)
        assert sha(alias) == sha(target)
        alias.unlink()
        print("PASS" if expected else "DETECTED", name, flush=True)
        return target

    try:
        for path, h in receipt["inputs"].items():
            assert sha(path) == h, path
        text = compile_source("text", root / "ide/src/analysis/text.c")
        color = compile_source("semantic_color_result", root / "ide/src/services/semantic_color_result.c")
        regular = compile_source("semantic_color_result_test", root / "ide/tests/semantic_color_result_test.c")
        link_and_run("semantic_color_result_test", [text, color, regular])
        for name in ("semantic_color_fault_test", "semantic_color_worker_test"):
            test = compile_source(name, root / ("ide/tests/" + name + ".c"))
            link_and_run(name, [text, test])
        if args.adversarial:
            original = (root / "ide/src/services/semantic_color_result.c").read_bytes()
            changes = [
                ("surrogate", b"if(c->line==line && units>column-c->column)return XIDE_COLOR_COORDINATE;", b"/* omitted UTF16 interior proof */", "semantic_color_result_test"),
                ("base-legend", b"!sameLegend(&base->legend,&r->legend)", b"(sameLegend(&base->legend,&r->legend) && 0)", "semantic_color_result_test"),
                ("unaligned-poll", b"if(c->sincePoll>=4096)", b"if(!(c->at%4096))", "semantic_color_fault_test"),
                ("owner-retirement", b"d->retired=result;xrtCondSignal(&d->changed);", b"xideColorResultRelease(result);xrtCondSignal(&d->changed);", "semantic_color_worker_test"),
            ]
            for name, before, after, test_name in changes:
                assert original.count(before) == 1
                folder = out / ("mutant-" + name)
                source = folder / "services/semantic_color_result.c"
                source.parent.mkdir(parents=True)
                source.write_bytes(original.replace(before, after, 1))
                extra = ["-I" + str(folder)]
                if test_name == "semantic_color_result_test":
                    obj = compile_source("mutant-" + name, source)
                    objects = [text, obj, regular]
                else:
                    obj = compile_source("mutant-" + name, root / ("ide/tests/" + test_name + ".c"), extra)
                    objects = [text, obj]
                link_and_run("mutant-" + name, objects, expected=False)
                mutants.append(dict(name=name, originalSha256=sha(root / "ide/src/services/semantic_color_result.c"),
                                    changedSource=str(source), changedSha256=sha(source),
                                    before=before.decode(), after=after.decode(), fixture=test_name))
        for row in rows:
            assert sha(row["object"]) == row["sha256"]
            for path, h in row["dependencies"].items():
                assert sha(path) == h, path
        report["status"] = "passed"
    finally:
        for alias, h in owned:
            if alias.exists():
                assert sha(alias) == h
                alias.unlink()
        for path, h in protected.items():
            assert sha(path) == h, path
        (out / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
