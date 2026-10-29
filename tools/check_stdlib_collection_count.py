#!/usr/bin/env python3
"""Collection count: native/wire, exact IR, generated sync/continuations, JIT/AOT.

Standalone --sanitize instruments the actual selected native sources. --shared
instruments host only. Language sanitizer covers generated C/host, not products.
Language producers remain value-v1; a separate generated-function host supplies
a genuine native Array. No complete source Array cutover is claimed.
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
    root, out = Path(__file__).resolve().parents[1], args.out.resolve()
    cc, nm = shutil.which('gcc'), shutil.which('nm')
    if not cc or not nm or out.exists() or (args.sanitize and os.name == 'nt') or (
            args.language and os.name == 'nt' and not args.public_directory):
        parser.error('Require gcc/nm, fresh evidence, Linux sanitizer and Windows current-source compiler')
    host = 'test/cases/stdlib_collection_count_native_host.c'
    provider = 'src/runtime/xl6_collection_count.c'
    sources = [host, 'test/support/collection_count_native.c', provider,
        'src/runtime/xl6_sequence.c', 'src/runtime/xl6_array.c',
        'src/runtime/xl6_array_erased.c', 'src/runtime/xl6_array_reference_value.c']
    sources.append('src/api/xl_foreign_error.c')
    product = root / ('release/xl.dll' if os.name == 'nt' else 'release/libxl.so')
    files = {Path(__file__), Path(cc), Path(nm), Path(sys.executable),
        root / 'lib/xrt/xrt.h', root / 'lib/xruntime/xruntime.h'}
    files.update(root / source for source in sources)
    files.add(root / 'src/api/xl_foreign_error.h')
    files.add(root / 'test/cases/stdlib_collection_count_header.c')
    for directory in ('src/runtime', 'release/include'):
        files.update((root / directory).rglob('*.h'))
    public = None
    fixtures = ['stdlib_collection_count', 'stdlib_collection_count_continuation']
    if args.language:
        files.update(root / f'test/cases/{name}.xl' for name in fixtures)
        for directory in ('src', 'lib/tcc'):
            files.update(p for p in (root / directory).rglob('*') if p.suffix in ('.c', '.h', '.inc'))
        files.update(root / path for path in ('test/cases/xlir_collection_count_contract.c',
            'test/cases/stdlib_collection_count_projection_host.c', 'test/cases/xlir_module_product_oom_host.c',
            'test/support/module_product_memory.c', 'test/support/module_product_memory.h'))
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
    if args.shared or args.language:
        files.add(product)
    inputs = [{'Path': str(path.resolve()), 'Sha256': digest(path)} for path in sorted(files)]
    rows, sweeps = [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Scope': __doc__, 'Inputs': inputs,
        'Rows': rows, 'Sweeps': sweeps, 'NativeRuntimeInstrumented': args.sanitize and not args.shared,
        'LanguageRuntimeInstrumented': False, 'Shared': args.shared, 'Language': args.language}
    out.mkdir(parents=True)
    env = dict(os.environ)
    if args.sanitize:
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    if os.name == 'nt' and (args.shared or args.language):
        shutil.copy2(product, out / 'xl.dll')
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-I.', '-Irelease', '-Irelease/include']
    platform = [] if os.name == 'nt' else ['-pthread', '-lm', '-ldl']
    if os.name != 'nt':
        flags += ['-D_GNU_SOURCE']
    sanitized = ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie'] if args.sanitize else []
    links = [str(product)] if os.name == 'nt' else ['-Lrelease', '-lxl', '-Wl,-rpath,' + str(root / 'release'), *platform]

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
        leaf = out / ('header.exe' if os.name == 'nt' else 'header')
        run('header.build', [cc, '-std=c11', '-pedantic-errors', '-Wall', '-Wextra', '-Werror', '-I.',
            'test/cases/stdlib_collection_count_header.c', '-o', str(leaf)])
        run('header.run', [str(leaf)])
        obj = out / 'provider.o'
        run('provider.build', [cc, *flags, '-O0', '-c', provider, '-o', str(obj)])
        text = run('provider.symbols', [nm, '-g', str(obj)])
        assert all(name in text for name in ('xl6CollectionCountV1', 'xl6CollectionCountIfPresentV1', 'xl6CollectionCountOrNullV1'))
        assert not any(name in text for name in ('xrtTypedArrayGetValue', 'xl6ArrayInputAcquireV1', 'xl6ArrayCreateV1', 'xrtValueClone'))
        for level in (0, 2):
            lane = f'O{level}'
            binary = out / (lane + ('.exe' if os.name == 'nt' else ''))
            run(lane + '.build', [cc, *flags, *sanitized, f'-O{level}',
                *([host] if args.shared else sources), *(links if args.shared else platform), '-o', str(binary)], 360)
            assert '100 physical cycles balanced' in run(lane + '.vectors', [str(binary)])
            for mode in (0, 1):
                for point in range(32):
                    text = run(f'{lane}.{mode}.{point}', [str(binary), str(mode), str(point)])
                    match = re.search(r'hit=([01]) balanced', text)
                    assert match
                    if match[1] == '0':
                        sweeps.append({'Lane': lane, 'Native': mode, 'FirstNoHit': point})
                        break
                else:
                    raise AssertionError('Unclosed count box allocation prefix')
        assert len(sweeps) == 4
        if args.language:
            private = out / 'include/__xl_runtime'
            private.mkdir(parents=True)
            for directory in (root / 'src/runtime', root / 'release/include'):
                for header in directory.glob('*.h'):
                    shutil.copy2(header, private / header.name)
            strict = [*flags, '-DXL_STATIC_EMBED', '-I' + str(out / 'include'),
                '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable',
                '-Wno-unused-label', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
                '-Wno-missing-field-initializers', *sanitized]
            contract = out / ('contract.exe' if os.name == 'nt' else 'contract')
            compiler_links = ([row['Path'] for row in public['Objects'] if Path(row['Path']).name != 'xlir_public_source_driver.o'] +
                [str(product), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if public else ['-lxlc', *links])
            run('contract.build', [cc, *flags, '-DXL_STATIC_EMBED', '-Ilib/tcc', '-O0',
                'test/cases/xlir_collection_count_contract.c', *compiler_links, '-o', str(contract)], 360)
            for name in fixtures:
                target, ir, bindings = out / (name + '.c'), out / (name + '.json'), out / 'bindings.h'
                source = f'test/cases/{name}.xl'
                for mode, path in (('cgen-default', target), ('ir-lowered', ir)):
                    def emit(p: Path) -> list[str]:
                        if public:
                            return [str(cli), source, mode, str(p)]
                        return [str(cli), 'ir', source, '--lowered', '--format', 'json', '--emit', str(p)] if mode == 'ir-lowered' else [
                            str(cli), 'cgen', source, '--inc-path', str(out / 'include'), '--emit', str(p)]
                    run(name + '.' + mode, emit(path))
                    repeat = path.with_name(path.stem + '.repeat' + path.suffix)
                    run(name + '.' + mode + '.repeat', emit(repeat))
                    assert path.read_bytes() == repeat.read_bytes()
                original = target.read_bytes()
                text = run(name + '.contract', [str(contract), source, str(target), *([str(bindings)] if name == fixtures[0] else [])])
                assert 'mutations refused before C/header output' in text and original == target.read_bytes()
                code = original.decode('utf-8')
                assert 'xl6_collection_count.h' in code and '(int64_t)xrtValueCount(v' not in code
                if name == fixtures[0]:
                    assert all(symbol + '(' in code for symbol in (
                        'xl6CollectionCountV1', 'xl6CollectionCountIfPresentV1', 'xl6CollectionCountOrNullV1'))
                for level in (0, 2):
                    binary = out / (name + f'.O{level}' + ('.exe' if os.name == 'nt' else ''))
                    memory = out / f'memory.O{level}.o'
                    if not memory.exists():
                        run(f'memory.O{level}.build', [cc, *strict, f'-O{level}', '-c', 'test/support/module_product_memory.c', '-o', str(memory)])
                    run(name + f'.O{level}.build', [cc, *strict, f'-O{level}', '-include', str(target),
                        'test/cases/xlir_module_product_oom_host.c', str(memory), *links, '-o', str(binary)], 360)
                    assert 'balanced' in run(name + f'.O{level}.run', [str(binary)])
                    if name == fixtures[0]:
                        projected = out / (f'projection.O{level}' + ('.exe' if os.name == 'nt' else ''))
                        run(f'projection.O{level}.build', [cc, *strict, f'-O{level}', '-include', str(target), '-include', str(bindings),
                            'test/cases/stdlib_collection_count_projection_host.c', *links, '-o', str(projected)], 360)
                        assert '100 physical cycles balanced' in run(f'projection.O{level}.run', [str(projected)])
                prefix = out / (name + '.product')
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
    print(f'Collection count: {len(rows)} checks, {len(sweeps)} closed prefixes, complete={report["Complete"]}')
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
