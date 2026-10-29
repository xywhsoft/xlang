"""Actual field-store transaction vectors against both embedded products.
This is a core prerequisite gate, NOT language clone/Cgen/full53 acceptance.
Optional XRT source regressions must use the explicit supplied source root.
"""
from __future__ import annotations
import argparse, hashlib, json, os, re, shutil, subprocess, sys
from pathlib import Path
from sync_xrt_core_sections import section as snapshot_section

SUITE = ('value_container,value_collection,value_graph,memory_debug,ownership_scope_tests,'
         'value_lifetime_copy_tests,value_finalizer_publication_tests,'
         'value_finalizer_lifetime_tests,value_iterator_ownership_tests,'
         'ownership_snapshot_tests,value_object_discovery_tests')

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--xrt-root', type=Path)
    parser.add_argument('--previous-root', type=Path, help='Verify every unselected embedded-header byte against the prior snapshot')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists() and (not args.sanitize or sys.platform == 'linux')
    cc = shutil.which('gcc'); assert cc
    source = root/'test/cases/xrt_restore_fields_host.c'
    inputs = {Path(__file__).resolve(), Path(cc), source, root/'src/api/xl_xrt_symbols.inc'}
    for product in ('xrt', 'xruntime'):
        inputs.update(root/name for name in (f'lib/{product}/{product}.h',
                      f'lib/{product}/{product}.snapshot.json', f'release/include/{product}_decl.h'))
    native = args.xrt_root.resolve() if args.xrt_root else None
    previous = args.previous_root.resolve() if args.previous_root else None
    preserved = None
    if previous:
        for product in ('xrt','xruntime'):
            for declaration in (False,True):
                name = f'release/include/{product}_decl.h' if declaration else f'lib/{product}/{product}.h'
                prior = previous/name; inputs.add(prior)
                def unselected(path):
                    text = path.read_text(encoding='utf-8')
                    spans = [snapshot_section(text,'public','include/xrt/value.h')]
                    if not declaration: spans.append(snapshot_section(text,'source','src/value/value_container.c'))
                    for start,end in sorted(spans,reverse=True): text = text[:start]+'OWNED-SECTION\n'+text[end:]
                    return text
                assert unselected(prior) == unselected(root/name), ('unselected source changed',name)
        preserved = True
    if native:
        original = native/'tests/value/test_value_object_fields.c'
        inputs.add(native/'LICENSE')
        assert source.read_text(encoding='utf-8').split('static unsigned Finalized',1)[1].rstrip('\n') == \
               original.read_text(encoding='utf-8').split('static unsigned Finalized',1)[1].rstrip('\n'), 'native vectors differ'
        for folder, extensions in (('include', {'.h'}), ('src', {'.h','.c'}),
                                   ('tests', {'.h','.c'}), ('tools', {'.py'}), ('config', {'.json'})):
            inputs.update(path for path in (native/folder).rglob('*') if path.is_file() and path.suffix in extensions)
        inputs.update(native/'single'/name for name in ('xrt.h', 'xrt_decl.h'))
    before = {str(path): sha(path) for path in sorted(inputs)}
    for product in ('xrt', 'xruntime'):
        metadata = json.loads((root/f'lib/{product}/{product}.snapshot.json').read_text(encoding='utf-8'))
        for section, path in (('implementation',root/f'lib/{product}/{product}.h'),
                              ('declarations',root/f'release/include/{product}_decl.h')):
            assert metadata[section]['sha256'].lower() == sha(path) and metadata[section]['bytes'] == path.stat().st_size
    out.mkdir(parents=True); rows, prefixes, errors = [], [], []
    def run(name, command, cwd=root):
        process = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        log = out/(name+'.log'); log.write_bytes(process.stdout)
        rows.append({'Name':name,'Command':command,'Cwd':str(cwd),'Exit':process.returncode,
                     'Log':str(log),'Sha256':sha(log)})
        print(name+': '+str(process.returncode), flush=True)
        if process.returncode: errors.append(name)
        return process
    modes = ['O0','O2'] + (['sanitized'] if args.sanitize else [])
    for product in ('xrt','xruntime'):
        for declaration_first in (False,True):
            for mode in modes:
                name = f'{product}.decl-{int(declaration_first)}.{mode}'
                executable = out/(name+('.exe' if os.name=='nt' else ''))
                command = [cc,'-std=c11','-Wall','-Wextra','-Werror',
                           '-DXRT_MODULE_VALUE_CONTAINER','-DXRT_MODULE_MEMORY_DEBUG']
                command += (['-DXRUNTIME_MODULE_RUNTIME_TYPE','-DXRUNTIME_IMPLEMENTATION','-DXRT_IMPLEMENTATION',
                             '-include',str(root/'lib/xruntime/xruntime_features.h'),'-include',str(root/'lib/xrt/xrt.h')]
                            if product=='xruntime' else ['-DXRT_IMPLEMENTATION'])
                command += (['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
                            if mode=='sanitized' else ['-'+mode])
                if declaration_first: command += ['-include',str(root/f'release/include/{product}_decl.h')]
                command += ['-include',str(root/f'lib/{product}/{product}.h'),str(source)]
                if os.name!='nt': command += ['-pthread','-lm','-ldl']
                command += ['-o',str(executable)]
                if run(name+'.compile',command).returncode: continue
                execution = run(name+'.run',[str(executable)])
                text = execution.stdout.decode('utf-8',errors='replace')
                closed = re.findall(r'Object fields mode=(\d+) first-no-hit=(\d+) closed-prefix',text)
                count = re.findall(r'Object fields: (\d+) real allocation-failure prefixes balanced',text)
                if [mode for mode,_ in closed] != ['0','1','2','3'] or len(count)!=1 or \
                   sum(int(point)+1 for _,point in closed) != int(count[0]):
                    errors.append(name+'.prefix-closure')
                else: prefixes.append({'Name':name,'Prefixes':int(count[0]),'ClosedModes':closed})
    regression_cases = 0
    if native:
        run('xrt.generated',[sys.executable,str(native/'tools/amalgamate.py'),'--check'],native)
        regression = run('xrt.modular',[sys.executable,str(native/'tools/build.py'),'--suite',SUITE,
                         '--no-single','--no-examples','--jobs','4','--cflag=-Wall','--cflag=-Wextra',
                         '--cflag=-Werror','--cflag=-O2'],native)
        regression_cases = len(re.findall(rb'(?m)^\[test\] ',regression.stdout))
        if regression_cases < 20 or b'[pass] compiler=' not in regression.stdout: errors.append('xrt.modular.coverage')
    unchanged = all(Path(path).is_file() and sha(Path(path))==digest for path,digest in before.items())
    expected = 2*2*len(modes)*2 + (2 if native else 0)
    artifacts = [{'Path':str(path),'Sha256':sha(path)} for path in sorted(out.iterdir()) if path.is_file()]
    if native:
        artifacts += [{'Path':str(path),'Sha256':sha(path)} for path in sorted((native/'out').rglob('*')) if path.is_file()]
    result = {'Complete':unchanged and len(rows)==expected and not errors,
              'InputsUnchanged':unchanged,'Inputs':[{'Path':path,'Sha256':digest} for path,digest in before.items()],
              'Rows':rows,'Errors':errors,'Artifacts':artifacts,'Products':artifacts,'FaultRuns':prefixes,
              'AllocationFailurePrefixes':sum(row['Prefixes'] for row in prefixes),'ModularRegressionCases':regression_cases,
              'Scope':__doc__,'Sanitized':args.sanitize,'LanguageCloneAcceptance':False,
              'UnselectedSnapshotSectionsUnchanged':preserved,
              'ShippingLibraryAcceptance':False,'Full53Acceptance':False}
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({key:result[key] for key in ('Complete','InputsUnchanged','Errors','AllocationFailurePrefixes','ModularRegressionCases')}),flush=True)
    return 0 if result['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
