#!/usr/bin/env python3
"""Actual Array method IR, emitted C, native caller, JIT/AOT and complete module OOM prefixes."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--public-directory', type=Path)
    parser.add_argument('--sanitize', action='store_true', help='Generated/host code only; shared runtime is not instrumented')
    args = parser.parse_args()
    root, out = Path(__file__).resolve().parents[1], args.out.resolve()
    cc = shutil.which('gcc')
    if not cc or out.exists() or (os.name == 'nt' and (not args.public_directory or args.sanitize)):
        parser.error('Require gcc, fresh evidence and Windows current-source driver')
    fixtures = ['stdlib_array_methods', 'stdlib_array_methods_continuation']
    product = root / ('release/xl.dll' if os.name == 'nt' else 'release/libxl.so')
    public = None
    files = {Path(__file__), Path(cc), Path(sys.executable), product, root / 'lib/xrt/xrt.h',
             root / 'lib/xruntime/xruntime.h'}
    for directory in ('src', 'lib/tcc', 'release/include'):
        files.update(p for p in (root / directory).rglob('*') if p.suffix in ('.c', '.h', '.inc'))
    files.update(root / name for name in ('test/cases/xlir_array_methods_contract.c',
        'test/cases/stdlib_array_methods_projection_host.c', 'test/cases/xlir_module_product_oom_host.c',
        'test/support/module_product_memory.c', 'test/support/module_product_memory.h'))
    files.update(root / f'test/cases/{name}.xl' for name in fixtures)
    if args.public_directory:
        public = json.loads((args.public_directory / 'build.json').read_text(encoding='utf-8-sig'))
        staged = json.loads((args.public_directory / 'artifact_sources.json').read_text(encoding='utf-8-sig'))
        assert not public['RuntimeOnly'] and public['SourceXrt']
        for row in public['Inputs'] + public['Headers'] + public['Objects'] + staged['Files']:
            assert digest(Path(row['Path'])) == row['Sha256'].lower(), row['Path']
            files.add(Path(row['Path']))
        cli = Path(public['Driver'])
        files.update((cli, args.public_directory / 'build.json', args.public_directory / 'artifact_sources.json'))
    else:
        cli = root / 'release/xl'
        files.update((cli, root / 'release/libxlc.so'))
    inputs = [{'Path': str(path.resolve()), 'Sha256': digest(path)} for path in sorted(files)]
    rows, sweeps = [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Inputs': inputs, 'Rows': rows, 'Sweeps': sweeps,
              'Scope': __doc__, 'LanguageRuntimeInstrumented': False, 'SanitizedHost': args.sanitize}
    out.mkdir(parents=True)
    private = out / 'include/__xl_runtime'; private.mkdir(parents=True)
    for directory in (root / 'src/runtime', root / 'release/include'):
        for header in directory.glob('*.h'):
            shutil.copy2(header, private / header.name)
    env = dict(os.environ)
    platform = [] if os.name == 'nt' else ['-pthread', '-lm', '-ldl']
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-I.', '-Irelease',
             '-Irelease/include', '-Ilib/tcc', '-I' + str(out / 'include')]
    if os.name != 'nt': flags += ['-D_GNU_SOURCE']
    links = [str(product)] if os.name == 'nt' else ['-Lrelease', '-lxl', '-Wl,-rpath,' + str(root / 'release'), *platform]
    strict = [*flags, '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable',
              '-Wno-unused-label', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
              '-Wno-missing-field-initializers']
    if args.sanitize:
        strict += ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
                   UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    if os.name == 'nt': shutil.copy2(product, out / 'xl.dll')

    def run(name, command, timeout=180):
        result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=timeout)
        data = result.stdout + result.stderr
        (out / (name + '.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode, 'Complete': result.returncode == 0})
        if result.returncode: raise RuntimeError(name + ': ' + data[-4000:].decode(errors='replace'))
        return data.decode(errors='replace')

    try:
        compiler_links = ([row['Path'] for row in public['Objects'] if Path(row['Path']).name != 'xlir_public_source_driver.o'] +
            [str(product), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if public else ['-lxlc', *links])
        contract = out / ('contract.exe' if os.name == 'nt' else 'contract')
        run('contract.build', [cc, *flags, '-O0', 'test/cases/xlir_array_methods_contract.c',
            *compiler_links, '-o', str(contract)], 360)
        for name in fixtures:
            source = f'test/cases/{name}.xl'
            target, ir, bindings = out / (name + '.c'), out / (name + '.json'), out / 'bindings.h'
            for mode, path in (('cgen-default', target), ('ir-lowered', ir)):
                def emit(p):
                    if public: return [str(cli), source, mode, str(p)]
                    if mode == 'ir-lowered': return [str(cli), 'ir', source, '--lowered', '--format', 'json', '--emit', str(p)]
                    return [str(cli), 'cgen', source, '--inc-path', str(out / 'include'), '--emit', str(p)]
                run(name + '.' + mode, emit(path))
                repeat = path.with_name(path.stem + '.repeat' + path.suffix)
                run(name + '.' + mode + '.repeat', emit(repeat))
                assert path.read_bytes() == repeat.read_bytes()
            original = target.read_bytes()
            proof = run(name + '.contract', [str(contract), source, str(target),
                *([str(bindings)] if name == fixtures[0] else [])])
            assert 'mutations refused before C/header output' in proof and original == target.read_bytes()
            code = original.decode('utf-8'); assert 'xl6_array_methods.h' in code
            assert not any(token in code for token in ('static bool xlir_xvalue_array_remove_value(',
                'static int64_t xlir_xvalue_typed_array_index_of(', 'static bool xlir_xvalue_typed_array_reverse('))
            if name == fixtures[0]:
                assert all('xl6ArrayMethod' + suffix + 'V1(' in code for suffix in
                    ('Clear', 'Capacity', 'Reserve', 'Trim', 'Swap', 'Reverse', 'Insert', 'IndexOf', 'RemoveValue'))
            for level in (0, 2):
                binary = out / (name + f'.O{level}' + ('.exe' if os.name == 'nt' else ''))
                memory = out / f'memory.O{level}.o'
                if not memory.exists():
                    run(f'memory.O{level}.build', [cc, *strict, f'-O{level}', '-c',
                        'test/support/module_product_memory.c', '-o', str(memory)])
                run(name + f'.O{level}.build', [cc, *strict, f'-O{level}', '-include', str(target),
                    'test/cases/xlir_module_product_oom_host.c', str(memory), *links, '-o', str(binary)], 360)
                assert 'balanced' in run(name + f'.O{level}.run', [str(binary)])
                if name == fixtures[0]:
                    for point in range(4097):
                        proof = run(name + f'.O{level}.oom.{point}', [str(binary), str(point)])
                        assert 'balanced' in proof
                        if 'hit=0 balanced' in proof:
                            sweeps.append({'Fixture': name, 'Level': level, 'FirstNoHit': point}); break
                    else: raise RuntimeError('Unclosed complete module allocation prefix')
                    projected = out / (f'projection.O{level}' + ('.exe' if os.name == 'nt' else ''))
                    run(f'projection.O{level}.build', [cc, *strict, f'-O{level}', '-include', str(target),
                        '-include', str(bindings), 'test/cases/stdlib_array_methods_projection_host.c',
                        *links, '-o', str(projected)], 360)
                    assert '100 physical cycles balanced' in run(f'projection.O{level}.run', [str(projected)])
                    for domain in ('int', 'text'):
                        for point in range(513):
                            proof = run(f'projection.O{level}.{domain}.oom.{point}', [str(projected), domain, str(point)])
                            assert 'balanced' in proof
                            if 'hit=0 balanced' in proof:
                                sweeps.append({'Fixture': 'generated/native', 'Domain': domain,
                                               'Level': level, 'FirstNoHit': point}); break
                        else: raise RuntimeError('Unclosed generated/native allocation prefix')
            prefix = out / (name + '.product')
            if public:
                run(name + '.jit', [str(cli), source, 'run', str(prefix)])
                run(name + '.aot', [str(cli), source, 'exe', str(prefix)])
            else:
                run(name + '.jit', [str(cli), 'run', source, '--inc-path', str(out / 'include')])
                run(name + '.aot.build', [str(cli), 'build', source, '--inc-path', str(out / 'include'), '-o', str(prefix)])
                run(name + '.aot', [str(prefix)])
        report['InputsUnchanged'] = all(digest(Path(row['Path'])) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete'] = True
    except Exception as error:
        report['Error'] = repr(error); print(error, file=sys.stderr)
    finally:
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'Array methods language: {len(rows)} checks, {len(sweeps)} complete prefixes, complete={report["Complete"]}')
    return 0 if report['Complete'] else 1


if __name__ == '__main__': raise SystemExit(main())
