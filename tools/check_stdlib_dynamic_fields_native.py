"""Frozen-source DynamicFields physical ownership and adjacent xruntime gates.
No xlang carrier admission, Open restoration, shipping product or full53 claim.
All selected modular and single-TU implementations are rebuilt, not linked to
an old runtime. Linux sanitizer instruments the complete selected closure.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess, sys, time

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--xrt-root',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args(); source=args.xrt_root.resolve(); out=args.out.resolve()
    cc=shutil.which('gcc'); assert cc and not out.exists()
    assert not args.sanitize or sys.platform=='linux'
    out.mkdir(parents=True); root=out/'frozen/xrt'; root.mkdir(parents=True)
    for folder in ('include','src','tests','config','tools','extlibs/xruntime'):
        shutil.copytree(source/folder,root/folder,ignore=shutil.ignore_patterns('__pycache__','out'))
    shutil.copy2(source/'LICENSE',root/'LICENSE')
    # Generation is mechanical and precedes input pinning. Each build's own
    # repeated generation must remain byte-identical to these pinned headers.
    subprocess.run([sys.executable,'tools/amalgamate.py'],cwd=root,check=True,capture_output=True)
    subprocess.run([sys.executable,'tools/amalgamate.py','--manifest','extlibs/xruntime/config/modules.json'],
        cwd=root,check=True,capture_output=True)
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc)}
    files.update(path for path in root.rglob('*') if path.is_file() and '__pycache__' not in path.parts)
    for name in ('cc1','as','ld'):
        selected=subprocess.check_output([cc,'-print-prog-name='+name],text=True).strip()
        files.add((Path(selected) if Path(selected).is_absolute() else Path(shutil.which(selected))).resolve())
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    report={'Scope':__doc__,'Complete':False,'InputsUnchanged':False,'Inputs':inputs,'Rows':[],
        'Failures':[],'Products':[],'Platform':sys.platform,'Sanitized':args.sanitize,
        'SelectedNativeClosureInstrumented':args.sanitize,'WholeRuntimeInstrumented':False,
        'SourceLanguageDynamicFieldsCutover':False,'OpenRestoreCutover':False,
        'ShippingLibraryAcceptance':False,'Full53Acceptance':False}
    target=out/'results.json'
    def save(): target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    env=dict(os.environ)
    if args.sanitize: env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
        UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name,command,expected=0):
        start=time.monotonic()
        result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=900)
        log=out/(name+'.log'); log.write_bytes(result.stdout+result.stderr)
        ok=result.returncode==expected
        report['Rows'].append({'Name':name,'Command':list(map(str,command)),
            'Exit':result.returncode,'ExpectedExit':expected,'Seconds':time.monotonic()-start,'Complete':ok,
            'Log':str(log),'LogSha256':sha(log)})
        if not ok: report['Failures'].append(name)
        save(); assert ok,(name,(result.stdout+result.stderr)[-2400:].decode(errors='replace'))
    base=[sys.executable,'tools/build.py','--manifest','extlibs/xruntime/config/modules.json','--no-examples','--rebuild']
    suite='runtime_dynamic_field,runtime_dynamic_field_oom_tests,typed_dict,runtime_object,runtime_object_graph,runtime_value_trace'
    for optimization in ('O0','O2'):
        flags=['--cflag=-'+optimization]
        if args.sanitize: flags+=['--cflag=-g','--cflag=-fsanitize=address,undefined',
            '--cflag=-fno-omit-frame-pointer','--cflag=-fno-pie','--cflag=-no-pie']
        if os.name!='nt': flags+=['--ldflag=-pthread']
        run('bare-object-'+optimization,base+['--suite','runtime_object','--no-single']+flags)
        run('physical-and-adjacent-'+optimization,base+['--suite',suite,'--no-single']+flags)
        run('physical-single-'+optimization,base+['--suite','runtime_dynamic_field,memory_debug','--single-test',
            'extlibs/xruntime/tests/single/test_single_runtime_dynamic_field_ownership.c']+flags)
    # Negative controls mutate the actual newly accepted single implementation,
    # not an oracle stub. Each must compile and then fail its real contract.
    header=root/'single/extlibs/xruntime.h'; original=header.read_text(encoding='utf-8')
    controls={
        'omit-slots':('static bool __xrtDynamicFieldOwnershipTrace',r'\tif \(pVisit == NULL.*?\n\}',
            '\t(void)pData; (void)pContext; return pVisit != NULL;\n}'),
        'wrong-count':(None,'*pCount = xrtObjectRefCount(pData);','*pCount = xrtObjectRefCount(pData) + 1u;'),
        'weak-claim-escape':(None,'if (pObject->OwnershipClaim != NULL || pObject->OwnershipCleared) return NULL;',
            'if (pObject->OwnershipCleared) return NULL;'),
        'cleared-retain':(None,'pObject->OwnershipCleared = true;',
            'pObject->OwnershipCleared = false; /* invalid retirement: negative control only */'),
        'foreign-storage-policy':(None,
            'pDict->Storage.Drop == __xrtTypedDictDrop &&\n\t\tpDict->Storage.DropUserData == (ptr)pType &&',
            'true && /* invalid arbitrary payload destructor admission */'),
    }
    # Never mutate a pinned input, even temporarily. Build mutants in a sibling
    # private root; its tests and includes are exact copies of accepted sources.
    mutant=out/'negative/xrt'; shutil.copytree(root,mutant,ignore=shutil.ignore_patterns('out','__pycache__'))
    for name,(anchor,old,new) in controls.items():
        if anchor:
            start=original.index(anchor); head=original[:start]; tail=original[start:]
            changed,count=re.subn(old,new,tail,count=1,flags=re.S); assert count==1
            text=head+changed
        else:
            assert original.count(old)==1,(name,original.count(old)); text=original.replace(old,new,1)
        (mutant/'single/extlibs/xruntime.h').write_text(text,encoding='utf-8')
        product=out/(name+('.exe' if os.name=='nt' else ''))
        command=[cc,'-std=c11','-Wall','-Wextra','-Werror','-O0','-DXRT_MODULE_MEMORY_DEBUG',
            '-DXRUNTIME_MODULE_RUNTIME_DYNAMIC_FIELD','-I'+str(mutant/'include'),
            '-I'+str(mutant/'extlibs/xruntime/include'),
            str(mutant/'extlibs/xruntime/tests/single/test_single_runtime_dynamic_field_ownership.c'),'-o',str(product)]
        if os.name!='nt': command+=['-pthread','-lm']
        run('negative-'+name+'-compile',command)
        run('negative-'+name+'-run',[product],1)
    assert all(sha(row['Path'])==row['Sha256'] for row in inputs),'frozen input drift'
    report['InputsUnchanged']=True
    report['Products']=[{'Path':str(path),'Sha256':sha(path)} for path in sorted((root/'out').rglob('*'))
        if path.is_file() and path.suffix in ('.exe','.o','')]
    report['Complete']=True; save()
    print('DynamicFields native: '+str(len(report['Rows']))+' phases, exact ownership/claims/cleanup/OOM/freeze and '+str(len(controls))+' real implementation negative controls passed',flush=True)

if __name__=='__main__': main()
