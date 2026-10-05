#!/usr/bin/env python3
"""Array shuffle consumers: actual native slots/wire COW, two independent RNG providers.

Standalone O0/O2 runs selected XRT and resident code (all selected units are
instrumented with --sanitize). --shared instruments the host only. --language
adds strict generated C, deterministic IR/C, JIT/AOT and normal module lifetime;
the language fixtures still use value-v1, not evidence of its native cutover.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--shared', action='store_true')
    parser.add_argument('--language', action='store_true')
    parser.add_argument('--public-directory', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    cc, nm = shutil.which('gcc'), shutil.which('nm')
    if out.exists() or not cc or not nm or (args.sanitize and os.name == 'nt') or (
            args.language and os.name == 'nt' and not args.public_directory):
        parser.error('Require gcc/nm, fresh evidence, Linux sanitizer and Windows current-source compiler')
    host = 'test/cases/stdlib_array_shuffle_native_host.c'
    providers = ['src/runtime/xl6_array_rng_shuffle.c', 'src/runtime/xl6_array_rand_shuffle.c']
    sources = [host, 'test/support/array_shuffle_native.c', *providers,
        'src/runtime/xl6_sequence.c', 'src/runtime/xl6_array.c',
        'src/runtime/xl6_array_primitive_admission.c', 'src/runtime/xl6_array_reference_admission.c',
        'src/runtime/xl6_array_future_admission.c', 'src/runtime/xl6_array_erased.c',
        'src/runtime/xl6_array_reference_value.c', 'src/runtime/xl6_value_admission.c',
        'src/runtime/xl6_string_type.c', 'src/runtime/xl6_string_value.c',
        'src/runtime/xl6_nullable_string.c', 'src/runtime/xl6_char_type.c']
    product = root / ('release/xl.dll' if os.name == 'nt' else 'release/libxl.so')
    files = {Path(__file__), Path(cc), Path(nm), Path(sys.executable),
        root / 'lib/xrt/xrt.h', root / 'lib/xruntime/xruntime.h'}
    files.update(root / source for source in sources)
    for directory in ('src/runtime', 'release/include'):
        files.update((root / directory).rglob('*.h'))
    public = None
    fixtures = ['stdlib_array_shuffle_default_only', 'stdlib_array_shuffle_explicit_only', 'stdlib_random_extended']
    if args.language:
        files.update(root / f'test/cases/{name}.xl' for name in fixtures)
        for directory in ('src', 'lib/tcc'):
            files.update(p for p in (root / directory).rglob('*') if p.suffix in ('.c', '.h', '.inc'))
        if args.public_directory:
            public = json.loads((args.public_directory / 'build.json').read_text(encoding='utf-8-sig'))
            assert not public['RuntimeOnly'] and public['SourceXrt']
            staged = json.loads((args.public_directory / 'artifact_sources.json').read_text(encoding='utf-8-sig'))
            for row in public['Inputs'] + public['Headers'] + public['Objects'] + staged['Files']:
                assert digest(Path(row['Path'])) == row['Sha256'].lower(), row['Path']
                files.add(Path(row['Path']))
            cli = Path(public['Driver'])
            files.update((cli, args.public_directory / 'build.json', args.public_directory / 'artifact_sources.json'))
        else:
            cli = root / 'release/xl'
            files.update((cli, root / 'release/libxlc.so'))
        files.update((root / 'test/cases/xlir_module_product_oom_host.c',
            root / 'test/support/module_product_memory.c', root / 'test/support/module_product_memory.h'))
    if args.shared or args.language:
        files.add(product)
    inputs = [{'Path': str(p.resolve()), 'Sha256': digest(p)} for p in sorted(files)]
    rows, sweeps = [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Scope': __doc__, 'Inputs': inputs,
        'Rows': rows, 'Sweeps': sweeps, 'Shared': args.shared, 'Language': args.language,
        'NativeRuntimeInstrumented': args.sanitize and not args.shared, 'LanguageRuntimeInstrumented': False}
    out.mkdir(parents=True)
    env = dict(os.environ)
    if args.sanitize:
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    if os.name == 'nt' and (args.shared or args.language):
        shutil.copy2(product, out / 'xl.dll')
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-I.', '-Irelease', '-Irelease/include']
    platform_links = [] if os.name == 'nt' else ['-pthread', '-lm', '-ldl']
    if os.name != 'nt':
        flags += ['-D_GNU_SOURCE']
    sanitized = ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie'] if args.sanitize else []
    shared_links = [str(product)] if os.name == 'nt' else ['-Lrelease', '-lxl', '-Wl,-rpath,' + str(root / 'release'), *platform_links]

    def run(name: str, command: list[str], timeout: int = 180) -> str:
        try:
            result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=timeout)
        except subprocess.TimeoutExpired as error:
            (out / (name + '.log')).write_bytes((error.stdout or b'') + (error.stderr or b''))
            rows.append({'Name': name, 'Command': command, 'Complete': False, 'Timeout': timeout})
            raise
        data = result.stdout + result.stderr
        (out / (name + '.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode, 'Complete': result.returncode == 0})
        assert result.returncode == 0, name + ': ' + data[-4000:].decode(errors='replace')
        return data.decode(errors='replace')

    try:
        for provider, symbol, forbidden in zip(providers, ('xl6ArrayRngShuffleV1', 'xl6ArrayRandShuffleV1'),
                (r'\bxrtRand\w*\b', r'\bxrtRng\w*\b')):
            obj = out / (Path(provider).stem + '.o')
            run(obj.stem + '.build', [cc, *flags, '-O0', '-c', provider, '-o', str(obj)])
            text = run(obj.stem + '.symbols', [nm, '-g', str(obj)])
            assert symbol in text and not re.search(forbidden, text)
            assert not any(name in text for name in ('xl6ArrayInputAcquireV1', 'xrtTypedArrayGetValue', 'xrtValueInt'))
        for level in (0, 2):
            lane = f'O{level}'
            binary = out / (lane + ('.exe' if os.name == 'nt' else ''))
            selected = [host] if args.shared else sources
            run(lane + '.build', [cc, *flags, *sanitized, f'-O{level}', *selected,
                *(shared_links if args.shared else platform_links), '-o', str(binary)], 360)
            text = run(lane + '.vectors', [str(binary)])
            assert '100 physical cycles balanced; native shuffle zero allocations' in text
            for mode in (0, 1):
                for copy in (0, 1):
                    for point in range(64):
                        text = run(f'{lane}.{mode}.{copy}.{point}', [str(binary), str(mode), str(copy), str(point)])
                        match = re.search(r'hit=([01]) balanced', text)
                        assert match
                        if match[1] == '0':
                            sweeps.append({'Lane': lane, 'Default': mode, 'CowCopy': copy, 'FirstNoHit': point})
                            break
                    else:
                        raise AssertionError('Unclosed wire Array shuffle OOM prefix')
        assert len(sweeps) == 8
        if args.language:
            private = out / 'include/__xl_runtime'
            private.mkdir(parents=True)
            for directory in (root / 'src/runtime', root / 'release/include'):
                for header in directory.glob('*.h'):
                    shutil.copy2(header, private / header.name)
            language_flags = [*flags, '-DXL_STATIC_EMBED', '-I' + str(out / 'include'),
                '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable',
                '-Wno-unused-label', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
                '-Wno-missing-field-initializers', *sanitized]

            def emit(name: str, mode: str, path: Path) -> list[str]:
                source = f'test/cases/{name}.xl'
                if public:
                    return [str(cli), source, mode, str(path)]
                if mode == 'ir-lowered':
                    return [str(cli), 'ir', source, '--lowered', '--format', 'json', '--emit', str(path)]
                return [str(cli), 'cgen', source, '--inc-path', str(out / 'include'), '--emit', str(path)]

            memory = {}
            for level in (0, 2):
                memory[level] = out / f'memory-O{level}.o'
                run(f'memory-O{level}.build', [cc, *language_flags, f'-O{level}', '-c',
                    'test/support/module_product_memory.c', '-o', str(memory[level])])
            for name in fixtures:
                target, ir = out / (name + '.c'), out / (name + '.json')
                for mode, path in (('cgen-default', target), ('ir-lowered', ir)):
                    run(name + '.' + mode, emit(name, mode, path))
                    repeat = path.with_name(path.stem + '.repeat' + path.suffix)
                    run(name + '.' + mode + '.repeat', emit(name, mode, repeat))
                    assert path.read_bytes() == repeat.read_bytes()
                code = target.read_text(encoding='utf-8')
                assert 'xl6_array_shuffle.h' in code
                assert 'xrtValueArraySwap(' not in code and 'count = xrtValueCount(values); for' not in code
                if name != 'stdlib_random_extended':
                    chosen = 'xl6ArrayRandShuffleV1(' if 'default_only' in name else 'xl6ArrayRngShuffleV1('
                    other = 'xl6ArrayRngShuffleV1(' if 'default_only' in name else 'xl6ArrayRandShuffleV1('
                    assert chosen in code and other not in code
                assert 'value-v1' in ir.read_text(encoding='utf-8')
                for level in (0, 2):
                    binary = out / (name + f'.O{level}' + ('.exe' if os.name == 'nt' else ''))
                    run(name + f'.O{level}.build', [cc, *language_flags, f'-O{level}', '-include', str(target),
                        'test/cases/xlir_module_product_oom_host.c', str(memory[level]), *shared_links, '-o', str(binary)], 360)
                    assert 'balanced' in run(name + f'.O{level}.run', [str(binary)])
                prefix = out / (name + '.product')
                source = f'test/cases/{name}.xl'
                if public:
                    run(name + '.jit', [str(cli), source, 'run', str(prefix)])
                    run(name + '.aot', [str(cli), source, 'exe', str(prefix)])
                else:
                    run(name + '.jit', [str(cli), 'run', source, '--inc-path', str(out / 'include')])
                    run(name + '.aot.build', [str(cli), 'build', source, '--inc-path', str(out / 'include'), '-o', str(prefix)])
                    run(name + '.aot', [str(prefix)])
        report['InputsUnchanged'] = all(digest(Path(row['Path'])) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']
        report['Complete'] = True
    except Exception as error:
        report['Error'] = repr(error)
        print(error, file=sys.stderr)
    finally:
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'Array shuffle: {len(rows)} checks, {len(sweeps)} closed prefixes, '
        f'{sum(row["FirstNoHit"]+1 for row in sweeps)} positions, complete={report["Complete"]}')
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
