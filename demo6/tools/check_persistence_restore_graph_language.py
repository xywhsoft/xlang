"""Current-source restore graph lowering and existing language-path regression.

Checks actual public compiler output, deterministic C, O0/O2 generated-C
execution with a balanced allocation ledger, and genuine TCC JIT. Optional
closed/Open-native cutover and complete language OOM prefixes are explicit flags.
Pending Set/List/Dict families remain legacy; no shipping/AOT claim.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess, sys

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--public-directory', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--test-root', type=Path)
    parser.add_argument('--native-cutover', action='store_true')
    parser.add_argument('--oom', action='store_true')
    args = parser.parse_args()
    root, public_dir, out = args.root.resolve(), args.public_directory.resolve(), args.out.resolve()
    test_root=args.test_root.resolve() if args.test_root else root
    assert not out.exists()
    manifest = public_dir / 'build.json'
    public = json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert public['SourceXrt'] and not public['RuntimeOnly'] and public['EnableExeCompiler'] == 1
    if os.name != 'nt': assert public['Complete'] and public['InputsUnchanged']
    files = {Path(__file__).resolve(), Path(sys.executable), manifest}
    for row in public['Inputs'] + public['Headers'] + public['Objects']:
        assert sha(row['Path']).lower() == row['Sha256'].lower(), row['Path']
        files.add(Path(row['Path']))
    cc = shutil.which('gcc'); assert cc
    driver = Path(public['Driver'])
    assert sha(driver).lower() == public['DriverSha256'].lower()
    product = root / 'release/xl.dll' if os.name == 'nt' else Path(public['NativeRoot']) / 'release/libxl.so'
    files.update((Path(cc), driver, product))
    cases = [('wide', test_root / 'test/cases/xlir_persistence_restore_graph.xl'),
             ('construction', test_root / 'test/cases/stdlib_persistence_restore_construction.xl'),
             # Empty wire preserves defaults on the OLD path. This exercises
             # emission/reachability/cleanup, NOT shared-default isolation.
             ('isolation_metadata', test_root / 'test/cases/xlir_persistence_restore_isolation.xl')]
    if args.native_cutover:
        cases += [('native',test_root / 'test/cases/xlir_persistence_restore_native.xl'),
                  ('native_oom',test_root / 'test/cases/xlir_persistence_restore_native_oom.xl'),
                  ('open',test_root / 'test/cases/xlir_persistence_restore_open.xl'),
                  ('open_reject',test_root / 'test/cases/xlir_persistence_restore_open_reject.xl'),
                  ('open_oom',test_root / 'test/cases/xlir_persistence_restore_open_oom.xl')]
    host = test_root / 'test/cases/xlir_module_product_oom_host.c'
    contract=test_root / 'test/cases/xlir_persistence_restore_take_contract.c'
    memory = test_root / 'test/support/module_product_memory.c'
    files.update((host, memory, test_root / 'test/support/module_product_memory.h', *(path for _, path in cases)))
    if args.native_cutover: files.add(contract)
    inputs = [{'Path': str(path), 'Sha256': sha(path)} for path in sorted(files)]
    out.mkdir(parents=True)
    (out / 'files').mkdir()
    if os.name == 'nt': shutil.copy2(product, out / 'xl.dll')
    environment = os.environ.copy()
    environment['XL6_PERSIST_FILE_ROOT']=str(out / 'files')
    if os.name != 'nt': environment['XL6_SOURCE_NATIVE_ROOT'] = public['NativeRoot']
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
             '-I' + str(root), '-I' + str(root / 'release'), '-I' + str(root / 'release/include'),
             '-I' + str(root / 'lib/tcc'), '-I' + str(public_dir / 'include'),
             '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable',
             '-Wno-unused-label', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
             '-Wno-missing-field-initializers']
    links = [row['Path'] for row in public['Objects'] if Path(row['Path']).name not in
             ('xlir_public_source_driver.o', 'xlir_public_source_linux_driver.o')]
    if os.name == 'nt':
        links += [str(product), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
    else:
        flags += ['-D_GNU_SOURCE']
        links += [str(product), '-Wl,-rpath,' + str(product.parent), '-pthread', '-ldl', '-lm']
    rows, results, errors = [], [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Inputs': inputs, 'Rows': rows,
              'Results': results, 'Scope': __doc__, 'NativePlanCgenCutover': args.native_cutover,
              'Aot': False, 'ShippingProductAcceptance': False, 'Sanitized': False,
              'LanguageOomSweep': args.oom, 'LinkedSourceCompilerOptimization': 'O0'}
    def run(name, command):
        process = subprocess.run(list(map(str, command)), cwd=root, env=environment,
                                 capture_output=True, timeout=360)
        data = process.stdout + process.stderr
        (out / (name + '.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': list(map(str, command)), 'Exit': process.returncode})
        print(name + ': ' + str(process.returncode), flush=True)
        assert process.returncode == 0, data[-6000:].decode(errors='replace')
        return data.decode(errors='replace')
    try:
        if args.native_cutover:
            for optimization in ('O0','O2'):
                binary=out / ('contract.'+optimization+('.exe' if os.name=='nt' else ''))
                run('contract.'+optimization+'.compile',[cc,*flags,'-'+optimization,contract,*links,'-o',binary])
                assert '22 corrupt ownership/plan mutations refused' in run('contract.'+optimization+'.run',
                    [binary,dict(cases)['native_oom'],dict(cases)['construction'],dict(cases)['open']])
        for name, source in cases:
            try:
                typed, lowered = out / (name + '.typed.json'), out / (name + '.lowered.json')
                run(name + '.typed', [driver, source, 'ir-typed', typed])
                run(name + '.lowered', [driver, source, 'ir-lowered', lowered])
                typed_ir = json.loads(typed.read_text(encoding='utf-8-sig'))
                ir = json.loads(lowered.read_text(encoding='utf-8-sig'))
                assert not typed_ir['persistenceRestoreRoots'] and not typed_ir['persistenceRestoreNodes']
                assert len(ir['persistenceRestoreRoots']) >= 2
                generated, repeat = out / (name + '.c'), out / (name + '.repeat.c')
                run(name + '.cgen', [driver, source, 'cgen-default', generated])
                run(name + '.repeat', [driver, source, 'cgen-default', repeat])
                assert sha(generated) == sha(repeat)
                if args.native_cutover and name!='construction':
                    text=generated.read_text(encoding='utf-8-sig')
                    assert all(not r['pending'] for r in ir['persistenceRestoreRoots'])
                    assert 'xl6PersistenceRestoreParsedTakeV1(' in text
                    assert 'xlir_persist_json_assign_' not in text and 'xlir_persist_xlon_assign_' not in text
                    assert 'xl6SerializeRestoreReadFinish' not in text
                    if name.startswith('open'):
                        opened=[n for n in ir['persistenceRestoreNodes'] if n['openClassLayout'] != 2**64-1]
                        assert opened and all(n['openCloneSymbol'] for n in opened)
                        assert ir['persistenceRestoreClaims']
                        assert 'xl6RestoreOpenProviderV1()' in text and '.open=&open_' in text
                        assert 'xlir_persist_json_assign_' not in text and 'xlir_persist_xlon_assign_' not in text
                if args.native_cutover and name=='construction':
                    roots=ir['persistenceRestoreRoots']
                    assert all(not (r['pending'] & 128) for r in roots)
                    assert all(not (r['pending'] & 1) for r in roots) and any(r['pending'] & 8 for r in roots)
                    assert any(not r['pending'] for r in roots)
                    text=generated.read_text(encoding='utf-8-sig')
                    assert 'xl6PersistenceRestoreParsedTakeV1(' in text
                    assert 'xl6SerializeRestoreReadFinishTakeV1(' in text
                for optimization in ('O0', 'O2'):
                    memory_object = out / (name + '.' + optimization + '.memory.o')
                    binary = out / (name + '.' + optimization + ('.exe' if os.name == 'nt' else ''))
                    run(name + '.' + optimization + '.memory', [cc, *flags, '-' + optimization,
                        '-c', memory, '-o', memory_object])
                    run(name + '.' + optimization + '.compile', [cc, *flags, '-' + optimization,
                        '-include', generated, host, memory_object, *links, '-o', binary])
                    assert 'balanced' in run(name + '.' + optimization + '.run', [binary])
                    if args.oom and name in ('native_oom','open_oom'):
                        message=run(name+'.'+optimization+'.prefixes',[binary,'--all'])
                        batch=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',message)
                        assert batch and int(batch[1])==int(batch[2])+1 and int(batch[2])>0
                        report.setdefault('Prefixes',[]).append({'Case':name,'Optimization':optimization,
                            'Complete':True,'Count':int(batch[1]),'FirstNoHit':int(batch[2])})
                run(name + '.jit', [driver, source, 'run', out / (name + '.jit')])
                results.append({'Case': name, 'RestoreRoots': len(ir['persistenceRestoreRoots']),
                    'RestoreNodes': len(ir['persistenceRestoreNodes']), 'RestoreEdges': len(ir['persistenceRestoreEdges']),
                    'GeneratedSha256': sha(generated), 'LoweredIrSha256': sha(lowered)})
            except Exception as error: errors.append(name + ': ' + repr(error))
        report['InputsUnchanged'] = all(sha(row['Path']) == row['Sha256'] for row in inputs)
        report['Complete'] = not errors and report['InputsUnchanged'] and len(results) == len(cases)
    finally:
        report['Errors'] = errors
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if errors: print('\n'.join(errors), file=sys.stderr)
    return 0 if report['Complete'] else 1

if __name__ == '__main__': raise SystemExit(main())
