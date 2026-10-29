#!/usr/bin/env python3
"""Public native Queue API: deterministic closed IR/C, strict C, JIT and AOT.

This gate is not the full 53-module or language OOM/ownership-graph acceptance.
Every case is attempted; failures remain in the report for batch repair.
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
import time


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--public-directory', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    cc = shutil.which('gcc')
    if not cc or out.exists() or ((os.name == 'nt') != bool(args.public_directory)):
        parser.error('Require gcc, fresh evidence, and a Windows source build / Linux source release')
    cases = ['stdlib_concurrent_queue_recipes', 'stdlib_concurrent_queue_owned',
             'stdlib_concurrent_queue_callable_future', 'stdlib_special_collections_concurrent',
             'stdlib_concurrent_queue_spsc_only', 'stdlib_concurrent_queue_mpsc_only',
             'stdlib_concurrent_queue_mpmc_only']
    cases += ['stdlib_nullable_projection_' + kind for kind in ('bool', 'char', 'f32', 'f64', 'i64', 'u64', 'time')]
    cases += ['stdlib_nullable_queue_transport', 'stdlib_nullable_resource_transport',
              'stdlib_nullable_text_batch_projection', 'stdlib_nullable_text_resource_transport',
              'stdlib_scalar_projection_char', 'stdlib_scalar_projection_text', 'stdlib_special_collections']
    cases += ['stdlib_tree_native_projection']
    rejected = [('legacy_into', '[SC-QUEUE-OWNED]'),
                ('wrong_array_owner', '队列批量输入'), ('wrong_element', '队列批量输入')]
    files = {Path(__file__).resolve(), Path(cc), Path(sys.executable)}
    for folder in ('src', 'lib', 'release/include'):
        files.update(p for p in (root / folder).rglob('*') if p.suffix in ('.c', '.h', '.inc'))
    files.update(root / f'test/cases/{case}.xl' for case in cases)
    files.update(root / f'test/fixtures/native_queue_rejected/{name}.xl' for name, _ in rejected)
    cli = root / 'release/xl'
    if args.public_directory:
        public = args.public_directory.resolve()
        for name, fields in [('build.json', ['Inputs', 'Headers', 'Objects']),
                             ('artifact_sources.json', ['Files'])]:
            manifest = public / name
            data = json.loads(manifest.read_text(encoding='utf-8-sig'))
            files.add(manifest)
            for field in fields:
                for row in data[field]:
                    assert digest(row['Path']) == row['Sha256'].lower(), 'Stale package: ' + row['Path']
                    files.add(Path(row['Path']))
            if name == 'build.json':
                assert data['SourceXrt'] and not data['RuntimeOnly'] and data['EnableExeCompiler']
                cli = Path(data['Driver'])
                assert digest(cli) == data['DriverSha256'].lower()
        files.add(root / 'release/xl.dll')
        private_include = public / 'include'
    else:
        files.update(root / ('release/' + name) for name in ('libxl.so', 'libxlc.so'))
        private_include = out / 'include'
    files.add(cli)
    inputs = [{'Path': str(p.resolve()), 'Sha256': digest(p)} for p in sorted(files)]
    out.mkdir(parents=True)
    if not args.public_directory:
        private = private_include / '__xl_runtime'
        private.mkdir(parents=True)
        for directory in (root / 'src/runtime', root / 'release/include'):
            for header in directory.glob('*.h'):
                shutil.copy2(header, private / header.name)
    rows, failures, proofs = [], [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Scope': __doc__,
              'Full53Acceptance': False, 'LanguageOomAcceptance': False,
              'OwnershipGraphAcceptance': False, 'Inputs': inputs,
              'Rows': rows, 'Failures': failures, 'Proofs': proofs}

    def run(name, command, rejection=None):
        command = list(map(str, command))
        start = time.monotonic()
        try:
            result = subprocess.run(command, cwd=root, capture_output=True, timeout=120)
            output = result.stdout + result.stderr
            code = result.returncode
        except subprocess.TimeoutExpired as error:
            output = (error.stdout or b'') + (error.stderr or b'')
            code = 'timeout-120s'
        (out / (name + '.log')).write_bytes(output)
        rows.append({'Name': name, 'Command': command, 'Exit': code,
                     'Seconds': time.monotonic() - start})
        if rejection is not None:
            if code == 0 or isinstance(code, str) or rejection not in output.decode(errors='replace'):
                raise RuntimeError(name + ': expected semantic rejection: ' + output[-4000:].decode(errors='replace'))
            return
        if code != 0:
            raise RuntimeError(name + ': ' + output[-4000:].decode(errors='replace'))

    def emit(case, suffix):
        source = root / f'test/cases/{case}.xl'
        prefix = out / (case + suffix)
        if args.public_directory:
            run(case + suffix + '.emit', [cli, source, 'emit', prefix])
        else:
            run(case + suffix + '.C', [cli, 'cgen', source, '--emit', str(prefix) + '.c'])
            run(case + suffix + '.IR', [cli, 'ir', source, '--lowered', '--format', 'json',
                                        '--emit', str(prefix) + '.json'])
        return prefix

    for case in cases:
        source = root / f'test/cases/{case}.xl'
        try:
            prefix = emit(case, '')
            second = emit(case, '.second')
            for extension in ('.c', '.json'):
                assert Path(str(prefix) + extension).read_bytes() == Path(str(second) + extension).read_bytes(), case
            code = Path(str(prefix) + '.c').read_text(encoding='utf-8')
            ir = json.loads(Path(str(prefix) + '.json').read_text(encoding='utf-8-sig'))
            assert 'xlir_special_queue_push_batch' not in code and 'xlir_special_queue_pop_batch_into' not in code
            assert 'xlir_avl_collect' not in code
            definitions = {item['result']: item for item in ir['instructions'] if item['result']}
            types = {item['id']: item for item in ir['types']}
            calls = []
            for index, instruction in enumerate(ir['instructions']):
                operation = instruction['runtimeOpName']
                if instruction['kindName'] != 'runtime-call' or not operation.startswith(('special-queue-', 'concurrent-queue-')):
                    continue
                owner = instruction['type'] if operation == 'special-queue-create' else definitions[instruction['operands'][0]]['type']
                if types[owner]['runtimeResourceAbiName'] == 'wait-queue':
                    continue
                assert types[owner]['messageSchemaType'] and instruction['result']
                assert operation not in ('special-queue-try-pop-into', 'special-queue-pop-batch-into')
                check = ir['instructions'][index + 1]
                assert check['kindName'] == 'error-test' and check['operands'] == [instruction['result']], operation
                calls.append(operation)
            assert calls, case
            native = set(re.findall(r'xl6(Spsc|Mpsc|Mpmc)Queue\w+\(', code))
            if case.endswith('_only'):
                expected = case.rsplit('_', 2)[1].capitalize()
                assert native == {expected}, (case, native)
                assert 'xlir_channel_' not in code and 'xlir_special_queue_' not in code
            strict = [cc, '-std=c11', '-Wall', '-Wextra', '-Werror',
                      '-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-label',
                      '-Wno-unused-parameter', '-Wno-unused-const-variable',
                      '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers',
                      '-I' + str(private_include), '-Irelease']
            for level in (0, 2):
                run(case + f'.strict.O{level}', [*strict, f'-O{level}', '-c', str(prefix) + '.c',
                                                '-o', str(prefix) + f'.O{level}.o'])
            binary = out / (case + '.aot' + ('.exe' if os.name == 'nt' else ''))
            if args.public_directory:
                run(case + '.JIT', [cli, source, 'run', str(prefix) + '.jit'])
                run(case + '.AOT.build', [cli, source, 'exe', str(prefix) + '.aot'])
            else:
                run(case + '.JIT', [cli, 'run', source])
                run(case + '.AOT.build', [cli, 'build', source, '-o', binary])
            run(case + '.AOT.run', [binary])
            proofs.append({'Case': case, 'NativeFamilies': sorted(native),
                           'CheckedCalls': len(calls), 'Operations': sorted(set(calls))})
        except Exception as error:
            failures.append({'Case': case, 'Error': repr(error)})
            print(case + ': ' + str(error), file=sys.stderr, flush=True)
    for name, diagnostic in rejected:
        source = root / f'test/fixtures/native_queue_rejected/{name}.xl'
        prefix = out / ('rejected.' + name)
        generated = Path(str(prefix) + '.c')
        generated.write_bytes(b'KEEP')
        try:
            command = [cli, source, 'emit', prefix] if args.public_directory else [cli, 'cgen', source, '--emit', generated]
            run('rejected.' + name, command, rejection=diagnostic)
            assert generated.read_bytes() == b'KEEP', 'Rejected program overwrote output'
        except Exception as error:
            failures.append({'Case': 'rejected.' + name, 'Error': repr(error)})
    report['InputsUnchanged'] = all(digest(row['Path']) == row['Sha256'] for row in inputs)
    report['Complete'] = not failures and report['InputsUnchanged'] and len(proofs) == len(cases)
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'Public Queue gate: {len(rows)} phases, {len(proofs)} cases passed, complete={report["Complete"]}', flush=True)
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
