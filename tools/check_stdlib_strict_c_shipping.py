"""Compile the complete product fixture population as strict C at O0 and O2.

Generate C with the actual shipping CLI in an isolated directory. The only
private headers are an inspection SDK extracted from that build's VFS inputs.
Use the complete AOT receipt's exact assertion sources, including hosted and
platform-conditioned fixtures: compilation does not waive their execution.
Collect independent failures. This is not execution, runtime branch coverage,
provider substitution, source-driver fallback or full53 functional acceptance.
"""
from pathlib import Path
import argparse
import concurrent.futures
import hashlib
import importlib
import json
import os
import shutil
import subprocess
import sys
import threading
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'build-report', 'aot-report', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=2)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists() and 1 <= args.jobs <= 4
    windows = os.name == 'nt'
    cc = shutil.which('gcc')
    assert cc
    build = json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    aot = json.loads(args.aot_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit'] == 0
    assert aot['Complete'] and aot['InputsUnchanged'] and aot['AllExecutablePassed']
    assert aot['RelocatedAotInventoryAcceptance'] and not aot['ExplicitSubset']
    assert not aot.get('Failures')
    products = [root / 'release' / name for name in
                (('xl.exe', 'xl.dll', 'xlc.dll') if windows else
                 ('xl', 'libxl.so', 'libxlc.so'))]
    recorded = {Path(r['Path']).resolve(): r['Sha256'].lower() for r in build['Products']}
    assert all(recorded.get(p) == sha(p) for p in products)
    aot_inputs = {Path(r['Path']).resolve(): r['Sha256'].lower() for r in aot['Inputs']}
    assert aot_inputs.get(args.build_report.resolve()) == sha(args.build_report)
    assert all(aot_inputs.get(p) == sha(p) for p in products)
    sys.path.insert(0, str(root / 'tools'))
    inventory = importlib.import_module('discover_stdlib_three_groups')
    from gen_tcc_vfs_resources import load_resources
    population = {p.stem for p in (root / 'test/cases').glob('stdlib_*.xl')
                  if inventory.PREFIX.match(p.name)}
    additions = [Path(p).resolve() for p in aot['AdditionalFixtureSources']]
    assert len(population) == aot['FrozenCandidateCount']
    for source in additions:
        assert source.stem not in population and inventory.PREFIX.match(source.name)
        population.add(source.stem)
    cases = aot['Cases']
    assert len(cases) == aot['CandidateCount'] == aot['SelectedCount'] == len(population)
    assert {c['Name'] for c in cases} == population
    sources = {}
    for case in cases:
        source = Path(case['Source']).resolve()
        assert source.stem == case['Name'] and sha(source) == case['SourceSha256']
        assert aot_inputs.get(source) == sha(source)
        sources[case['Name']] = source

    files = {Path(__file__).resolve(), Path(sys.executable), Path(cc),
             args.build_report.resolve(), args.aot_report.resolve(),
             Path(inventory.__file__).resolve(), *sources.values()}
    for row in build['Inputs'] + build['Products']:
        assert sha(row['Path']) == row['Sha256'].lower(), row['Path']
        files.add(Path(row['Path']))
    out.mkdir()
    isolated = out / 'isolated'
    release, staged, sdk, temporary = [isolated / n for n in
                                      ('release', 'inputs', 'sdk', 'tmp')]
    for directory in (release, staged, sdk, temporary):
        directory.mkdir(parents=True)
    for product in products:
        shutil.copy2(product, release / product.name)
    for source in sources.values():
        shutil.copy2(source, staged / source.name)
    for header in (root / 'release/include').glob('*.h'):
        files.add(header)
        shutil.copy2(header, sdk / header.name)
    build_dir = root / ('build/tmp' if windows else 'build/linux')
    manifest = root / 'tools' / ('tcc_vfs_manifest_win64_runtime.txt' if windows else
                                'tcc_vfs_manifest_linux_runtime.txt')
    dist = root / 'res/tcc/win64/x86_64' if windows else build_dir / 'tcc-dist'
    files.add(manifest)
    files.add(root / 'tools/gen_tcc_vfs_resources.py')
    for resource in load_resources(manifest, root, dist, build_dir / 'xrt.o', build_dir):
        if resource.virtual.startswith('include/__xl_runtime/'):
            files.add(Path(resource.source))
            target = sdk / Path(resource.virtual).relative_to('include')
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(resource.source, target)
    files.add(root / 'lib/xruntime/xruntime.h')
    shutil.copy2(root / 'lib/xruntime/xruntime.h', sdk / 'xruntime.h')
    assert not any((isolated / name).exists() for name in ('src', 'lib', 'res', 'include'))
    inputs = [dict(Path=str(p), Sha256=sha(p)) for p in sorted(files)]
    environment = {k: v for k, v in os.environ.items()
                   if not k.upper().startswith('XL6_') and k not in
                   ('LD_LIBRARY_PATH', 'LD_PRELOAD', 'LD_AUDIT', 'CPATH',
                    'C_INCLUDE_PATH', 'LIBRARY_PATH')}
    environment.update(TMPDIR=str(temporary), TEMP=str(temporary), TMP=str(temporary))
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-I' + str(sdk),
             '-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter',
             '-Wno-unused-label', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
             '-Wno-missing-field-initializers', '-Wno-pointer-sign']
    if not windows:
        flags += ['-D_GNU_SOURCE']
    rows, records, failures, controls = [], [], [], []
    lock = threading.Lock()
    report = dict(Complete=False, InputsUnchanged=False, Inputs=inputs, Rows=rows,
                  Cases=records, Failures=failures, Controls=controls, Scope=__doc__,
                  CandidateCount=len(population), SelectedCount=len(sources), ExplicitSubset=False,
                  StrictCInventoryAcceptance=False, NativeExecutionAcceptance=False,
                  Full53Acceptance=False, CompilerSourceObjects=False, ProviderOverrides=False,
                  AotEvidence=dict(Path=str(args.aot_report.resolve()), Sha256=sha(args.aot_report)),
                  AotExecutionConditions=[dict(Name=c['Name'], Status=c['Status']) for c in cases
                                          if c['Status'] in ('requires-host', 'requires-platform')])

    def command(name, argv):
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
        row = dict(Name=name, Command=argv, Exit=code, Seconds=time.monotonic() - started,
                   Log=str(log), LogSha256=sha(log))
        with lock:
            rows.append(row)
        return row, data

    def compile_case(item):
        name, source = item
        record = dict(Name=name, Source=str(source), SourceSha256=sha(source),
                      Passed=False, Rows=[], Objects=[])
        directory = out / name
        directory.mkdir()
        generated = directory / 'program.c'
        row, data = command(name + '.cgen', [release / products[0].name, 'cgen',
                                             staged / source.name, '--emit', generated])
        record['Rows'].append(row)
        if name in inventory.NEGATIVE:
            record['ExpectedRejection'] = True
            record['Passed'] = (row['Exit'] not in (0, 'timeout') and
                                inventory.NEGATIVE[name].encode() in data and not generated.exists())
        elif row['Exit'] == 0 and generated.is_file() and generated.stat().st_size:
            record['Generated'] = dict(Path=str(generated), Sha256=sha(generated))
            for opt in ('O0', 'O2'):
                obj = directory / (opt + '.o')
                row, data = command(name + '.' + opt,
                                    [cc, *flags, '-' + opt, '-c', generated, '-o', obj])
                record['Rows'].append(row)
                if row['Exit'] == 0 and obj.is_file() and obj.stat().st_size:
                    record['Objects'].append(dict(Optimization=opt, Path=str(obj), Sha256=sha(obj)))
            record['Passed'] = len(record['Objects']) == 2
        if not record['Passed']:
            record['Diagnostic'] = data.decode(errors='replace')[-6000:]
        print(name + ': ' + ('passed' if record['Passed'] else 'FAILED'), flush=True)
        return record

    def save():
        report['PhaseCount'] = len(rows)
        pending = out / 'results.json.pending'
        pending.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        pending.replace(out / 'results.json')

    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            for record in pool.map(compile_case, sorted(sources.items())):
                records.append(record)
                if not record['Passed']:
                    failures.append(dict(Name=record['Name'], Diagnostic=record['Diagnostic']))
                save()
        # The same strict compiler must reject real malformed generated C.
        valid = next(Path(c['Generated']['Path']) for c in records if c['Passed'] and c.get('Generated'))
        for name, suffix in (
                ('syntax', '\n#error XL6_STRICT_C_SYNTAX_CONTROL\n'),
                ('pointer-return', '\nint xl6_strict_pointer_control(void) { return (void*)1; }\n'),
                ('undeclared-call', '\nint xl6_strict_call_control(void) { return xl6_strict_missing_call(); }\n')):
            source = out / ('control-' + name + '.c')
            source.write_bytes(valid.read_bytes() + suffix.encode())
            for opt in ('O0', 'O2'):
                obj = out / ('control-' + name + '.' + opt + '.o')
                row, data = command('control.' + name + '.' + opt,
                                    [cc, *flags, '-' + opt, '-c', source, '-o', obj])
                passed = row['Exit'] not in (0, 'timeout') and not obj.exists()
                controls.append(dict(Name=name, Optimization=opt, Rejected=passed))
                if not passed:
                    failures.append(dict(Name=row['Name'], Diagnostic=data.decode(errors='replace')[-6000:]))
        report['InputsUnchanged'] = all(sha(r['Path']) == r['Sha256'] for r in inputs)
        report['StrictCInventoryAcceptance'] = (len(records) == len(sources) and
                                                 all(c['Passed'] for c in records) and
                                                 len(controls) == 6 and all(c['Rejected'] for c in controls))
        report['Complete'] = report['InputsUnchanged'] and report['StrictCInventoryAcceptance'] and not failures
    except Exception as error:
        failures.append(dict(Name='gate', Error=repr(error)))
    report['Products'] = [dict(Path=str(p), Sha256=sha(p)) for p in sorted(out.rglob('*'))
                          if p.is_file() and p.name not in ('results.json', 'results.json.pending')]
    save()
    print(json.dumps(dict(Complete=report['Complete'], Cases=len(records), Failures=failures)), flush=True)
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
