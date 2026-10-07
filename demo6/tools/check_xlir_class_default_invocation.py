#!/usr/bin/env python3
"""Exact root factory binding; full corrupt-IR sweep once per fixture, repeat/O2 emission and deterministic C/header/IR."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--public', type=Path)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    test_root = Path(__file__).resolve().parents[1]
    cc = shutil.which('gcc')
    if not cc or out.exists() or (os.name == 'nt') != bool(args.public):
        parser.error('Require gcc, fresh evidence; --public only on Windows')
    cases = ['stdlib_persistence_root_default', 'stdlib_persistence_restore_construction',
             'stdlib_persistence_restore_domains', 'stdlib_persistence_restore_language',
             'stdlib_persistence_restore_array_json_prune']
    source = test_root / 'test/cases/xlir_class_default_invocation_contract.c'
    files = {Path(__file__), Path(sys.executable), Path(cc), source}
    files.update(test_root / ('test/cases/' + name + '.xl') for name in cases)
    objects = []
    if args.public:
        directory = args.public.resolve()
        build = json.loads((directory / 'build.json').read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly']
        for row in build['Inputs'] + build['Headers'] + build['Objects']:
            assert sha(row['Path']) == row['Sha256'].lower(), row['Path']
            files.add(Path(row['Path']))
        files.add(directory / 'build.json')
        objects = [r['Path'] for r in build['Objects'] if Path(r['Path']).name != 'xlir_public_source_driver.o']
        flags = ['-I' + str(directory / 'include')]
        links = [str(root / 'release/xl.dll'), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
        files.add(root / 'release/xl.dll')
    else:
        flags = ['-D_GNU_SOURCE']
        links = [str(root / 'release/libxlc.so'), '-Wl,-rpath,' + str(root / 'release'), '-pthread', '-lm', '-ldl']
        for name in ('src', 'lib', 'release/include'):
            files.update(p for p in (root / name).rglob('*') if p.suffix in ('.c', '.h', '.inc'))
        files.update(root / ('release/' + name) for name in ('xl', 'libxl.so', 'libxlc.so'))
    flags += ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-I.', '-Irelease',
              '-Irelease/include', '-Ilib/tcc', '-Wno-pointer-sign', '-Wno-format-truncation',
              '-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter']
    inputs = [{'Path': str(p.resolve()), 'Sha256': sha(p)} for p in sorted(files)]
    out.mkdir(parents=True)
    rows, bindings, first_outputs = [], [], {}
    report = {'Complete': False, 'InputsUnchanged': False, 'Inputs': inputs, 'Rows': rows,
              'Bindings': bindings, 'Scope': __doc__, 'LanguageExecution': False}
    def run(name, command):
        process = subprocess.run([str(p) for p in command], cwd=root, capture_output=True, timeout=300)
        data = process.stdout + process.stderr
        (out / (name + '.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': [str(p) for p in command], 'Exit': process.returncode})
        print(name + ': ' + str(process.returncode), flush=True)
        assert process.returncode == 0, (name, data[-5000:].decode(errors='replace'))
        return data
    try:
        for level in (0, 2):
            binary = out / ('contract-O' + str(level) + ('.exe' if os.name == 'nt' else ''))
            run('build-O' + str(level), [cc, *flags, '-O' + str(level), source, *objects, *links, '-o', binary])
            if os.name == 'nt': shutil.copy2(root / 'release/xl.dll', out / 'xl.dll')
            for name in cases:
                prefix = out / (name + '-O' + str(level))
                targets = [Path(str(prefix) + suffix) for suffix in ('.c', '.h', '.json')]
                extra = ['--emit-only'] if level == 2 else []
                data = run(name + '-O' + str(level), [binary, test_root / ('test/cases/' + name + '.xl'), *targets, *extra])
                match = re.search(rb'Class default: (\d+) checked invocations, (\d+) rejected C/header mutations', data)
                assert match and int(match[1]) > 0 and int(match[2]) == (int(match[1]) * 17 if level == 0 else 0)
                dead = re.search(rb'Class default: (\d+) unreachable caller invocations', data)
                assert dead and (int(dead[1]) == 1 if name == 'stdlib_persistence_restore_language' else int(dead[1]) == 0)
                text = targets[0].read_text(encoding='utf-8')
                assert re.search(r'\br\d+ = xlir_fn_\d+\(\);', text)
                module = json.loads(targets[2].read_text(encoding='utf-8'))
                for factory in module['functions']:
                    if factory['kindName'] == 'class-default' and factory['reachability'] == 'unreachable':
                        assert not re.search(r'\bxlir_fn_' + str(factory['symbol']) + r'\s*\(', text)
                current = [p.read_bytes() for p in targets]
                if level == 0: first_outputs[name] = current
                else: assert current == first_outputs[name], name + ': host O0/O2 changed canonical output'
                bindings.append({'Fixture': name, 'Level': level, 'Invocations': int(match[1]),
                                 'Mutations': int(match[2]), 'UnreachableInvocations': int(dead[1])})
                repeated = [Path(str(p) + '.repeat') for p in targets]
                run(name + '-O' + str(level) + '.repeat', [binary, test_root / ('test/cases/' + name + '.xl'), *repeated, '--emit-only'])
                assert all(p.read_bytes() == q.read_bytes() for p, q in zip(targets, repeated))
        report['InputsUnchanged'] = all(sha(r['Path']) == r['Sha256'] for r in inputs)
        assert report['InputsUnchanged']
        report['Complete'] = True
    except Exception as failure:
        report['Failure'] = type(failure).__name__ + ': ' + str(failure)
        print(report['Failure'], file=sys.stderr)
    finally:
        (out / 'results.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    return 0 if report['Complete'] else 1

if __name__ == '__main__': raise SystemExit(main())
