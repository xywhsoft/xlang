#!/usr/bin/env python3
"""Check public key callback failure boundaries in both bundled runtimes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--compiler', default='gcc')
    parser.add_argument('--sanitize', action='store_true', help='Also instrument complete selected single-header implementations')
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    source = root / 'test/cases/xrt_key_callback_boundary_host.c'
    inputs = [Path(__file__).resolve(), source]
    for product in ('xrt', 'xruntime'):
        inputs.extend([root / f'lib/{product}/{product}.h',
                       root / f'lib/{product}/{product}.snapshot.json',
                       root / f'release/include/{product}_decl.h'])
    before = {str(path): sha(path) for path in inputs}
    for product in ('xrt', 'xruntime'):
        metadata = json.loads((root / f'lib/{product}/{product}.snapshot.json').read_text())
        for section, path in [('implementation', root / f'lib/{product}/{product}.h'),
                              ('declarations', root / f'release/include/{product}_decl.h')]:
            assert metadata[section]['sha256'].lower() == sha(path)
            assert metadata[section]['bytes'] == path.stat().st_size
    rows = []

    def run(name, command):
        process = subprocess.run(command, cwd=root, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (out / (name + '.log')).write_bytes(process.stdout)
        rows.append({'Name': name, 'Command': command, 'Exit': process.returncode,
                     'Complete': process.returncode == 0})
        print(name + ': ' + str(process.returncode), flush=True)
        return process.returncode == 0

    for product in ('xrt', 'xruntime'):
        for declaration_first in (False, True):
            for mode in (['O0', 'O2', 'sanitized'] if args.sanitize else ['O0', 'O2']):
                name = f'{product}.decl-{int(declaration_first)}.{mode}'
                executable = out / (name + ('.exe' if os.name == 'nt' else ''))
                command = [args.compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                           '-DXRT_MODULE_VALUE_GRAPH', '-DXRT_MODULE_VALUE_COLLECTION', '-DXRT_MODULE_SET']
                if product == 'xruntime':
                    command += ['-DXRUNTIME_MODULE_RUNTIME_TYPE', '-DXRUNTIME_IMPLEMENTATION','-DXRT_IMPLEMENTATION',
                        '-include',str(root/'lib/xruntime/xruntime_features.h'),'-include',str(root/'lib/xrt/xrt.h')]
                else:
                    command += ['-DXRT_IMPLEMENTATION']
                if mode == 'sanitized':
                    command += ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
                else:
                    command += ['-' + mode]
                if declaration_first:
                    command += ['-include', str(root / f'release/include/{product}_decl.h')]
                command += ['-include', str(root / f'lib/{product}/{product}.h'), str(source)]
                if os.name != 'nt':
                    command += ['-pthread', '-lm', '-ldl']
                command += ['-o', str(executable)]
                if run(name + '.compile', command):
                    run(name + '.run', [str(executable)])
    unchanged = all(sha(Path(path)) == digest for path, digest in before.items())
    result = {'Complete': unchanged and len(rows) == (24 if args.sanitize else 16) and
                          all(row['Complete'] for row in rows),
              'InputsUnchanged': unchanged, 'Inputs': [{'Path': path, 'Sha256': digest}
                                                      for path, digest in before.items()],
              'Rows': rows, 'Scope': 'public XRT/xruntime key error ABI, both declaration orders, O0/O2',
              'Sanitized': args.sanitize, 'LanguageExecution': False}
    (out / 'results.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    return 0 if result['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
