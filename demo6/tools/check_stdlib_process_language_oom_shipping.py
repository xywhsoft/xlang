"""Complete emitted-language Process allocation prefixes on actual DLL/SO.

Preserve all eleven historical Run/Pipeline/File/live/terminal/Future fixtures.
Use unmodified C emitted by the real compiler, no production implementation
objects. Transparent GNU call-site observation forwards to the actual resident
Spawn and outcome entry before managed boxing or cancellation diagnostics.
This is observed emitted-C execution, NOT unobserved JIT, trimmed AOT, provider
unload, other-thread/scheduler-wide OOM or full53 completion.
"""
from pathlib import Path
import argparse
import concurrent.futures
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from stage_stdlib_process_observer import stage


# Name, exact successful native children, generated continuation executor.
FIXTURES = (
    ('stdlib_process_run_allocations', 0, False),
    ('stdlib_process_pipeline_allocations', 0, False),
    ('stdlib_process_file_allocations', 0, False),
    ('stdlib_process_live_wait', 2, False),
    ('stdlib_process_live_drop', 1, False),
    ('stdlib_process_live_allocations', 1, False),
    ('stdlib_process_terminal_allocations', 1, False),
    ('stdlib_process_future', 2, True),
    ('stdlib_process_future_allocations', 1, True),
    ('stdlib_process_future_drop_allocations', 1, True),
    ('stdlib_process_future_cancel_allocations', 1, True),
)
WRAPPED = ('xl6ProcessSpawnV1', 'xl6PayloadObserveOutcome')
# These two EXISTING file ABI adapters belong to the real compiler's emitted
# program, not to this test or a substituted resident provider. Pin their closed
# registry names and preserve the emitted bytes. Other fixtures have none.
FILE_ADAPTERS = {'xl6_xrt_fs_file_close', 'xl6_xrt_fs_file_temp'}
WITNESS = re.compile(rb'language-process point=(\d+) hit=([01]) children=(\d+) waiter/executor retired balanced=1 '
                     rb'outcome=(memory|success|cancelled-diagnostic) cancelled-diagnostic-failures=(\d+)')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def provider_definitions(data):
    # Inspect consumer OBJECTS, not PE import trampolines in a final image.
    # Accept CRLF explicitly; a trailing CR must not disable the check.
    return re.findall(rb'\b[TDB]\s+((?:xrt|xl6)\w+)\r?$', data, re.M)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'build-report', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=2)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists() and 1 <= args.jobs <= 4
    windows = os.name == 'nt'
    task = Path(__file__).resolve().parents[1]
    cc, nm, inspector = [shutil.which(n) for n in ('gcc', 'nm', 'objdump' if windows else 'readelf')]
    assert cc and nm and inspector
    sys.path.insert(0, str(root / 'tools'))
    from gen_tcc_vfs_resources import load_resources
    build = json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit'] == 0
    products = [root / 'release' / n for n in
                (('xl.exe', 'xl.dll', 'xlc.dll') if windows else ('xl', 'libxl.so', 'libxlc.so'))]
    recorded = {Path(r['Path']).resolve(): r['Sha256'] for r in build['Products']}
    assert all(recorded.get(p) == sha(p) for p in products)
    sources = {n: task / 'test/cases' / (n + '.xl') for n, _, _ in FIXTURES}
    host = task / 'test/cases/stdlib_process_language_product_oom_host.c'
    probe = task / 'test/support/process_language_product_probe.c'
    header = task / 'test/support/process_product_observer.h'
    child_source = task / 'test/cases/xlir_process_child.c'
    fs_registry = root / 'src/ir/xl6_ir_fs_operations.inc'
    registered_fs = set(re.findall(r'XL6_FS_OPERATION\([^,]+,\s*"([^"]+)"', fs_registry.read_text(encoding='utf-8-sig')))
    assert FILE_ADAPTERS <= registered_fs
    build_dir = root / ('build/tmp' if windows else 'build/linux')
    manifest = root / 'tools' / ('tcc_vfs_manifest_win64_runtime.txt' if windows else 'tcc_vfs_manifest_linux_runtime.txt')
    dist = root / 'res/tcc/win64/x86_64' if windows else build_dir / 'tcc-dist'
    resources = [r for r in load_resources(manifest, root, dist, build_dir / 'xrt.o', build_dir)
                 if r.virtual.startswith('include/__xl_runtime/')]
    files = {Path(__file__).resolve(), Path(sys.executable), Path(cc), Path(nm), Path(inspector),
             args.build_report.resolve(), host, probe, header, child_source, manifest, fs_registry,
             task / 'tools/stage_stdlib_process_observer.py', root / 'tools/gen_tcc_vfs_resources.py',
             *sources.values(), *(r.source for r in resources)}
    for row in build['Inputs'] + build['Products']:
        assert sha(row['Path']) == row['Sha256'], row['Path']
        files.add(Path(row['Path']))
    inputs = [dict(Path=str(p), Sha256=sha(p)) for p in sorted(files)]
    out.mkdir()
    isolated = out / 'consumer'
    release = isolated / 'release'
    release.mkdir(parents=True)
    temporary = out / 'tmp'
    temporary.mkdir()
    sdk = out / 'sdk'
    sdk.mkdir()
    for r in resources:
        target = sdk / Path(r.virtual).relative_to('include')
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(r.source, target)
    for p in products:
        shutil.copy2(p, release / p.name)
        if windows:
            shutil.copy2(p, isolated / p.name)
    observer = stage(root, out / 'observer')
    fixture_files = {}
    for name, source in sources.items():
        destination = isolated / source.name
        shutil.copy2(source, destination)
        fixture_files[name] = destination
    assert not any((isolated / p).exists() for p in ('src', 'lib', 'res', 'include'))
    environment = {k: v for k, v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
                   ('LD_LIBRARY_PATH', 'LD_PRELOAD', 'LD_AUDIT', 'CPATH', 'C_INCLUDE_PATH', 'LIBRARY_PATH')}
    environment.update(TMPDIR=str(temporary), TEMP=str(temporary), TMP=str(temporary))
    if not windows:
        import resource
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-I' + str(sdk), '-I' + str(root),
             '-I' + str(root / 'release'), '-I' + str(root / 'release/include'),
             '-I' + str(task), '-I' + str(out / 'observer')]
    if not windows:
        flags += ['-D_GNU_SOURCE']
    compat = ['-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter', '-Wno-unused-label',
              '-Wno-unused-const-variable', '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers', '-Wno-pointer-sign']
    rows, proofs, failures, controls = [], [], [], []
    report = dict(Complete=False, InputsUnchanged=False, Inputs=inputs, Rows=rows, Proofs=proofs, Failures=failures,
                  Controls=controls, Scope=__doc__, Observer=observer, ExplicitSubset=False,
                  CompilerSourceObjects=False, NativeProviderSourceObjects=False, ResidentProviderSourceObjects=False, RuntimeInstrumented=False,
                  ProviderImplementationOverrides=False, TransparentCallSiteObservation=list(WRAPPED),
                  ExistingEmittedFileAdapters=sorted(FILE_ADAPTERS),
                  EmittedLanguageProcessPrefixAcceptance=False, UnobservedJitPrefixAcceptance=False,
                  TrimmedAotPrefixAcceptance=False, ProviderUnloadAcceptance=False, Full53Acceptance=False)

    def run(name, argv, env=None, reject=False):
        argv = list(map(str, argv))
        started = time.monotonic()
        try:
            result = subprocess.run(argv, cwd=isolated, env=env or environment, capture_output=True, timeout=180)
            code, data = result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired as error:
            code, data = 'timeout', (error.stdout or b'') + (error.stderr or b'')
        log = out / (name + '.log')
        log.write_bytes(data)
        rows.append(dict(Name=name, Command=argv, Exit=code, ExpectedRejection=reject, Log=str(log),
                         LogSha256=sha(log), Seconds=time.monotonic() - started))
        assert code not in (0, 'timeout') if reject else code == 0, (name, data.decode(errors='replace')[-6500:])
        return data

    def definitions(name, obj, emitted_adapters=()):
        data = run(name, [nm, '--defined-only', obj])
        assert {n.decode() for n in provider_definitions(data)} == set(emitted_adapters), (name, provider_definitions(data))

    def link(name, obj, probe_obj, product):
        binary = isolated / (name + ('.exe' if windows else ''))
        libs = [release / product.name] if windows else [
            '-L' + str(release), '-l' + product.stem[3:], '-Wl,-rpath,$ORIGIN/release', '-pthread', '-ldl', '-lm']
        run(name + '.link', [cc, obj, probe_obj, *('-Wl,--wrap=' + n for n in WRAPPED), *libs, '-o', binary])
        imports = run(name + '.imports', [inspector, '-p' if windows else '-d', binary])
        names = re.findall(rb'DLL Name:\s*([^\s]+)', imports) if windows else re.findall(rb'Shared library:\s*\[([^\]]+)\]', imports)
        actual = {n.decode().lower() for n in names} & {p.name.lower() for p in products[1:]}
        assert actual == {product.name.lower()}, actual
        return binary

    def check(name, binary, budget, children, cancellation):
        argv = [binary] if budget is None else [binary, budget]
        data = run(name, argv, execution_environment)
        found = WITNESS.findall(data)
        assert len(found) == 1, (name, data[-3000:])
        point, hit, count, outcome, diagnostics = found[0]
        point, hit, count, diagnostics = map(int, (point, hit, count, diagnostics))
        assert point == (2**64 - 1 if budget is None else budget) and count <= children
        assert (outcome == b'cancelled-diagnostic') == (diagnostics == 1)
        if hit:
            assert outcome == b'memory' or cancellation and outcome == b'cancelled-diagnostic'
        else:
            assert count == children and outcome == b'success' and diagnostics == 0
        return bool(hit), outcome == b'cancelled-diagnostic'

    try:
        child = isolated / ('process-child.exe' if windows else 'process-child')
        run('child.build', [cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-O2',
                            *(['-municode'] if windows else ['-D_GNU_SOURCE']), child_source, '-o', child])
        execution_environment = dict(environment, XL6_PROCESS_CHILD=str(child))
        cli = release / products[0].name
        generated = {}
        objects = {}
        for name, _, future in FIXTURES:
            try:
                source = out / (name + '.c')
                run(name + '.cgen', [cli, 'cgen', fixture_files[name], '--emit', source])
                generated[name] = dict(Path=str(source), Sha256=sha(source), SourceSha256=sha(sources[name]))
                extra = ['-DXL6_PROCESS_LANGUAGE_FUTURE'] if future else []
                if name.endswith('future_cancel_allocations'):
                    extra += ['-DXL6_PROCESS_LANGUAGE_CANCEL_DIAGNOSTIC']
                for opt in ('O0', 'O2'):
                    obj = out / (name + '.' + opt + '.o')
                    run(name + '.' + opt + '.compile', [cc, *flags, *compat, '-' + opt, *extra,
                                                       '-include', source, '-c', host, '-o', obj])
                    definitions(name + '.' + opt + '.definitions', obj, FILE_ADAPTERS if name == 'stdlib_process_file_allocations' else ())
                    objects[name, opt] = obj
            except Exception as error:
                failures.append(dict(Name=name + '.build', Error=repr(error)))
        probe_objects = {}
        for opt in ('O0', 'O2'):
            obj = out / ('observer.' + opt + '.o')
            run('observer.' + opt + '.compile', [cc, *flags, '-' + opt, '-DXL6_PROCESS_PRODUCT_OBSERVER', '-c', probe, '-o', obj])
            definitions('observer.' + opt + '.definitions', obj)
            probe_objects[opt] = obj
        jobs = [(p, opt, name, count, future) for p in products[1:] for opt in ('O0', 'O2')
                for name, count, future in FIXTURES if (name, opt) in objects]

        def execute(job):
            product, opt, name, count, future = job
            tag = product.stem + '.' + opt + '.' + name
            cancellation = name.endswith('future_cancel_allocations')
            try:
                binary = link(tag, objects[name, opt], probe_objects[opt], product)
                assert check(tag + '.baseline', binary, None, count, cancellation) == (False, False)
                fallbacks = []
                for point in range(512):
                    hit, fallback = check(tag + '.point.' + str(point), binary, point, count, cancellation)
                    if fallback:
                        fallbacks.append(point)
                    if not hit:
                        break
                else:
                    raise AssertionError('Complete allocation prefix did not reach first no-hit')
                assert point > 0
                assert check(tag + '.confirmed', binary, point + 1, count, cancellation) == (False, False)
                assert len(fallbacks) == int(cancellation), fallbacks
                result = dict(Name=tag, Product=product.name, Optimization=opt, Fixture=name,
                              FailedAllocationPoints=point, FirstNoHit=point, Confirmation=point + 1,
                              CancelledDiagnosticPoints=fallbacks, ObservedChildren=count,
                              Generated=generated[name], ConsumerObjectSha256=sha(objects[name, opt]),
                              Image=dict(Path=str(binary), Sha256=sha(binary)))
                print(tag + ': complete ' + str(point), flush=True)
                return result, None
            except Exception as error:
                print(tag + ': failed ' + repr(error), flush=True)
                return None, dict(Name=tag, Error=repr(error))

        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            for proof, error in pool.map(execute, jobs):
                if error:
                    failures.append(error)
                else:
                    proofs.append(proof)
        host_body = host.read_text(encoding='utf-8-sig')
        probe_body = probe.read_text(encoding='utf-8-sig')
        live = 'stdlib_process_live_allocations'
        for product in products[1:]:
            for label in ('no-failpoint', 'leaked-observer', 'missing-cancel-witness'):
                tag = product.stem + '.control.' + label
                try:
                    name = 'stdlib_process_future_cancel_allocations' if label == 'missing-cancel-witness' else live
                    source = out / (tag + '.c')
                    obj = out / (tag + '.o')
                    if label == 'no-failpoint':
                        needle = 'assert(xrtMemDebugFailAfter(point));'
                        assert host_body.count(needle) == 1
                        source.write_text(host_body.replace(needle, '(void)point;'), encoding='utf-8')
                        run(tag + '.compile', [cc, *flags, *compat, '-O2', '-include', generated[name]['Path'], '-c', source, '-o', obj])
                        binary = link(tag, obj, probe_objects['O2'], product)
                        argv, diagnostic = [binary, 0], b'point!=0 || hit'
                    else:
                        needle, replacement = ('xrtProcessDestroy(process);observed[i]=NULL;', '(void)process;observed[i]=NULL;') if label == 'leaked-observer' else ('state==XFUTURE_CANCELLED && !result', 'state==-999 && !result')
                        assert probe_body.count(needle) == 1
                        source.write_text(probe_body.replace(needle, replacement), encoding='utf-8')
                        run(tag + '.compile', [cc, *flags, '-O2', '-DXL6_PROCESS_PRODUCT_OBSERVER', '-c', source, '-o', obj])
                        binary = link(tag, objects[name, 'O2'], obj, product)
                        if label == 'leaked-observer':
                            argv, diagnostic = [binary], b'LiveCount'
                        else:
                            positive = next(p for p in proofs if p['Product'] == product.name and p['Optimization'] == 'O2' and p['Fixture'] == name)
                            argv, diagnostic = [binary, positive['CancelledDiagnosticPoints'][0]], b'valid'
                    data = run(tag + '.run', argv, execution_environment, reject=True)
                    assert re.search(rb'assert.*fail', data, re.I) and diagnostic in data
                    controls.append(dict(Name=tag, Kind=label, Compiled=True, AssertionRejected=True))
                except Exception as error:
                    failures.append(dict(Name=tag, Error=repr(error)))
        fake = out / 'fake-provider.c'
        fake.write_text('void* xl6ProcessSpawnV1(void* command){(void)command;return 0;}\n', encoding='utf-8')
        fake_obj = out / 'fake-provider.o'
        run('fake-provider.compile', [cc, *flags, '-O2', '-c', fake, '-o', fake_obj])
        data = run('fake-provider.definitions', [nm, '--defined-only', fake_obj])
        assert provider_definitions(data) == [b'xl6ProcessSpawnV1']
        assert provider_definitions(data.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')) == [b'xl6ProcessSpawnV1']
        controls.append(dict(Kind='fake-provider', Compiled=True, DefinitionRejected=True, CrLfChecked=True))
        report['Generated'] = list(generated.values())
        assert all(sha(p['Path']) == p['Sha256'] for p in generated.values()), 'Emitted C changed'
        report['InputsUnchanged'] = all(sha(r['Path']) == r['Sha256'] for r in inputs)
        report['EmittedLanguageProcessPrefixAcceptance'] = len(proofs) == 44 and len(controls) == 7 and not failures
        report['Complete'] = report['InputsUnchanged'] and report['EmittedLanguageProcessPrefixAcceptance']
    except Exception as error:
        failures.append(dict(Name='gate', Error=repr(error)))
    report['PhaseCount'] = len(rows)
    report['Products'] = [dict(Path=str(p), Sha256=sha(p)) for p in sorted(out.rglob('*')) if p.is_file()]
    (out / 'results.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'], Proofs=len(proofs), Failures=failures)), flush=True)
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
