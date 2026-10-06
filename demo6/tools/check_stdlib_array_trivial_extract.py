#!/usr/bin/env python3
"""Closed trivial Array extraction: actual native/wire profiles, atomic COW and OOM."""
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
    args = parser.parse_args()
    root, out = Path(__file__).resolve().parents[1], args.out.resolve()
    cc, nm = shutil.which('gcc'), shutil.which('nm')
    if not cc or not nm or out.exists() or (args.sanitize and os.name == 'nt'):
        parser.error('Require gcc/nm, a fresh evidence path, and Linux for sanitizers')
    optional = ('bool', 'i64', 'u64', 'f32', 'f64', 'time', 'char',
                'native_i8', 'native_u8', 'native_i16', 'native_u16', 'native_i32', 'native_u32')
    sources = ['src/runtime/xl6_array_extract_trivial.c', 'src/runtime/xl6_sequence.c',
        'src/runtime/xl6_array_primitive_admission.c', 'src/runtime/xl6_array_c_scalar_admission.c',
        'src/runtime/xl6_array_nullable_c_scalar_admission.c', 'src/runtime/xl6_value_admission.c',
        'src/runtime/xl6_array_erased.c', 'src/runtime/xl6_array_reference_value.c',
        'src/runtime/xl6_char_type.c', 'src/runtime/xl6_char_value.c']
    sources += [f'src/runtime/xl6_nullable_{profile}{suffix}.c' for profile in optional for suffix in ('', '_value')]
    host = 'test/cases/stdlib_array_extract_trivial_host.c'
    support = 'test/support/array_import_native.c'
    product = root / ('release/xl.dll' if os.name == 'nt' else 'release/libxl.so')
    files = {Path(__file__), Path(cc), Path(nm), Path(sys.executable), root / host, root / support,
        root / 'lib/xruntime/xruntime.h', root / 'lib/xrt/xrt.h', *[root / source for source in sources]}
    for directory in ('src/runtime', 'release/include'):
        files.update((root / directory).rglob('*.h'))
    if args.shared: files.add(product)
    inputs = [{'Path': str(path.resolve()), 'Sha256': digest(path)} for path in sorted(files)]
    out.mkdir(parents=True)
    env = dict(os.environ)
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-I.', '-Irelease', '-Irelease/include']
    links = [] if os.name == 'nt' else ['-pthread', '-lm']
    if os.name != 'nt': flags += ['-D_GNU_SOURCE']
    if args.sanitize:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
                   UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    if args.shared:
        links = [str(product)] if os.name == 'nt' else ['-Lrelease', '-lxl', '-Wl,-rpath,'+str(root/'release'), *links]
        if os.name == 'nt': shutil.copy2(product, out / 'xl.dll')
    rows, sweeps = [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Inputs': inputs, 'Rows': rows, 'Sweeps': sweeps,
              'Profiles': 50, 'SharedRuntime': args.shared, 'RuntimeInstrumented': args.sanitize and not args.shared,
              'Scope': __doc__}
    def run(name, command):
        result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=240)
        data = result.stdout + result.stderr
        (out / (name+'.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode})
        if result.returncode: raise RuntimeError(name+': '+data[-4000:].decode(errors='replace'))
        return data.decode(errors='replace')
    try:
        for level in (0, 2):
            objects = []
            if not args.shared:
                for source in [*sources, support]:
                    target = out / f'O{level}.{Path(source).stem}.o'
                    run(f'O{level}.compile.{Path(source).stem}', [cc, *flags, f'-O{level}',
                        '-DXL6_FACTORY_PROFILE=0', '-c', source, '-o', str(target)])
                    objects.append(str(target))
            binary = out / (f'profiles.O{level}'+('.exe' if os.name == 'nt' else ''))
            run(f'O{level}.build', [cc, *flags, f'-O{level}', host, *objects, *links, '-o', str(binary)])
            proof = run(f'O{level}.profiles', [str(binary)])
            match = re.search(r'50 closed trivial profiles, (\d+) complete prefixes, 100 cycles balanced', proof)
            assert match and int(match[1]) == 300
            sweeps.append({'Level': level, 'CompletePrefixes': int(match[1])})
        report['InputsUnchanged'] = all(digest(Path(row['Path'])) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete'] = True
    except Exception as error:
        report['Error'] = repr(error); print(error, file=sys.stderr)
    finally:
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(f'Trivial extraction: {len(rows)} checks, {sum(row["CompletePrefixes"] for row in sweeps)} complete prefixes, complete={report["Complete"]}')
    return 0 if report['Complete'] else 1


if __name__ == '__main__': raise SystemExit(main())
