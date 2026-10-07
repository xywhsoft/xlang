#!/usr/bin/env python3
"""Actual lowered projection tables: schema/erased Array/unused-format pruning and corrupt-IR refusal.

This checks IR data and corrupt-IR refusal, not execution of PROJECT or serializers.
O0/O2 optimize the contract host; the linked source compiler keeps its build mode.
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
    hashed = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            hashed.update(chunk)
    return hashed.hexdigest()

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--test-root', type=Path)
    parser.add_argument('--public-directory', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    test_root = args.test_root.resolve() if args.test_root else root
    cc = shutil.which('gcc')
    if not cc or out.exists() or (os.name == 'nt' and not args.public_directory):
        parser.error('Require gcc, a fresh evidence directory, and a current-source Windows compiler')
    source = test_root / 'test/cases/xlir_persistence_project_plan_contract.c'
    semantic = test_root / 'test/cases/xlir_persistence_field_inclusion_contract.c'
    bridge = test_root / 'test/cases/xlir_dynamic_fields_contract_host.c'
    cases = [('schema', test_root / 'test/cases/xlir_persistence_project_plan.xl'),
             ('sequences', test_root / 'test/cases/xlir_persistence_project_sequences.xl'),
             ('roots', test_root / 'test/cases/stdlib_array_persistence_language.xl'),
             ('empty', test_root / 'test/cases/stdlib_dynamic_numeric_prune.xl'),
             ('defaults', test_root / 'test/cases/cgen_string_persistence_default_nul.xl')]
    cases.append(('readers', test_root / 'test/cases/stdlib_serialization_read_errors.xl'))
    cases.append(('configured', test_root / 'test/cases/stdlib_persistence_configured_language.xl'))
    cases.append(('sinks', test_root / 'test/cases/stdlib_persistence_sink_language.xl'))
    cases.append(('files', test_root / 'test/cases/stdlib_persistence_file_language.xl'))
    cases.append(('writers', test_root / 'test/cases/stdlib_persistence_writer_language.xl'))
    cases.append(('nominal', test_root / 'test/cases/stdlib_persistence_nominal_language.xl'))
    cases.append(('pretty', test_root / 'test/cases/stdlib_persistence_pretty_language.xl'))
    cases.append(('restore', test_root / 'test/cases/stdlib_persistence_restore_language.xl'))
    cases.append(('restore_domains', test_root / 'test/cases/stdlib_persistence_restore_domains.xl'))
    refusals = [test_root / ('test/cases/' + name + '.xl') for name in
                ('xlir_persistence_project_included_reject', 'xlir_persistence_project_format_skip_reject')]
    files = {Path(__file__).resolve(), Path(sys.executable), Path(cc), source, semantic,
             bridge, test_root / 'test/cases/xlir_dynamic_fields_contract.inc',
             *(path for _, path in cases), *refusals}
    product = root / ('release/xl.dll' if os.name == 'nt' else 'release/libxlc.so')
    files.add(product)
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
             '-I.', '-Irelease', '-Irelease/include', '-Ilib/tcc']
    if args.public_directory:
        manifest = args.public_directory.resolve() / 'build.json'
        public = json.loads(manifest.read_text(encoding='utf-8-sig'))
        files.add(manifest)
        for row in public['Inputs'] + public['Headers'] + public['Objects']:
            path = Path(row['Path'])
            if digest(path).lower() != row['Sha256'].lower():
                parser.error('Stale source compiler input: ' + str(path))
            files.add(path)
        flags += ['-I' + str(args.public_directory.resolve() / 'include')]
        links = [row['Path'] for row in public['Objects'] if Path(row['Path']).name != 'xlir_public_source_driver.o']
        links += [str(product), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
    else:
        flags += ['-D_GNU_SOURCE']
        links = ['-Lrelease', '-lxlc', '-Wl,-rpath,' + str(root / 'release'), '-pthread', '-ldl', '-lm']
        files.add(root / 'release/libxl.so')
        files.update(path for path in (root / 'src').rglob('*') if path.suffix in ('.c', '.h', '.inc'))
    inputs = [{'Path': str(path), 'Sha256': digest(path)} for path in sorted(files)]
    rows, results, errors = [], [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Inputs': inputs, 'Rows': rows, 'Results': results,
              'Scope': __doc__, 'LanguageExecution': False, 'Sanitized': False,
              'SemanticOptimizations': [], 'DirectFieldPartitions': 13, 'ActualSemanticCases': 3}
    out.mkdir(parents=True)
    if os.name == 'nt': shutil.copy2(product, out / 'xl.dll')
    def run(name: str, command: list[str]) -> str:
        proc = subprocess.run(command, cwd=root, capture_output=True, timeout=360)
        data = proc.stdout + proc.stderr
        (out / (name + '.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': proc.returncode, 'Complete': proc.returncode == 0})
        print(name + ': ' + str(proc.returncode), flush=True)
        if proc.returncode: raise RuntimeError(name + ': ' + data[-5000:].decode(errors='replace'))
        return data.decode(errors='replace')
    try:
        for opt in ('O0', 'O2'):
            host = out / ('contract.' + opt + ('.exe' if os.name == 'nt' else ''))
            host_ready = False
            try:
                run('build.' + opt, [cc, *flags, '-' + opt, str(source), *links, '-o', str(host)])
                host_ready = True
            except Exception as error:
                errors.append(str(error))
            bridge_host = out / ('bridge.' + opt + ('.exe' if os.name == 'nt' else ''))
            try:
                run('bridge.build.' + opt, [cc, *flags, '-' + opt, str(bridge), *links, '-o', str(bridge_host)])
                assert 'DynamicFields effect contracts verified' in run('bridge.' + opt, [str(bridge_host)])
            except Exception as error:
                errors.append(str(error))
            semantic_host = out / ('field.' + opt + ('.exe' if os.name == 'nt' else ''))
            try:
                run('field.build.' + opt, [cc, *flags, '-' + opt, str(semantic), *links, '-o', str(semantic_host)])
                log = run('field.' + opt, [str(semantic_host), str(cases[0][1]), *(str(path) for path in refusals)])
                assert 'Field inclusion: 13 direct partitions and 3 actual semantic programs accepted/refused' in log
                report['SemanticOptimizations'].append(opt)
            except Exception as error:
                errors.append(str(error))
            if not host_ready:
                continue
            for name, fixture in cases:
                target = out / (name + '.' + opt + '.json')
                try:
                    log = run(name + '.' + opt, [str(host), str(fixture), str(target), 'empty' if name in ('defaults', 'readers') else name])
                    match = re.search(r'Projection plans: (\d+) plans, (\d+) schemas, (\d+) fields; (\d+) mutations refused before C/header', log)
                    assert match
                    ir = json.loads(target.read_text(encoding='utf-8-sig'))
                    count = [int(match[i]) for i in range(1, 5)]
                    assert count[:3] == [len(ir['persistenceProjectPlans']), len(ir['persistenceProjectSchemas']), len(ir['persistenceProjectFields'])]
                    if name in ('empty', 'defaults', 'readers'): assert count[:3] == [0, 0, 0]
                    else: assert count[0] == 2 and count[3] >= 10
                    if name == 'schema': assert count[1] >= 6 and count[2] >= 12
                    if name == 'sequences': assert len(ir['persistenceProjectSequences']) == 6
                    assert 'persistenceRestorePlans' in ir and 'persistenceRestoreFields' in ir
                    if name in ('defaults', 'restore'):
                        assert len(ir['persistenceRestorePlans']) == 2 and ir['persistenceRestoreFields']
                    if name == 'restore':
                        assert len(ir['persistenceRestoreSchemas']) == 6
                        assert len(ir['persistenceRestoreFields']) == 14
                    if name == 'restore_domains':
                        assert len(ir['persistenceRestorePlans']) == 2
                        assert len(ir['persistenceRestoreSchemas']) == 6 and len(ir['persistenceRestoreFields']) == 40
                        assert any(row['persistenceRestoreCarrierAbi'] == 2 for row in ir['types'])
                        assert any(row['acceptNull'] for row in ir['persistenceRestoreFields'])
                        # Nullable/native scalar carriers and a nested declaration
                        # constructor coexist; required rows remain in restore.
                        assert all(not row['required'] for row in ir['persistenceRestoreFields'])
                        assert sum(row['assignAbi'] == 2 for row in ir['persistenceRestoreFields']) == 2
                        types = {row['id']: row for row in ir['types']}
                        layouts = {types[row['type']]['name']: row for row in ir['classLayouts']}
                        factory = ir['functions'][layouts['DomainChild']['persistenceDefaultFunction'] - 1]
                        assert factory['kindName'] == 'class-default' and factory['reachability'] == 'transitive'
                        assert factory['parameterCount'] == 0 and factory['errorAbi'] == 'may-raise'
                        marker = ir['functions'][layouts['DomainMarker']['persistenceDefaultFunction'] - 1]
                        assert marker['kindName'] == 'class-default' and marker['reachability'] == 'unreachable'
                    results.append({'Case': name, 'Optimization': opt, 'Plans': count[0], 'Schemas': count[1],
                                    'Fields': count[2], 'Sequences': len(ir['persistenceProjectSequences']),
                                    'RejectedMutations': count[3], 'IrSha256': digest(target)})
                except Exception as error:
                    errors.append(name + '.' + opt + ': ' + str(error))
            for fixture in refusals:
                try:
                    log = run(fixture.stem + '.' + opt, [str(host), str(fixture), str(out / 'unused.json'), 'reject'])
                    assert 'Included non-persistent field refused by semantic checking' in log
                except Exception as error:
                    errors.append(str(error))
        for name, _ in cases:
            try:
                assert digest(out / (name + '.O0.json')) == digest(out / (name + '.O2.json'))
            except Exception as error:
                errors.append(name + ' deterministic IR: ' + str(error))
        report['InputsUnchanged'] = all(digest(Path(row['Path'])) == row['Sha256'] for row in inputs)
        report['Complete'] = not errors and report['InputsUnchanged'] and all(row['Complete'] for row in rows)
    except Exception as error:
        errors.append(str(error))
    finally:
        if errors:
            report['Errors'] = errors
            print('\n'.join(errors), file=sys.stderr)
        (out / 'results.json').write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
    return 0 if report['Complete'] else 1

if __name__ == '__main__':
    raise SystemExit(main())
