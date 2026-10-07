#!/usr/bin/env python3
"""Closed class identity IR and inheritance acceptance, not checked native identity completion."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--public', type=Path)
    args = parser.parse_args()
    root, out = Path(__file__).resolve().parents[1], args.out.resolve()
    cc = shutil.which('gcc')
    if not cc or out.exists() or (os.name == 'nt' and not args.public) or (os.name != 'nt' and args.public):
        parser.error('Require gcc, fresh evidence root; Windows uses --public, Linux uses source products')
    cases = ['stdlib_class_identity_inherited', 'cgen_derive_string_hash_equals', 'stdlib_native_sequence_identity']
    runtime = root / ('release/xl.dll' if os.name == 'nt' else 'release/libxl.so')
    compiler = root / ('release/xlc.dll' if os.name == 'nt' else 'release/libxlc.so')
    cli = root / ('release/xl.exe' if os.name == 'nt' else 'release/xl')
    objects = []
    files = {Path(__file__), Path(cc), Path(sys.executable), runtime}
    for folder in ['src', 'lib/xrt', 'lib/xruntime', 'release/include']:
        files.update(p for p in (root / folder).rglob('*') if p.suffix in ['.h', '.c', '.inc'])
    files.update(root / ('test/cases/' + case + '.xl') for case in cases)
    files.update(root / p for p in ['test/cases/xlir_class_identity_plan_contract.c',
        'test/cases/xlir_module_product_oom_host.c', 'test/support/module_product_memory.c',
        'test/support/module_product_memory.h'])
    if args.public:
        public = args.public.resolve()
        build = json.loads((public / 'build.json').read_text(encoding='utf-8-sig'))
        stage = json.loads((public / 'artifact_sources.json').read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly']
        for row in build['Inputs'] + build['Objects'] + build['Headers'] + stage['Files']:
            path = Path(row['Path']); assert digest(path) == row['Sha256'].lower(), str(path)
            files.add(path)
        objects = [row['Path'] for row in build['Objects'] if Path(row['Path']).name != 'xlir_public_source_driver.o']
        cli = Path(build['Driver']); files.update([cli, public / 'build.json', public / 'artifact_sources.json'])
    else:
        files.update([cli, compiler])
    inputs = [dict(Path=str(p.resolve()), Sha256=digest(p)) for p in sorted(files)]
    out.mkdir(parents=True)
    rows = []
    report = dict(Complete=False, InputsUnchanged=False, Inputs=inputs, Rows=rows,
        Scope=__doc__, CheckedNativeIdentity=False, Platform=sys.platform)
    env = dict(os.environ)
    include = out / 'include/__xl_runtime'; include.mkdir(parents=True)
    for folder in ['src/runtime', 'release/include']:
        for header in (root / folder).glob('*.h'): shutil.copy2(header, include / header.name)
    if os.name == 'nt':
        shutil.copy2(runtime, out / 'xl.dll')
        links = [str(runtime)]
        contract_links = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
    else:
        links = ['-Lrelease', '-lxl', '-Wl,-rpath,' + str(root / 'release'), '-pthread', '-lm', '-ldl']
        contract_links = [str(compiler)]
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
        '-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter', '-Wno-unused-label',
        '-Wno-unused-const-variable', '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers',
        '-I.', '-Irelease', '-Irelease/include', '-I' + str(out / 'include')]
    if os.name != 'nt': flags += ['-D_GNU_SOURCE']

    def run(name, command, timeout=600):
        result = subprocess.run([str(x) for x in command], cwd=root, env=env,
            capture_output=True, timeout=timeout)
        data = result.stdout + result.stderr; (out / (name + '.log')).write_bytes(data)
        rows.append(dict(Name=name, Command=[str(x) for x in command], Exit=result.returncode))
        assert result.returncode == 0, name + ': ' + data[-6000:].decode(errors='replace')
        return data

    def emit(source, mode, target):
        if args.public: return [cli, source, mode, target]
        if mode == 'cgen-default': return [cli, 'cgen', source, '--inc-path', out / 'include', '--emit', target]
        return [cli, 'ir', source, '--' + ('typed' if mode == 'ir-typed' else 'lowered'),
            '--format', 'json', '--emit', target]

    try:
        contract = out / ('contract.exe' if os.name == 'nt' else 'contract')
        run('contract.build', [cc, *flags, '-Wno-pointer-sign', '-Wno-format-truncation', '-Ilib/tcc',
            'test/cases/xlir_class_identity_plan_contract.c', *objects, *links, *contract_links, '-o', contract])
        # Preinclude belongs only to the generated host TU, never the separate
        # allocator probe (which would define the module a second time).
        for level in [0, 2]:
            run(f'memory.O{level}.build', [cc, *flags, f'-O{level}', '-c',
                'test/support/module_product_memory.c', '-o', out / f'memory.O{level}.o'])
        for case in cases:
            source = 'test/cases/' + case + '.xl'
            paths = [out / (case + '.contract' + suffix) for suffix in ['.c', '.h', '.json']]
            assert b'rejected C/header mutations' in run(case + '.contract', [contract, source, *paths])
            repeated = [p.with_name(p.stem + '.repeat' + p.suffix) for p in paths]
            run(case + '.contract.repeat', [contract, source, *repeated])
            assert all(p.read_bytes() == q.read_bytes() for p, q in zip(paths, repeated))
            for mode, suffix in [('ir-typed', '.typed.json'), ('ir-lowered', '.json'), ('cgen-default', '.c')]:
                target = out / (case + suffix)
                run(case + '.' + mode, emit(source, mode, target))
                repeat = target.with_name(target.stem + '.repeat' + target.suffix)
                run(case + '.' + mode + '.repeat', emit(source, mode, repeat))
                assert target.read_bytes() == repeat.read_bytes()
            ir = json.loads((out / (case + '.json')).read_text())
            assert len(ir['classIdentityPlans']) == len(ir['classLayouts'])
            if case == 'stdlib_class_identity_inherited':
                by_name = {t['name']: t['id'] for t in ir['types']}
                expected = {'IdentityChild': ['baseNumber', 'baseText', 'childNumber'],
                    'UnconstructedIdentityChild': ['inherited', 'own']}
                for name, names in expected.items():
                    layout_index = next(i for i, v in enumerate(ir['classLayouts']) if v['type'] == by_name[name])
                    plan = ir['classIdentityPlans'][layout_index]; assert plan['residentKeyedPair'] == 1
                    fields = ir['classIdentityFields'][plan['firstField']:plan['firstField'] + plan['fieldCount']]
                    # Exact names/order are asserted against the real arena by
                    # the independent C contract (dump intentionally omits names).
                    assert len(fields) == len(names)
            target = out / (case + '.c')
            for level in [0, 2]:
                binary = out / (case + f'.O{level}' + ('.exe' if os.name == 'nt' else ''))
                run(case + f'.O{level}.build', [cc, *flags, f'-O{level}', '-include', target,
                    'test/cases/xlir_module_product_oom_host.c', out / f'memory.O{level}.o',
                    *objects, *links, *(contract_links if args.public else []), '-o', binary])
                assert b'balanced' in run(case + f'.O{level}.vectors', [binary])
            binary = out / (case + '.product' + ('.exe' if os.name == 'nt' else ''))
            if args.public:
                run(case + '.JIT', [cli, source, 'run', out / (case + '.jit')])
                run(case + '.AOT.build', [cli, source, 'exe', binary])
            else:
                run(case + '.JIT', [cli, 'run', source])
                run(case + '.AOT.build', [cli, 'build', source, '-o', binary])
            run(case + '.AOT.run', [binary])
        report['InputsUnchanged'] = all(digest(Path(row['Path'])) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete'] = True
    except Exception as failure:
        report['Failure'] = repr(failure); print(report['Failure'], file=sys.stderr)
    finally:
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'Identity plan: {len(rows)} checks, complete={report["Complete"]}')
    return 0 if report['Complete'] else 1


if __name__ == '__main__': raise SystemExit(main())
