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
import traceback

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
    parser.add_argument('--pending-carriers-only', action='store_true',
                        help='Supplemental actual-source List/Set/Dict pending-ABI test, not the full matrix')
    parser.add_argument('--cases', help='Comma-separated declared fixture subset; explicitly recorded as focused validation')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--recheck-evidence', type=Path,
                        help='Revalidate recorded successful commands/logs/IR after a checker-only fix; no new host execution')
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
    cases.append(('restore_construction', test_root / 'test/cases/stdlib_persistence_restore_construction.xl'))
    cases.append(('restore_graph', test_root / 'test/cases/xlir_persistence_restore_graph.xl'))
    cases.append(('restore_isolation', test_root / 'test/cases/xlir_persistence_restore_isolation.xl'))
    if args.pending_carriers_only:
        cases = [('pending_carriers', test_root / 'test/cases/xlir_persistence_restore_pending_carriers.xl')]
    if args.cases:
        assert not args.pending_carriers_only
        selected=args.cases.split(',')
        assert len(selected)==len(set(selected)) and set(selected)<={name for name,_ in cases}
        cases=[row for row in cases if row[0] in selected]
    refusals = [test_root / ('test/cases/' + name + '.xl') for name in
                ('xlir_persistence_project_included_reject', 'xlir_persistence_project_format_skip_reject')]
    files = {Path(__file__).resolve(), Path(sys.executable), Path(cc), source, semantic,
             bridge, test_root / 'test/cases/xlir_dynamic_fields_contract.inc',
             *(path for _, path in cases), *refusals}
    product = root / ('release/xl.dll' if os.name == 'nt' else 'release/libxlc.so')
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
             '-I.', '-Irelease', '-Irelease/include', '-Ilib/tcc']
    if args.public_directory:
        manifest = args.public_directory.resolve() / 'build.json'
        public = json.loads(manifest.read_text(encoding='utf-8-sig'))
        if os.name != 'nt':
            assert public['Complete'] and public['InputsUnchanged'] and public['SourceXrt']
            product = Path(public['NativeRoot']) / 'release/libxl.so'
        files.add(manifest)
        for row in public['Inputs'] + public['Headers'] + public['Objects']:
            path = Path(row['Path'])
            if digest(path).lower() != row['Sha256'].lower():
                parser.error('Stale source compiler input: ' + str(path))
            files.add(path)
        flags += ['-I' + str(args.public_directory.resolve() / 'include')]
        links = [row['Path'] for row in public['Objects'] if Path(row['Path']).name not in
                 ('xlir_public_source_driver.o', 'xlir_public_source_linux_driver.o')]
        if os.name == 'nt':
            links += [str(product), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
        else:
            flags += ['-D_GNU_SOURCE']
            links += [str(product), '-Wl,-rpath,' + str(product.parent), '-pthread', '-ldl', '-lm']
    else:
        flags += ['-D_GNU_SOURCE']
        links = ['-Lrelease', '-lxlc', '-Wl,-rpath,' + str(root / 'release'), '-pthread', '-ldl', '-lm']
        files.add(root / 'release/libxl.so')
        files.update(path for path in (root / 'src').rglob('*') if path.suffix in ('.c', '.h', '.inc'))
    files.add(product)
    recorded = None
    recorded_rows = {}
    evidence = args.recheck_evidence.resolve() if args.recheck_evidence else None
    if evidence:
        previous_path = evidence / 'results.json'
        recorded = json.loads(previous_path.read_text(encoding='utf-8-sig'))
        assert recorded['InputsUnchanged'] and recorded['FullFixtureMatrix'] == (not args.pending_carriers_only)
        assert recorded['SelectedCases'] == [name for name, _ in cases]
        # A reporting failure is not an execution failure. Refuse reuse of ANY
        # unsuccessful command or changed input; retain the old report as-is.
        assert recorded['Rows'] and all(r['Exit'] == 0 and r['Complete'] for r in recorded['Rows'])
        for row in recorded['Inputs']:
            path = Path(row['Path'])
            assert digest(path) == row['Sha256'], path
            files.add(path)
        recorded_rows = {row['Name']: row for row in recorded['Rows']}
        assert len(recorded_rows) == len(recorded['Rows'])
        recorded_root = Path(recorded_rows['schema.O0' if not args.pending_carriers_only else 'pending_carriers.O0']['Command'][1]).parents[2]
        # Every source/test input in this run must be byte-identical to the
        # executed input, except this explicitly changed processing script.
        for path in list(files):
            if path == Path(__file__).resolve(): continue
            if path.is_relative_to(test_root):
                prior = recorded_root / path.relative_to(test_root)
                assert prior.is_file() and digest(path) == digest(prior), path
        files.add(previous_path)
        files.update(evidence / (name + '.log') for name in recorded_rows)
        files.update(evidence / (name + '.' + opt + '.json') for name, _ in cases for opt in ('O0', 'O2'))
    inputs = [{'Path': str(path), 'Sha256': digest(path)} for path in sorted(files)]
    rows, results, errors = [], [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Inputs': inputs, 'Rows': rows, 'Results': results,
              'Scope': __doc__, 'LanguageExecution': False, 'Sanitized': False,
              'SemanticOptimizations': [], 'DirectFieldPartitions': 13, 'ActualSemanticCases': 3}
    report['RecheckedRecordedExecution'] = bool(evidence)
    report['PriorProcessingErrors'] = recorded.get('Errors', []) if recorded else []
    report['RecordedExecutionReport'] = str(evidence / 'results.json') if evidence else None
    report['NewHostExecution'] = not bool(evidence)
    report['SelectedCases'] = [name for name, _ in cases]
    report['FullFixtureMatrix'] = not args.pending_carriers_only and not args.cases
    out.mkdir(parents=True)
    if os.name == 'nt': shutil.copy2(product, out / 'xl.dll')
    def run(name: str, command: list[str]) -> str:
        if evidence:
            prior = recorded_rows[name]
            expected = [part.replace(str(out), str(evidence)).replace(str(test_root), str(recorded_root)) for part in command]
            assert expected == prior['Command'], name + ': changed recorded command'
            data = (evidence / (name + '.log')).read_bytes()
            (out / (name + '.log')).write_bytes(data)
            rows.append(dict(prior, EvidenceReused=True))
            print(name + ': rechecked recorded success', flush=True)
            return data.decode(errors='replace')
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
                    if evidence: shutil.copy2(evidence / target.name, target)
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
                    graph = re.search(r'Restore graphs: (\d+) roots, (\d+) nodes, (\d+) edges; (\d+) graph mutations refused before C/header', log)
                    assert graph
                    roots, nodes, edges = [ir['persistenceRestore' + table] for table in ('Roots', 'Nodes', 'Edges')]
                    assert [int(graph[i]) for i in (1, 2, 3)] == [len(roots), len(nodes), len(edges)]
                    assert int(graph[4]) >= 3
                    assert len({(row['type'], row['format']) for row in roots}) == len(roots)
                    next_node = next_edge = 0
                    for row in roots:
                        assert row['firstNode'] == next_node and row['firstEdge'] == next_edge
                        assert row['root'] == 0 and row['nodeCount'] > 0 and row['format'] in (2, 4)
                        ns = nodes[next_node:next_node + row['nodeCount']]
                        es = edges[next_edge:next_edge + row['edgeCount']]
                        assert len(ns) == row['nodeCount'] and len(es) == row['edgeCount']
                        assert ns[0]['type'] == row['type']
                        assert len({(n['type'], n['contextField']) for n in ns}) == len(ns)
                        assert sorted(n['seed'] for n in ns if n['abi'] == 2 and n['carrierAbi'] == 2) == list(range(row['seedCount']))
                        assert all(e['field'] < len(ir['persistenceRestoreFields']) and e['node'] < len(ns) for e in es)
                        for n in ns:
                            assert bool(n['flags'] & 2) == (n['contextField'] < len(ir['classFields']))
                            if n['flags'] & 2: assert n['abi'] == 3 and not n['flags'] & 1 and not n['defaultFunction']
                            if n['abi'] in (2, 4, 5, 6): assert n['element'] < len(ns)
                            if n['abi'] == 3:
                                assert n['schema'] < len(ir['persistenceRestoreSchemas'])
                                assert n['firstEdge'] + n['edgeCount'] <= len(es)
                                layout = ir['classLayouts'][ir['persistenceRestoreSchemas'][n['schema']]['classLayout']]
                                # Isolation is a distinct checked contract, not
                                # a default factory or noncopyable-null result.
                                assert n['isolationAbi'] == {0: 1, 1: 1, 2: 2, 3: 3, 4: 4}[layout['cloneAbi']]
                                if n['isolationAbi'] == 4:
                                    assert n['cloneSymbol'] == layout['cloneTarget']
                                    clone = ir['functions'][n['cloneFunction'] - 1]
                                    clone_target = next(t for t in ir['callTargets'] if t['symbol'] == n['cloneSymbol'])
                                    assert clone['symbol'] == clone_target['symbol'] and clone['returnType'] == n['payloadType']
                                    assert clone['parameterCount'] == clone_target['parameterCount'] == 1
                                    assert clone['errorAbi'] == {1: 'no-raise', 2: 'may-raise'}[clone_target['errorAbi']]
                                    assert n['cloneErrorAbi'] == clone_target['errorAbi']
                                    assert clone['reachability'] != 'unreachable' and clone_target['reachability'] != 'unreachable'
                                else:
                                    assert not n['cloneFunction'] and not n['cloneSymbol']
                                    assert n['cloneErrorAbi'] == 2  # MAY_RAISE, including checked rejection
                                if n['defaultFunction']:
                                    factory = ir['functions'][n['defaultFunction'] - 1]
                                    assert factory['kindName'] == 'class-default' and factory['returnType'] == n['payloadType']
                                for edge in es[n['firstEdge']:n['firstEdge'] + n['edgeCount']]:
                                    field = ir['persistenceRestoreFields'][edge['field']]
                                    child = ns[edge['node']]
                                    assert child['type'] == field['payloadType']
                                    if field['assignAbi'] == 2:
                                        assert child['contextField'] == field['classField'] and child['flags'] & 2 and child['defaultFunction'] == 0
                                    else: assert not child['flags'] & 2
                            if n['abi'] == 4: assert row['pending'] & 4
                            if n['abi'] == 5: assert row['pending'] & 8
                            if n['abi'] == 6: assert row['pending'] & 16
                            if n['abi'] != 3:
                                assert n['isolationAbi'] == n['cloneFunction'] == n['cloneSymbol'] == 0
                                assert n['cloneErrorAbi'] == 1  # NO_RAISE
                        next_node += row['nodeCount']; next_edge += row['edgeCount']
                    assert next_node == len(nodes) and next_edge == len(edges)
                    prefixes = re.search(r'Restore rebuild: (\d+) exhaustive allocation-failure prefixes preserved all six tables', log)
                    assert prefixes and (int(prefixes[1]) > 0 or not roots)
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
                    if name == 'restore_construction':
                        assert len(ir['persistenceRestorePlans']) == 2
                        types = {row['id']: row for row in ir['types']}
                        layouts = {types[row['type']]['name']: row for row in ir['classLayouts']}
                        for class_name in ('ConstructionItem', 'ConstructionOpen', 'ConstructionKey'):
                            layout = layouts[class_name]
                            assert layout['persistenceConstructionAbiName'] == 'declaration-default'
                            factory = ir['functions'][layout['persistenceDefaultFunction'] - 1]
                            assert factory['kindName'] == 'class-default' and factory['reachability'] == 'transitive'
                        for class_name in ('ConstructionEnvelope', 'ConstructionKeys'):
                            layout = layouts[class_name]
                            factory = ir['functions'][layout['persistenceDefaultFunction'] - 1]
                            # Root declaration construction has its own actual
                            # lowered invocation; it is not an unused factory.
                            assert factory['reachability'] == 'transitive'
                            assert any(i['runtimeOpName'] == 'class-default-invoke' and
                                       i['payloadType'] == layout['type'] for i in ir['instructions'])
                        for class_name in ('ConstructionBase', 'ConstructionMiddle'):
                            factory = ir['functions'][layouts[class_name]['persistenceDefaultFunction'] - 1]
                            assert factory['reachability'] == 'unreachable'
                        assert layouts['ConstructionItem']['destroyStepCount'] == 1
                        assert all('persistenceDefaultAbi' not in field for field in ir['classFields'])
                    if name == 'restore_graph':
                        types = {row['id']: row for row in ir['types']}
                        assert len(roots) == 2 and len(ir['persistenceRestoreSchemas']) == 4
                        for graph_root in roots:
                            ns = nodes[graph_root['firstNode']:graph_root['firstNode'] + graph_root['nodeCount']]
                            leaf = [n for n in ns if types[n['payloadType']]['name'] == 'GraphRecipeLeaf']
                            assert len(leaf) == 3 and len({n['contextField'] for n in leaf}) == 3
                            assert sum(bool(n['flags'] & 1) for n in leaf) == 1
                            assert sum(bool(n['defaultFunction']) for n in leaf) == 1
                            assert graph_root['pending'] == 0 and graph_root['seedCount'] == 2 and graph_root['edgeCount'] > 64
                            assert all(types[n['type']]['name'] != 'GraphRecipeUnused' for n in ns)
                    if name == 'pending_carriers':
                        assert len(roots) == 2 and len(edges) == 6
                        assert sorted(n['abi'] for n in nodes if n['abi'] in (4, 5, 6)) == [4, 4, 5, 5, 6, 6]
                        assert all(graph_root['pending'] == (4 | 8 | 16) for graph_root in roots)
                    if name == 'restore_isolation':
                        assert len(roots) == 2 and all(row['pending'] == 0 for row in roots)
                        types = {t['id']: t for t in ir['types']}
                        named = {types[n['payloadType']]['name']: n for n in nodes if n['abi'] == 3}
                        assert named['IsolationRefused']['isolationAbi'] == 1
                        assert named['IsolationShallow']['isolationAbi'] == 2
                        assert named['IsolationDeep']['isolationAbi'] == 3
                        assert named['IsolationCustom']['isolationAbi'] == named['IsolationFallback']['isolationAbi'] == 4
                        assert named['IsolationAlias']['isolationAbi'] == 4
                        assert 'IsolationUnused' not in named
                        unused_layout = next(l for l in ir['classLayouts'] if types[l['type']]['name'] == 'IsolationUnused')
                        unused_clone = next(f for f in ir['functions'] if f['symbol'] == unused_layout['cloneTarget'])
                        assert unused_clone['reachability'] == 'unreachable'
                    results.append({'Case': name, 'Optimization': opt, 'Plans': count[0], 'Schemas': count[1],
                                    'Fields': count[2], 'Sequences': len(ir['persistenceProjectSequences']),
                                    'RejectedMutations': count[3], 'RestoreRoots': len(roots),
                                    'RestoreNodes': len(nodes), 'RestoreEdges': len(edges),
                                    'GraphRejectedMutations': int(graph[4]), 'IrSha256': digest(target)})
                    results[-1]['RebuildAllocationFailurePrefixes'] = int(prefixes[1])
                except Exception as error:
                    errors.append(name + '.' + opt + ': ' + traceback.format_exc())
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
        if evidence: assert {row['Name'] for row in rows} == set(recorded_rows)
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
