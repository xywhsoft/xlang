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
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--public-directory', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    cc = shutil.which('gcc')
    if not cc or out.exists() or (os.name == 'nt' and not args.public_directory):
        parser.error('Require gcc, a fresh evidence directory, and a current-source Windows compiler')
    source = root / 'test/cases/xlir_persistence_project_plan_contract.c'
    semantic = root / 'test/cases/xlir_persistence_field_inclusion_contract.c'
    cases = [('schema', root / 'test/cases/xlir_persistence_project_plan.xl'),
             ('sequences', root / 'test/cases/xlir_persistence_project_sequences.xl'),
             ('roots', root / 'test/cases/stdlib_array_persistence_language.xl'),
             ('empty', root / 'test/cases/stdlib_dynamic_numeric_prune.xl'),
             ('defaults', root / 'test/cases/cgen_string_persistence_default_nul.xl')]
    cases.append(('readers', root / 'test/cases/stdlib_serialization_read_errors.xl'))
    cases.append(('configured', root / 'test/cases/stdlib_persistence_configured_language.xl'))
    refusals = [root / ('test/cases/' + name + '.xl') for name in
                ('xlir_persistence_project_included_reject', 'xlir_persistence_project_format_skip_reject')]
    files = {Path(__file__).resolve(), Path(sys.executable), Path(cc), source, semantic,
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
