"""Native Process contracts and allocation prefixes on actual shared products.

Run/Command, Pipeline, live spawn/streams/status, File and public Future factories use the
ordinary DLL/SO, never a source implementation. Preserve existing test bodies;
source-only private mapper and POSIX-plan units are not claimed here. Exact
snapshot layout permits a test-owned real waiter join before heap comparison.
Keep products loaded. No worker-wide injection, OS fault seam or full53 claim.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from stage_stdlib_process_observer import stage


HOSTS = {
    'run': ('xlir_process_run_host.c', (
        rb'process configuration OOM: (\d+)/(\d+)/(\d+) triggers and 3 first no-hit completions, balanced',
        rb'native run complete prefix: (\d+) failures and 1 first no-hit, balanced')),
    'pipeline': ('xlir_process_pipeline_host.c', (
        rb'pipeline resident prefixes: (\d+)/(\d+)/(\d+)/(\d+)/(\d+) failures \+ 5 first no-hit',
        rb'pipeline projections: (\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+) failures \+ 6 first no-hit')),
    'live': ('xlir_process_live_host.c', (
        rb'live product spawn OOM (\d+) failures \+ 1 first no-hit, waiter retired, balanced',
        rb'reader/writer OOM (\d+)/(\d+) failures \+ 2 first no-hit, balanced',)),
    'future': ('xlir_process_future_host.c', (
        rb'pending/terminal OOM (\d+)/(\d+) failures \+ 2 first no-hit, waiter retired, balanced',)),
    'file': ('xlir_process_file_host.c', (
        rb'OS error prefix (\d+) failures \+ 1 first no-hit, heap/handles balanced',)),
}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'build-report', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists()
    windows = os.name == 'nt'
    cc, inspector, nm = [shutil.which(n) for n in ('gcc', 'objdump' if windows else 'readelf', 'nm')]
    assert cc and inspector and nm
    task = Path(__file__).resolve().parents[1]
    header = task / 'test/support/process_product_observer.h'
    child_source = task / 'test/cases/xlir_process_child.c'
    sources = {name: task / 'test/cases' / spec[0] for name, spec in HOSTS.items()}
    build = json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit'] == 0
    products = [root / 'release' / name for name in
                (('xl.dll', 'xlc.dll') if windows else ('libxl.so', 'libxlc.so'))]
    recorded = {Path(r['Path']).resolve(): r['Sha256'] for r in build['Products']}
    assert all(recorded.get(p) == sha(p) for p in products)
    files = {Path(__file__).resolve(), task / 'tools/stage_stdlib_process_observer.py',
             Path(sys.executable), Path(cc), Path(inspector), Path(nm),
             args.build_report.resolve(), header, child_source, *sources.values()}
    for row in build['Inputs'] + build['Products']:
        assert sha(row['Path']) == row['Sha256'], row['Path']
        files.add(Path(row['Path']))
    inputs = [dict(Path=str(p), Sha256=sha(p)) for p in sorted(files)]
    out.mkdir()
    isolated = out / 'host'
    release = isolated / 'release'
    release.mkdir(parents=True)
    observer = stage(root, out / 'observer')
    for product in products:
        shutil.copy2(product, release / product.name)
        if windows:
            shutil.copy2(product, isolated / product.name)
    environment = {k: v for k, v in os.environ.items()
                   if not k.upper().startswith('XL6_') and k not in
                   ('LD_LIBRARY_PATH', 'LD_PRELOAD', 'LD_AUDIT', 'CPATH', 'C_INCLUDE_PATH', 'LIBRARY_PATH')}
    rows, proofs, failures, controls = [], [], [], []
    report = dict(Complete=False, InputsUnchanged=False, Inputs=inputs, Rows=rows,
                  Proofs=proofs, Failures=failures, Controls=controls, Scope=__doc__, Observer=observer,
                  RuntimeInstrumented=False, ProviderOverrides=False, CompilerSourceObjects=False,
                  PrivateLayoutFromPinnedSnapshot=True, ProcessProductPrefixAcceptance=False,
                  PrivateMapperAcceptance=False, PrivatePosixPlanAcceptance=False,
                  WorkerWideOomAcceptance=False, Full53Acceptance=False)

    def run(name, argv, reject=False):
        started = time.monotonic()
        argv = list(map(str, argv))
        try:
            result = subprocess.run(argv, cwd=isolated, env=environment,
                                    capture_output=True, timeout=180)
            code, data = result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired as error:
            code, data = 'timeout', (error.stdout or b'') + (error.stderr or b'')
        log = out / (name + '.log')
        log.write_bytes(data)
        rows.append(dict(Name=name, Command=argv, Exit=code, ExpectedRejection=reject, Log=str(log),
                         LogSha256=sha(log), Seconds=time.monotonic() - started))
        print(name + ': ' + str(code), flush=True)
        assert (code not in (0, 'timeout')) if reject else code == 0, (name, data.decode(errors='replace')[-6000:])
        return data

    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
             '-DXL6_PROCESS_PRODUCT_OBSERVER', '-I' + str(root), '-I' + str(root / 'release'),
             '-I' + str(root / 'release/include'), '-I' + str(out / 'observer'),
             '-include', str(header), '-Wno-unused-function']
    if not windows:
        flags += ['-D_GNU_SOURCE']
    try:
        child = isolated / ('process-child.exe' if windows else 'process-child')
        run('child.build', [cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-O2',
                            *(['-municode'] if windows else ['-D_GNU_SOURCE']), child_source, '-o', child])
        for product in products:
            links = [release / product.name] if windows else [
                '-L' + str(release), '-l' + product.stem[3:], '-Wl,-rpath,$ORIGIN/release', '-pthread', '-ldl', '-lm']
            for opt in ('O0', 'O2'):
                for name, source in sources.items():
                    tag = product.stem + '.' + opt + '.' + name
                    binary = isolated / (tag + ('.exe' if windows else ''))
                    try:
                        run(tag + '.build', [cc, *flags, '-' + opt, source, *links, '-o', binary])
                        data = run(tag + '.imports', [inspector, '-p' if windows else '-d', binary])
                        dependencies = re.findall(rb'DLL Name:\s*([^\s]+)', data) if windows else re.findall(rb'Shared library:\s*\[([^\]]+)\]', data)
                        actual = {p.decode().lower() for p in dependencies} & {p.name.lower() for p in products}
                        assert actual == {product.name.lower()}, actual
                        definitions = run(tag + '.definitions', [nm, '--defined-only', binary])
                        assert not re.search(rb'\b[TDB] (?:xrtProcess|xl6Process|xl6Pipeline|xrtMemDebug)\w*$', definitions, re.M)
                        data = run(tag + '.run', [binary, child])
                        prefixes = []
                        for expression in HOSTS[name][1]:
                            found = re.findall(expression, data)
                            assert len(found) == 1, (name, expression, data[-2000:])
                            values = found[0] if isinstance(found[0], tuple) else (found[0],)
                            counts = list(map(int, values))
                            assert all(n > 0 for n in counts)
                            prefixes.extend(counts)
                        proofs.append(dict(Product=product.name, Optimization=opt, Family=name,
                                           CompletePrefixes=prefixes, HeapBalanced=True,
                                           ActualProductImports=True, RuntimeDefinitionsInHost=False))
                    except Exception as error:
                        failures.append(dict(Name=tag, Error=repr(error)))
        for product in products:
            links = [release / product.name] if windows else [
                '-L' + str(release), '-l' + product.stem[3:], '-Wl,-rpath,$ORIGIN/release', '-pthread', '-ldl', '-lm']
            for label, family, before, after, diagnostic in (
                    ('no-failpoint', 'run', 'assert(xrtMemDebugFailAfter(point));', '(void)point;', rb'hits'),
                    ('leaked-stage', 'pipeline', 'xl6ProcessStageDestroyV1(resource)', '(void)(resource)', rb'LiveCount')):
                tag = product.stem + '.control.' + label
                try:
                    body = sources[family].read_text(encoding='utf-8-sig')
                    assert before in body
                    source = out / (tag + '.c')
                    source.write_text(body.replace(before, after), encoding='utf-8')
                    binary = isolated / (tag + ('.exe' if windows else ''))
                    run(tag + '.build', [cc, *flags, '-O2', source, *links, '-o', binary])
                    data = run(tag + '.run', [binary, child], reject=True)
                    assert re.search(rb'assert.*fail', data, re.I) and diagnostic in data, data[-3000:]
                    controls.append(dict(Product=product.name, Kind=label, Compiled=True, AssertionRejected=True))
                except Exception as error:
                    failures.append(dict(Name=tag, Error=repr(error)))
        report['InputsUnchanged'] = all(sha(r['Path']) == r['Sha256'] for r in inputs)
        report['ProcessProductPrefixAcceptance'] = len(proofs) == 20 and len(controls) == 4 and not failures
        report['Complete'] = report['InputsUnchanged'] and report['ProcessProductPrefixAcceptance']
    except Exception as error:
        failures.append(dict(Name='gate', Error=repr(error)))
    report['PhaseCount'] = len(rows)
    report['Products'] = [dict(Path=str(p), Sha256=sha(p)) for p in sorted(out.rglob('*')) if p.is_file()]
    (out / 'results.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'], Proofs=len(proofs), Failures=failures)), flush=True)
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
