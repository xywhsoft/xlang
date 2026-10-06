#!/usr/bin/env python3
"""Exact resident class display gate, not the all-library acceptance verdict."""
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
    parser.add_argument('--mode', choices=['native', 'shared', 'language'], required=True)
    parser.add_argument('--public', type=Path)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    if out.exists() or (args.sanitize and os.name == 'nt'):
        parser.error('Use a fresh directory; ASan/UBSan is Linux-only')
    if args.public and (os.name != 'nt' or args.mode != 'language'):
        parser.error('Public source compiler is supported for Windows language mode')
    cc = Path(shutil.which('gcc') or '')
    if not cc.is_file():
        parser.error('gcc required')
    runtime = root / ('release/xl.dll' if os.name == 'nt' else 'release/libxl.so')
    compiler_runtime = root / ('release/xlc.dll' if os.name == 'nt' else 'release/libxlc.so')
    cli = root / ('release/xl.exe' if os.name == 'nt' else 'release/xl')
    cases = ['stdlib_class_text', 'stdlib_class_text_derive_only', 'xlir_string_conversion_abi',
             'cgen_derive_string_hash_equals', 'cgen_basic']
    sources = ['test/cases/stdlib_class_text_native_host.c', 'test/support/module_product_memory.c']
    if args.mode == 'native':
        sources += ['test/support/error_text_native.c', 'src/runtime/xl6_error_value.c',
                    'src/runtime/xl6_value_text.c', 'src/runtime/xl6_class_text.c']
    files = {Path(__file__).resolve(), cc, Path(sys.executable), root / 'lib/xrt/xrt.h',
             root / 'test/support/module_product_memory.h'}
    files.update(root / name for name in sources)
    files.update((root / 'src/runtime').glob('*.h'))
    files.update((root / 'release/include').glob('*.h'))
    if args.mode != 'native':
        files.update([runtime, cli])
    if args.mode == 'language':
        files.add(compiler_runtime)
        for folder in ['src', 'lib/xrt', 'lib/xruntime']:
            files.update(f for f in (root / folder).rglob('*') if f.suffix in ['.c', '.h', '.inc'])
        files.update(root / ('test/cases/' + case + '.xl') for case in cases)
        files.add(root / 'test/cases/xlir_module_product_oom_host.c')
        files.add(root / 'test/cases/xlir_trivial_module_product_host.c')
    if args.public or (args.mode == 'language' and os.name != 'nt'):
        files.update((root / 'test/cases').glob('xlir_*owner*.h'))
        files.add(root / 'test/cases/xlir_class_text_contract.c')
    if args.public:
        args.public = args.public.resolve()
        build = json.loads((args.public / 'build.json').read_text(encoding='utf-8-sig'))
        stage = json.loads((args.public / 'artifact_sources.json').read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly']
        for row in build['Inputs'] + build['Headers'] + build['Objects'] + stage['Files']:
            path = Path(row['Path'])
            assert digest(path) == row['Sha256'].lower(), 'Stale source build: ' + str(path)
            files.add(path)
        cli = Path(build['Driver'])
        files.update([cli, args.public / 'build.json', args.public / 'artifact_sources.json'])
    inputs = [dict(Path=str(path.resolve()), Sha256=digest(path)) for path in sorted(files)]
    out.mkdir(parents=True)
    rows, sweeps, contracts = [], [], []
    report = dict(Complete=False, InputsUnchanged=False, Inputs=inputs, Rows=rows,
                  Sweeps=sweeps, Contracts=contracts, Platform=sys.platform, Mode=args.mode,
                  Sanitized=args.sanitize, SelectedClosureInstrumented=args.sanitize and args.mode == 'native',
                  HostAndGeneratedOnlyInstrumented=args.sanitize and args.mode != 'native',
                  Scope='Resident class display; native prefixes or five real language fixtures, not all 53 modules')
    env = dict(os.environ)
    if args.sanitize:
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
                   UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    headers = out / 'include/__xl_runtime'
    headers.mkdir(parents=True)
    for folder in ['src/runtime', 'release/include']:
        for header in (root / folder).glob('*.h'):
            shutil.copy2(header, headers / header.name)
    if os.name == 'nt' and args.mode != 'native':
        shutil.copy2(runtime, out / 'xl.dll')
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
             '-I.', '-Irelease', '-Irelease/include', '-I' + str(out / 'include')]
    if os.name != 'nt':
        flags += ['-D_GNU_SOURCE']
    if args.sanitize:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
    links = ['-lm'] if args.mode == 'native' else ([str(runtime)] if os.name == 'nt' else
            ['-Lrelease', '-lxl', '-Wl,-rpath,' + str(root / 'release'), '-pthread', '-lm', '-ldl'])

    def run(name, command, timeout=600):
        result = subprocess.run([str(x) for x in command], cwd=root, env=env,
                                capture_output=True, timeout=timeout)
        data = result.stdout + result.stderr
        (out / (name + '.log')).write_bytes(data)
        rows.append(dict(Name=name, Command=[str(x) for x in command], Exit=result.returncode))
        assert result.returncode == 0, name + ': ' + data[-4000:].decode(errors='replace')
        return data

    def emit(source, mode, target):
        if args.public:
            return [cli, source, mode, target]
        if mode == 'cgen-default':
            return [cli, 'cgen', source, '--inc-path', out / 'include', '--emit', target]
        return [cli, 'ir', source, '--' + ('typed' if mode == 'ir-typed' else 'lowered'),
                '--format', 'json', '--emit', target]

    try:
        if args.mode != 'language':
            for level in [0, 2]:
                binary = out / ('native.O' + str(level) + ('.exe' if os.name == 'nt' else ''))
                run(f'O{level}.build', [cc, *flags, f'-O{level}', *sources, *links, '-o', binary])
                assert b'100 exact' in run(f'O{level}.vectors', [binary])
                for action in ['class', 'empty', 'null', 'large']:
                    for point in range(129):
                        data = run(f'O{level}.{action}.{point}', [binary, action, point])
                        assert b'balanced' in data
                        if b'hit=0 balanced' in data:
                            sweeps.append(dict(Level=level, Operation=action, FirstNoHit=point))
                            break
                    else:
                        raise AssertionError('Unclosed native prefix')
        else:
            generated_flags = flags + ['-DXL_STATIC_EMBED', '-Wno-unused-variable', '-Wno-unused-parameter',
                '-Wno-unused-label', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
                '-Wno-missing-field-initializers']
            if args.public or os.name != 'nt':
                contract = out / ('contract.exe' if os.name == 'nt' else 'contract')
                objects = []
                contract_links = [str(compiler_runtime)]
                if args.public:
                    contract_links = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
                    objects = [row['Path'] for row in build['Objects']
                               if Path(row['Path']).name != 'xlir_public_source_driver.o']
                run('contract.build', [cc, *generated_flags, '-Wno-pointer-sign', '-Wno-format-truncation',
                    '-Ilib/tcc', 'test/cases/xlir_class_text_contract.c', *objects, *links,
                    *contract_links, '-o', contract])
                for case in ['stdlib_class_text', 'stdlib_class_text_derive_only', 'xlir_string_conversion_abi']:
                    paths = [out / (case + '.contract' + suffix) for suffix in ['.c', '.h', '.json']]
                    run(case + '.contract', [contract, 'test/cases/' + case + '.xl', *paths])
                    repeated = [path.with_name(path.stem + '.repeat' + path.suffix) for path in paths]
                    run(case + '.contract.repeat', [contract, 'test/cases/' + case + '.xl', *repeated])
                    assert all(left.read_bytes() == right.read_bytes() for left, right in zip(paths, repeated))
            for level in [0, 2]:
                run(f'memory.O{level}.build', [cc, *flags, f'-O{level}', '-c',
                    'test/support/module_product_memory.c', '-o', out / f'memory.O{level}.o'])
            for case in cases:
                source = 'test/cases/' + case + '.xl'
                target = out / (case + '.c')
                for mode, suffix in [('cgen-default', '.c'), ('ir-typed', '.typed.json'), ('ir-lowered', '.json')]:
                    path = out / (case + suffix)
                    run(case + '.' + mode, emit(source, mode, path))
                    repeat = path.with_name(path.stem + '.repeat' + path.suffix)
                    run(case + '.' + mode + '.repeat', emit(source, mode, repeat))
                    assert path.read_bytes() == repeat.read_bytes(), 'Nondeterministic ' + case + ' ' + mode
                text = target.read_text(encoding='utf-8')
                ir = json.loads((out / (case + '.json')).read_text())
                operations = [item for item in ir['instructions'] if item.get('runtimeOpName') == 'class-derived-to-string']
                if operations:
                    assert '#include <__xl_runtime/xl6_class_text.h>' in text and 'xl6ClassValueTextV1(' in text
                    for old in ['xlir_string_append_take', 'static char* xlir_xvalue_to_string', 'xlir_class_derive_to_string_t']:
                        assert old not in text, 'Generated lossy formatter remains: ' + old
                    for item in operations:
                        assert item['payloadType'] and f'xlir_class_string_schema_t{item["payloadType"]}' in text
                else:
                    assert 'xl6ClassValueTextV1' not in text, 'Non-class program selected class display'
                contracts.append(dict(Case=case, DerivedOperations=len(operations)))
                host = ('test/cases/xlir_trivial_module_product_host.c' if case == 'cgen_basic' else
                        'test/cases/xlir_module_product_oom_host.c')
                for level in [0, 2]:
                    binary = out / (case + f'.O{level}' + ('.exe' if os.name == 'nt' else ''))
                    run(case + f'.O{level}.build', [cc, *generated_flags, f'-O{level}', '-include', target,
                        host, out / f'memory.O{level}.o', *links, '-o', binary])
                    data = run(case + f'.O{level}.vectors', [binary])
                    assert b'balanced' in data
                    if case == 'stdlib_class_text':
                        assert b'text=a\0\xe4\xbd\xa0, zero=\0, unicode=\xe4\xbd\xa0, large=18446744073709551615}' in data
                        for point in range(4097):
                            data = run(case + f'.O{level}.oom.{point}', [binary, point])
                            assert b'balanced' in data
                            if b'hit=0 balanced' in data:
                                sweeps.append(dict(Case=case, Level=level, FirstNoHit=point))
                                break
                        else:
                            raise AssertionError('Unclosed language prefix')
                binary = out / (case + '.product' + ('.exe' if os.name == 'nt' else ''))
                if args.public:
                    run(case + '.JIT', [cli, source, 'run', out / (case + '.jit')])
                    run(case + '.AOT.build', [cli, source, 'exe', binary])
                else:
                    # Product acceptance must resolve private declarations from
                    # its own VFS, not accidentally succeed via staged headers.
                    run(case + '.JIT', [cli, 'run', source])
                    run(case + '.AOT.build', [cli, 'build', source, '-o', binary])
                run(case + '.AOT.run', [binary])
        report['InputsUnchanged'] = all(digest(Path(row['Path'])) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged'], 'Inputs changed during verification'
        report['Complete'] = True
        print(f'class text {args.mode}: {len(rows)} checks, {len(sweeps)} complete prefixes; inputs unchanged')
        return 0
    except Exception as failure:
        report['Failure'] = str(failure)
        print(report['Failure'], file=sys.stderr)
        return 1
    finally:
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    raise SystemExit(main())
