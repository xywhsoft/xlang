"""Compare unchanged dynamic-method semantics on normal old/new products.

The private source unit is an independent decision oracle. Real products must
also retain byte-identical typed/lowered IR and C for existing language cases.
This gate does not certify the complete five-group release or global guards.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
from gen_tcc_vfs_resources import load_resources


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'build-report', 'previous-root', 'previous-build', 'unit-report', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    root, previous, out = args.root.resolve(), args.previous_root.resolve(), args.out.resolve()
    assert not out.exists(), 'Preserve earlier evidence; use a fresh directory'
    controls = Path(__file__).resolve().parents[1]
    windows = os.name == 'nt'
    cc = Path(shutil.which('gcc')).resolve()
    names = ('xl.exe', 'xl.dll', 'xlc.dll') if windows else ('xl', 'libxl.so', 'libxlc.so')
    unit = json.loads(args.unit_report.read_text(encoding='utf-8-sig'))
    assert unit['Complete'] and unit['InputsUnchanged'] and len(unit['Rows']) == 6
    implementation = controls / 'src/ir/xl6_ir.c'
    decision_test = controls / 'test/cases/xlir_dynamic_method_eligibility.c'
    assert unit['Inputs'][str(implementation)] == sha(implementation)
    assert unit['Inputs'][str(decision_test)] == sha(decision_test)
    assert all(row['Exit'] == 0 and sha(row['Log']) == row['Sha256'] for row in unit['Rows'])
    new_text = (root / 'src/ir/xl6_ir.c').read_text(encoding='utf8')
    assert new_text == implementation.read_text(encoding='utf8')
    begin = new_text.index('    /* Completeness visits every class/declaration pair.')
    end = new_text.index('    /* Typed registry rows select inherited members', begin)
    assert new_text[:begin] + new_text[end:] == (previous / 'src/ir/xl6_ir.c').read_text(encoding='utf8')

    out.mkdir(parents=True)
    work, inputs = out / 'empty-work', out / 'inputs'
    work.mkdir()
    inputs.mkdir()
    host_source = controls / 'test/cases/xlir_five_groups_joint_host.c'
    cases = [controls / 'test/cases' / (name + '.xl') for name in
             ('cgen_dynamic_method_inheritance', 'cgen_dynamic_method_overload',
              'cgen_generic_dynamic_method', 'cgen_dynamic_method_arguments',
              'cgen_dynamic_method_value_domains')]
    benchmark = controls / 'test/cases/stdlib_five_groups_joint.xl'
    pinned = {Path(__file__).resolve(), Path(sys.executable), cc, implementation, decision_test,
              args.unit_report.resolve(), host_source, benchmark, *cases,
              Path(load_resources.__code__.co_filename).resolve()}
    cohorts = {}
    for label, source, receipt in (('new', root, args.build_report), ('previous', previous, args.previous_build)):
        build = json.loads(receipt.read_text(encoding='utf-8-sig'))
        assert build['Complete'] and build['InputsUnchanged'] and build['Exit'] == 0
        recorded = {Path(row['Path']).resolve(): row['Sha256'] for row in build['Products']}
        isolated = out / (label + '-products')
        isolated.mkdir()
        for name in names:
            product = source / 'release' / name
            assert recorded[product] == sha(product)
            shutil.copy2(product, isolated / name)
            pinned.add(product)
        pinned.update((receipt.resolve(), source / 'src/ir/xl6_ir.c'))
        assert (root / 'release/include/xlc.h').read_bytes() == (source / 'release/include/xlc.h').read_bytes()
        assert (root / 'release/include/xl.h').read_bytes() == (source / 'release/include/xl.h').read_bytes()
        cohorts[label] = dict(Root=source, Products=isolated,
                              Host=isolated / ('audit-host.exe' if windows else 'audit-host'))
    manifest = root / 'tools' / ('tcc_vfs_manifest_win64_runtime.txt' if windows else 'tcc_vfs_manifest_linux_runtime.txt')
    directory = root / ('build/tmp' if windows else 'build/linux')
    dist = root / 'res/tcc/win64/x86_64' if windows else directory / 'tcc-dist'
    resources = load_resources(manifest, root, dist, directory / 'xrt.o', directory)
    sdk = out / 'strict-sdk'
    pinned.add(manifest)
    for row in resources:
        if not row.virtual.startswith('include/__xl_runtime/'):
            continue
        target = sdk / Path(row.virtual).relative_to('include')
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(row.source, target)
        pinned.add(row.source)
    report = dict(Complete=False, Inputs={str(p): sha(p) for p in sorted(pinned)},
                  Rows=[], Failures=[], IdenticalOutputs=[], ActualShippingProducts=True,
                  SourceObjectOverrides=False, FullFiveGroupsAccepted=False, Scope=__doc__)
    environment = {k: v for k, v in os.environ.items() if not k.upper().startswith('XL6_') and k.upper() not in
                   ('CPATH', 'C_INCLUDE_PATH', 'LIBRARY_PATH', 'LD_LIBRARY_PATH', 'LD_PRELOAD', 'LD_AUDIT')}
    environment.update(TMPDIR=str(out), TEMP=str(out), TMP=str(out))

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')

    def run(name, command):
        command = list(map(str, command))
        log = out / (name + '.log')
        started = time.monotonic()
        with log.open('wb') as stream:
            code = subprocess.run(command, cwd=work, env=environment, stdout=stream,
                                  stderr=subprocess.STDOUT, timeout=300).returncode
        row = dict(Name=name, Command=command, Exit=code, Expected=0, Log=str(log),
                   Sha256=sha(log), Seconds=time.monotonic() - started)
        report['Rows'].append(row)
        save()
        print(name, code, flush=True)
        assert code == 0, str(log)
        return log.read_text(encoding='utf8', errors='replace'), row['Seconds']

    try:
        for label, cohort in cohorts.items():
            run(label + '-host-build', [cc, '-std=c11', '-Wall', '-Wextra', '-Werror',
                '-I' + str(root / 'release/include'), host_source, cohort['Products'] / names[2],
                *([] if windows else ['-Wl,-rpath,$ORIGIN']), '-o', cohort['Host']])
        flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-unused-variable',
                 '-Wno-unused-parameter', '-Wno-unused-label', '-Wno-unused-but-set-variable',
                 '-Wno-unused-const-variable', '-Wno-missing-field-initializers', '-I' + str(sdk)]
        for case in cases:
            copied = inputs / case.name
            shutil.copy2(case, copied)
            outputs = {}
            for label, cohort in cohorts.items():
                target = out / (case.stem + '-' + label)
                target.mkdir()
                output, _ = run(case.stem + '-' + label, [cohort['Host'], copied, target])
                assert 'joint-host: checked-retirement\n' in output
                outputs[label] = target
                run(case.stem + '-' + label + '-aot', [target / 'joint.exe'])
            for name in ('joint.typed.json', 'joint.lowered.json', 'joint.c'):
                assert (outputs['new'] / name).read_bytes() == (outputs['previous'] / name).read_bytes(), (case.stem, name)
            report['IdenticalOutputs'].append(case.stem)
            for optimization in ('O0', 'O2'):
                run(case.stem + '-strict-' + optimization,
                    [cc, *flags, '-' + optimization, '-fsyntax-only', outputs['new'] / 'joint.c'])
        copied = inputs / benchmark.name
        shutil.copy2(benchmark, copied)
        timings = {}
        for label in ('previous', 'new'):
            output, elapsed = run(label + '-joint-check', [cohorts[label]['Products'] / names[0], 'check', copied])
            assert output.strip() == 'ok'
            timings[label] = elapsed
        report['BenchmarkSeconds'] = timings
        report['ObservedSpeedup'] = timings['previous'] / timings['new']
    except Exception as error:
        report['Failures'].append(repr(error))
    finally:
        report['InputsUnchanged'] = all(sha(p) == h for p, h in report['Inputs'].items())
        report['Complete'] = report['InputsUnchanged'] and not report['Failures']
        report['Products'] = [dict(Path=str(p), Sha256=sha(p)) for p in out.rglob('*')
                              if p.is_file() and p.name != 'results.json']
        save()
    assert report['Complete'], report['Failures']


if __name__ == '__main__':
    main()
