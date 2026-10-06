#!/usr/bin/env python3
"""Exact Error spans, resident emission, and complete allocation prefixes."""
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
    parser.add_argument('--mode', choices=['native', 'language'], required=True)
    parser.add_argument('--public', type=Path)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    if out.exists() or (args.sanitize and os.name == 'nt'):
        parser.error('Use fresh evidence; sanitizer requires Linux')
    if args.public and (args.mode != 'language' or os.name != 'nt'):
        parser.error('Public source compiler requires Windows language mode')
    cc = Path(shutil.which('gcc') or '')
    if not cc.is_file():
        parser.error('gcc required')
    runtime = root / ('release/xl.dll' if os.name == 'nt' else 'release/libxl.so')
    compiler = root / ('release/xlc.dll' if os.name == 'nt' else 'release/libxlc.so')
    cli = root / ('release/xl.exe' if os.name == 'nt' else 'release/xl')
    cases = ['stdlib_error_span', 'stdlib_error_span_continuation', 'stdlib_memory_error', 'stdlib_error_text',
             'cgen_error_model', 'xlir_error_location_abi', 'xlir_async_error_abi',
             'cgen_error_value_lifetime', 'cgen_error_member_lifetime', 'xlir_error_result_string_call']
    sources = ['test/cases/stdlib_error_span_native_host.c', 'test/support/error_span_native.c',
               'test/support/module_product_memory.c', 'src/runtime/xl6_error_text.c', 'src/api/xl_foreign_error.c']
    files = {Path(__file__).resolve(), cc, Path(sys.executable), root / 'lib/xrt/xrt.h'}
    files.update(root / name for name in sources)
    files.update((root / 'src/runtime').glob('*.h'))
    files.update((root / 'release/include').glob('*.h'))
    if args.mode == 'language':
        files.update([runtime, compiler, cli])
        for folder in ['src', 'lib/xrt', 'lib/xruntime']:
            files.update(f for f in (root / folder).rglob('*') if f.suffix in ['.c', '.h', '.inc'])
        files.update((root / 'test/cases').glob('xlir_*owner*.h'))
        files.add(root / 'test/cases/xlir_error_projection_contract.c')
        files.update(root / ('test/cases/' + name + '.xl') for name in cases)
        files.update(root / name for name in ['test/cases/xlir_module_product_oom_host.c',
            'test/support/module_product_memory.h', 'tools/tcc_vfs_manifest_win64_xlc.txt',
            'tools/tcc_vfs_manifest_linux_xlc.txt'])
    if args.public:
        args.public = args.public.resolve()
        build = json.loads((args.public / 'build.json').read_text(encoding='utf-8-sig'))
        stage = json.loads((args.public / 'artifact_sources.json').read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly']
        for row in build['Inputs'] + build['Headers'] + build['Objects'] + stage['Files']:
            path = Path(row['Path'])
            assert digest(path) == row['Sha256'].lower(), 'Stale source input: ' + str(path)
            files.add(path)
        cli = Path(build['Driver'])
        files.update([cli, args.public / 'build.json', args.public / 'artifact_sources.json'])
    inputs = [dict(Path=str(path.resolve()), Sha256=digest(path)) for path in sorted(files)]
    out.mkdir(parents=True)
    rows, sweeps, contracts = [], [], []
    report = dict(Complete=False, InputsUnchanged=False, Inputs=inputs, Rows=rows,
        Sweeps=sweeps, Contracts=contracts, Mode=args.mode, Platform=sys.platform,
        SelectedClosureInstrumented=args.sanitize and args.mode == 'native',
        HostAndGeneratedOnlyInstrumented=args.sanitize and args.mode == 'language',
        Scope='Immutable exact Error text and language boundaries; not all 53 modules')
    env = dict(os.environ)
    if args.sanitize:
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
                   UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    headers = out / 'include/__xl_runtime'
    headers.mkdir(parents=True)
    for folder in ['src/runtime', 'release/include']:
        for header in (root / folder).glob('*.h'):
            shutil.copy2(header, headers / header.name)
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
             '-I.', '-Irelease', '-Irelease/include', '-I' + str(out / 'include')]
    if os.name != 'nt':
        flags += ['-D_GNU_SOURCE']
    if args.sanitize:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
    links = [] if os.name == 'nt' else ['-pthread', '-lm']

    def run(name, command):
        result = subprocess.run([str(x) for x in command], cwd=root, env=env,
                                capture_output=True, timeout=600)
        data = result.stdout + result.stderr
        (out / (name + '.log')).write_bytes(data)
        rows.append(dict(Name=name, Command=[str(x) for x in command], Exit=result.returncode))
        assert result.returncode == 0, name + ': ' + data[-4000:].decode(errors='replace')
        return data

    def emit(source, mode, target):
        if args.public:
            return [cli, source, mode, target]
        if mode == 'cgen-default':
            return [cli, 'cgen', source, '--emit', target]
        return [cli, 'ir', source, '--' + ('typed' if mode == 'ir-typed' else 'lowered'),
                '--format', 'json', '--emit', target]

    try:
        if args.mode == 'native':
            for level in [0, 2]:
                binary = out / (f'native.O{level}' + ('.exe' if os.name == 'nt' else ''))
                run(f'O{level}.build', [cc, *flags, f'-O{level}', *sources, *links, '-o', binary])
                assert b'100 exact' in run(f'O{level}.vectors', [binary])
                for action in ['create', 'message', 'domain', 'operation', 'data', 'file', 'kind', 'async', 'callback']:
                    for point in range(129):
                        data = run(f'O{level}.{action}.{point}', [binary, action, point])
                        assert b'balanced' in data
                        if b'hit=0 balanced' in data:
                            sweeps.append(dict(Level=level, Operation=action, FirstNoHit=point))
                            break
                    else:
                        raise AssertionError('Unclosed native allocation prefix: ' + action)
        else:
            if os.name == 'nt':
                shutil.copy2(runtime, out / 'xl.dll')
                links = [str(runtime)]
            else:
                links += ['-Lrelease', '-lxl', '-Wl,-rpath,' + str(root / 'release'), '-ldl']
            generated_flags = flags + ['-DXL_STATIC_EMBED', '-Wno-unused-variable', '-Wno-unused-parameter',
                '-Wno-unused-label', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
                '-Wno-missing-field-initializers']
            if args.public or os.name != 'nt':
                contract = out / ('contract.exe' if os.name == 'nt' else 'contract')
                objects, contract_links = [], [str(compiler)]
                if args.public:
                    objects = [row['Path'] for row in build['Objects']
                               if Path(row['Path']).name != 'xlir_public_source_driver.o']
                    contract_links = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
                run('contract.build', [cc, *generated_flags, '-Wno-pointer-sign', '-Wno-format-truncation',
                    '-Ilib/tcc', 'test/cases/xlir_error_projection_contract.c', *objects, *links,
                    *contract_links, '-o', contract])
                for case in ['stdlib_error_span', 'xlir_error_location_abi']:
                    path = out / (case + '.contract.c')
                    data = run(case + '.contract', [contract, 'test/cases/' + case + '.xl', path])
                    assert b'6 allocating families' in data and b'rejected C/header-gated mutations' in data
                    repeat = path.with_name(path.stem + '.repeat.c')
                    run(case + '.contract.repeat', [contract, 'test/cases/' + case + '.xl', repeat])
                    assert path.read_bytes() == repeat.read_bytes(), 'Nondeterministic verified error emission'
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
                assert '#include <__xl_runtime/xl6_error_text.h>' in text
                for old in ['static xerrkind xlir_error_xrt_kind', 'static inline const char* xlir_error_parent_kind',
                            'xlir_string_from_cstr(xrtError', 'xrtStrView(xrtErrorMessage(']:
                    assert old not in text, 'Generated lossy Error implementation remains: ' + old
                contracts.append(dict(Case=case, ExactResident=True))
                for level in [0, 2]:
                    binary = out / (case + f'.O{level}' + ('.exe' if os.name == 'nt' else ''))
                    run(case + f'.O{level}.build', [cc, *generated_flags, f'-O{level}', '-include', target,
                        'test/cases/xlir_module_product_oom_host.c', out / f'memory.O{level}.o', *links, '-o', binary])
                    assert b'balanced' in run(case + f'.O{level}.vectors', [binary])
                    if case in ['stdlib_error_span', 'stdlib_error_span_continuation']:
                        for point in range(4097):
                            data = run(case + f'.O{level}.oom.{point}', [binary, point])
                            assert b'balanced' in data
                            if b'hit=0 balanced' in data:
                                sweeps.append(dict(Case=case, Level=level, FirstNoHit=point))
                                break
                        else:
                            raise AssertionError('Unclosed language allocation prefix: ' + case)
                product = out / (case + '.product' + ('.exe' if os.name == 'nt' else ''))
                if args.public:
                    run(case + '.JIT', [cli, source, 'run', out / (case + '.jit')])
                    run(case + '.AOT.build', [cli, source, 'exe', product])
                else:
                    run(case + '.JIT', [cli, 'run', source])
                    run(case + '.AOT.build', [cli, 'build', source, '-o', product])
                run(case + '.AOT.run', [product])
        report['InputsUnchanged'] = all(digest(Path(row['Path'])) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged'], 'Inputs changed during verification'
        report['Complete'] = True
        print(f'error spans {args.mode}: {len(rows)} checks, {len(sweeps)} complete prefixes; inputs unchanged')
        return 0
    except Exception as failure:
        report['Failure'] = str(failure)
        print(report['Failure'], file=sys.stderr)
        return 1
    finally:
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    raise SystemExit(main())
