#!/usr/bin/env python3
"""Actual recursive Sequence factories: deterministic IR/C, strict C, JIT/AOT and complete caller-thread OOM prefixes."""
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
    parser.add_argument('--sanitize', action='store_true', help='Host/generated C only, not the shared product')
    parser.add_argument('--algebra', action='store_true', help='Array concatenation outcome, source independence and cleanup')
    parser.add_argument('--import-arrays', action='store_true', help='Actual erased-to-native Array language import')
    args = parser.parse_args()
    root, out = Path(__file__).resolve().parents[1], args.out.resolve()
    cc = shutil.which('gcc')
    if not cc or out.exists() or (args.sanitize and os.name == 'nt') or (args.algebra and args.import_arrays):
        parser.error('Require gcc, fresh evidence and Linux for sanitizers')
    cases = ['stdlib_sequence_nested_schema', 'stdlib_sequence_nested_schema_continuation',
             'stdlib_stack_admitted_construction', 'stdlib_stack_nested_factory_only', 'stdlib_stack_factory_only']
    if args.algebra:
        cases = ['stdlib_array_algebra_language']
    if args.import_arrays:
        cases = ['stdlib_array_import_language']
    runtime = root / ('release/xl.dll' if os.name == 'nt' else 'release/libxl.so')
    compiler = root / ('release/xlc.dll' if os.name == 'nt' else 'release/libxlc.so')
    cli = root / ('release/xl.exe' if os.name == 'nt' else 'release/xl')
    files = {Path(__file__), Path(sys.executable), Path(cc), runtime, compiler, cli,
             root / 'lib/xrt/xrt.h', root / 'lib/xruntime/xruntime.h',
             root / 'test/cases/xlir_module_product_oom_host.c',
             root / 'test/support/module_product_memory.c', root / 'test/support/module_product_memory.h'}
    for folder in ('src', 'release/include', 'lib/tcc'):
        files.update(p for p in (root / folder).rglob('*') if p.suffix in ('.c', '.h', '.inc'))
    files.update(root / f'test/cases/{case}.xl' for case in cases)
    if args.algebra:
        files.add(root / 'test/cases/xlir_array_algebra_outcome_contract.c')
    if args.import_arrays:
        files.add(root / 'test/cases/xlir_array_import_outcome_contract.c')
    public = None
    if args.public_directory:
        public = json.loads((args.public_directory / 'build.json').read_text(encoding='utf-8-sig'))
        staged = json.loads((args.public_directory / 'artifact_sources.json').read_text(encoding='utf-8-sig'))
        assert not public['RuntimeOnly'] and public['SourceXrt']
        for row in public['Inputs'] + public['Headers'] + public['Objects'] + staged['Files']:
            assert digest(Path(row['Path'])) == row['Sha256'].lower(), 'Stale source build: ' + row['Path']
            files.add(Path(row['Path']))
        cli = Path(public['Driver'])
        files.update((cli, args.public_directory / 'build.json', args.public_directory / 'artifact_sources.json'))
    inputs = [{'Path': str(p.resolve()), 'Sha256': digest(p)} for p in sorted(files)]
    out.mkdir(parents=True)
    private = out / 'include/__xl_runtime'
    private.mkdir(parents=True)
    for folder in (root / 'src/runtime', root / 'release/include'):
        for header in folder.glob('*.h'):
            shutil.copy2(header, private / header.name)
    if os.name == 'nt':
        shutil.copy2(runtime, out / 'xl.dll')
    rows, sweeps, failures = [], [], []
    report = {'Complete': False, 'InputsUnchanged': False,
              'Scope': 'Actual erased-to-native Array language import' if args.import_arrays else
                  'Array concatenation outcome, source independence and cleanup' if args.algebra else __doc__, 'Inputs': inputs,
              'Rows': rows, 'Sweeps': sweeps, 'Failures': failures, 'Cases': cases,
              'SanitizedHost': args.sanitize, 'SharedRuntimeInstrumented': False, 'PublicSourceDriver': public is not None}
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-I.', '-Irelease',
             '-Irelease/include', '-I' + str(out / 'include'), '-Wno-unused-function', '-Wno-unused-parameter',
             '-Wno-unused-variable', '-Wno-unused-label', '-Wno-unused-const-variable',
             '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers']
    links = [str(runtime)] if os.name == 'nt' else ['-Lrelease', '-lxl', '-Wl,-rpath,' + str(root / 'release'),
                                                     '-pthread', '-lm', '-ldl']
    env = dict(os.environ)
    if (args.algebra or args.import_arrays) and public:
        # The private Sequence handle ops are inseparable from their factories
        # and consumers. Use the complete frozen source runtime/compiler here,
        # not a new concat object mixed with the released DLL's old owner ops.
        source_objects = [row['Path'] for row in public['Objects']
                          if Path(row['Path']).name != 'xlir_public_source_driver.o']
        links = [*source_objects, *links, '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
        report['SourceRuntimeObjects'] = source_objects
    if os.name != 'nt':
        flags += ['-D_GNU_SOURCE']
    if args.sanitize:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
                   UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')

    def run(name, command):
        command = [str(p) for p in command]
        result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=600)
        data = result.stdout + result.stderr
        (out / (name + '.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode})
        assert result.returncode == 0, name + ': ' + data[-4000:].decode(errors='replace')
        return data

    def emit(source, mode, target):
        if public:
            return [cli, source, mode, target]
        if mode == 'cgen-default':
            return [cli, 'cgen', source, '--emit', target]
        return [cli, 'ir', source, '--' + ('typed' if mode == 'ir-typed' else 'lowered'),
                '--format', 'json', '--emit', target]

    try:
        if args.algebra or args.import_arrays:
            contract_case = 'xlir_array_import_outcome_contract' if args.import_arrays else 'xlir_array_algebra_outcome_contract'
            contract = out / ('outcome_contract.exe' if os.name == 'nt' else 'outcome_contract')
            contract_links = links if public else ['-Lrelease', '-lxlc', *links]
            run('outcome.contract.build', [cc, *flags, '-O0', f'test/cases/{contract_case}.c',
                                           *contract_links, '-o', contract])
            expected_contract = b'Array import outcome:' if args.import_arrays else b'Array algebra outcome:'
            assert expected_contract in run('outcome.contract', [contract])
        for level in (0, 2):
            run(f'memory.O{level}.build', [cc, *flags, f'-O{level}', '-c',
                'test/support/module_product_memory.c', '-o', out / f'memory.O{level}.o'])
        # Independent cases continue after a failure to collect one useful
        # discovery batch; no failure, skipped stage or incomplete prefix is green.
        for case in cases:
            try:
                source = 'test/cases/' + case + '.xl'
                target = out / (case + '.c')
                for mode, suffix in [('cgen-default', '.c'), ('ir-typed', '.typed.json'), ('ir-lowered', '.json')]:
                    path = out / (case + suffix)
                    repeated = path.with_name(path.stem + '.repeat' + path.suffix)
                    run(case + '.' + mode, emit(source, mode, path))
                    run(case + '.' + mode + '.repeat', emit(source, mode, repeated))
                    assert path.read_bytes() == repeated.read_bytes(), 'Nondeterministic ' + mode
                code = target.read_text(encoding='utf-8')
                assert 'xl6SequenceNestedCreateV1' not in code
                if args.import_arrays:
                    assert 'xl6ArrayImportChainSourceTakeV1(' in code
                    assert 'xl6CollectionCheckedTakeV1(' not in code and 'xrtValueRebindTypeId' not in code
                    lowered = json.loads((out / (case + '.json')).read_text(encoding='utf-8'))
                    imports = [row for row in lowered['instructions'] if row['runtimeOpName'] == 'array-import-chain']
                    assert len(imports) >= 15 and all(int(row['operandCount']) == 1 for row in imports)
                    for row in imports:
                        test = lowered['instructions'][int(row['index']) + 1]
                        assert test['kindName'] == 'error-test' and test['operands'] == [int(row['result'])]
                        assert int(test['trueTarget']) != int(test['falseTarget']), 'Unchecked Array import result'
                elif args.algebra:
                    assert 'xl6ArrayConcatV1(' in code and 'xrtValueArrayConcat(' not in code
                    lowered = json.loads((out / (case + '.json')).read_text(encoding='utf-8'))
                    concat = [row for row in lowered['instructions'] if row['runtimeOpName'] == 'xvalue-array-concat']
                    assert concat and all(int(row['operandCount']) == 2 for row in concat)
                    for row in concat:
                        test = lowered['instructions'][int(row['index']) + 1]
                        assert test['kindName'] == 'error-test' and test['operands'] == [int(row['result'])]
                        assert int(test['trueTarget']) != int(test['falseTarget']), 'Unchecked Array concat result'
                elif case != 'stdlib_stack_factory_only':
                    assert 'xl6SequenceNestedCreateTakeV2(' in code and 'xlir_sequence_schema_1' in code
                for level in (0, 2):
                    binary = out / (case + f'.O{level}' + ('.exe' if os.name == 'nt' else ''))
                    user_entry = case.startswith('stdlib_stack_')
                    run(case + f'.O{level}.build', [cc, *flags, f'-O{level}',
                        *(['-DXL6_PRODUCT_USER_ENTRY'] if user_entry else []), '-include', target,
                        'test/cases/xlir_module_product_oom_host.c', out / f'memory.O{level}.o', *links, '-o', binary])
                    assert b'balanced' in run(case + f'.O{level}.vectors', [binary])
                    for point in range(4097):
                        data = run(case + f'.O{level}.oom.{point}', [binary, point])
                        assert b'balanced' in data
                        if b'hit=0 balanced' in data:
                            assert point > 0, 'The language body did not execute allocation work'
                            sweeps.append({'Case': case, 'Level': level, 'FirstNoHit': point})
                            break
                    else:
                        raise AssertionError('Unclosed allocation prefix: ' + case)
                product = out / (case + '.product' + ('.exe' if os.name == 'nt' else ''))
                if public:
                    run(case + '.JIT', [cli, source, 'run', out / (case + '.jit')])
                    run(case + '.AOT.build', [cli, source, 'exe', product])
                else:
                    run(case + '.JIT', [cli, 'run', source])
                    run(case + '.AOT.build', [cli, 'build', source, '-o', product])
                run(case + '.AOT.run', [product])
            except Exception as failure:
                failures.append({'Case': case, 'Failure': str(failure)})
                print(failures[-1], file=sys.stderr)
        report['InputsUnchanged'] = all(digest(Path(row['Path'])) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged'], 'Inputs changed during verification'
        assert not failures and len(sweeps) == 2 * len(cases), 'Incomplete language batch'
        report['Complete'] = True
    except Exception as failure:
        report['Failure'] = str(failure)
        print(report['Failure'], file=sys.stderr)
    finally:
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'Sequence schema language: {len(rows)} checks, {len(sweeps)} complete prefixes; complete={report["Complete"]}')
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
