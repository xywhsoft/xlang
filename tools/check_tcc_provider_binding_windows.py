"""Debug-heap TCC provider identity and static-production profile acceptance.

Uses an already completed source-free Windows fixture directory unchanged.
Checks actual create/relocate/delete modules and balanced relocated states,
then runs its standalone programs under the debugger. Static evidence includes
the exact target object, production response profiles and their common API path.
This is the stock fixed-adjacent-provider SourceDriver contract, not arbitrary
dynamic-provider host or custom tccDllPath certification.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','sourcefree','out'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root,public,sourcefree,out=[p.resolve()for p in (args.root,args.public_directory,args.sourcefree,args.out)]
    assert os.name=='nt'
    gdb=shutil.which('gdb');nm=shutil.which('nm');assert gdb and nm
    prior=json.loads((sourcefree/'results.json').read_text(encoding='utf-8-sig'));assert prior['complete'] and prior['passed']
    staged=json.loads((public/'provider.json').read_text(encoding='utf-8-sig'));assert staged['passed']and staged['providerImportBinding']['verified']
    out.mkdir(parents=True,exist_ok=False);sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    manifest_path=Path(staged['precompiledManifest']);manifest=json.loads(manifest_path.read_text(encoding='utf-8-sig'))
    assert '-DXL_STATIC_TCC'in manifest['flags']
    native_tcc=next(Path(row['source'])for row in manifest['objects']if row['virtual']=='__xl_objects/static-compiler/lib/tcc/libtcc.c.o')
    driver=sourcefree/'public_source.exe';provider=sourcefree/'xlc.dll'
    assert sha(provider)==staged['providerSha256']
    case_names=('stdlib_tcc_embed','double-nested-compiler','foreign_output_record_size')
    pins=[Path(__file__).resolve(),Path(gdb),Path(nm),sourcefree/'results.json',public/'provider.json',manifest_path,
        native_tcc,driver,provider,root/'src/api/xl_api.c',root/'src/runtime/xl6_tcc_context_policy.c',
        root/'tools/runtime_dll.rsp.in',root/'tools/compiler_dll.rsp.in',root/'tools/precompile_builtin.py']
    pins.extend(sourcefree/(name+'.xl')for name in case_names)
    pins.append(sourcefree/'foreign_output_record_size_native.c')
    pins.extend(sourcefree/'relocated'/(name+'-exe.exe')for name in case_names)
    inputs={str(p):sha(p)for p in pins}
    report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[],states=[],profiles=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,cwd,env):
        started=time.monotonic();r=subprocess.run(list(map(str,command)),cwd=cwd,env=env,capture_output=True,timeout=360)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=list(map(str,command)),exitCode=r.returncode,
            seconds=round(time.monotonic()-started,3),logSha256=sha(log)));save()
        print(name,r.returncode,flush=True);assert r.returncode==0,log
        return r.stdout.decode(errors='replace')
    static=run('static-tcc-object',[nm,'-g','--defined-only',native_tcc],root,os.environ)
    for function in ('tcc_new','tcc_relocate','tcc_delete'):
        assert re.search(r'\b'+function+r'\s*$',static,re.M),function
    for kind in ('runtime','compiler'):
        path=root/'tools'/(kind+'_dll.rsp.in');text=path.read_text(encoding='utf-8-sig')
        assert '-DXL_STATIC_TCC'in text and 'xl6_tcc_context_policy.c'in text and 'libtcc.c'in text
        report['profiles'].append(dict(kind=kind,path=str(path),sha256=sha(path),staticTcc=True,
            policyAndTccInSameImage=True,customTccDllPathSelectsAnotherProvider=False))
    api=(root/'src/api/xl_api.c').read_text(encoding='utf-8-sig')
    assert re.search(r'static int xlEnsureTcc\(XL_Context\* ctx\)\s*\{\s*return xlLoadStaticTcc\(ctx\);\s*\}',api)
    # Debugger defaults to the Windows debug heap. Explicitly remove its opt-out.
    clean={k:v for k,v in os.environ.items()if not k.upper().startswith('XL6_')and k.upper()!='_NO_DEBUG_HEAP'}
    work=sourcefree/'empty-work';env=dict(clean,XL6_SOURCE_NATIVE_ROOT=str(work),XL6_TCC_DLL=str(provider),
        XL6_NESTED_SDK_WORK=str(work),TEMP=str(out),TMP=str(out))
    commands=out/'provider.gdb'
    commands.write_text('set pagination off\nset breakpoint pending on\n'+''.join(
        'break '+symbol+'\ncommands\nsilent\nprintf "@@'+kind+' '+('%p'if kind!='CREATE'else'')+'\\n"'+(', $rcx'if kind!='CREATE'else'')+
        '\nbt 1\ncontinue\nend\n'for symbol,kind in (('tcc_new','CREATE'),('tcc_relocate','RELOCATE'),('tcc_delete','DELETE')))+'run\n',encoding='utf-8')
    for name in case_names:
        for mode in ('run','module'):
            sidecar=[sourcefree/'foreign_output_record_size_native.c']if name=='foreign_output_record_size'else[]
            text=run(name+'-'+mode,[gdb,'-batch','-x',commands,'--args',driver,sourcefree/(name+'.xl'),mode,out/(name+'-'+mode),*sidecar],work,env)
            assert 'received signal'not in text and 'exited normally'in text,text[-4000:]
            assert 'public source '+mode+' complete'in text
            events=re.findall(r'@@(CREATE|RELOCATE|DELETE)\s*(0x[0-9a-fA-F]+)?\s*\n\s*(#0[^\n]+)',text)
            assert events and any(kind=='CREATE'for kind,_,_ in events)
            active={}
            for kind,state,frame in events:
                assert str(provider).lower()in frame.lower(),frame
                if kind=='RELOCATE':active[state]=active.get(state,0)+1
                if kind=='DELETE'and active.get(state,0):active[state]-=1
            assert any(kind=='RELOCATE'for kind,_,_ in events)and not any(active.values()),active
            report['states'].append(dict(case=name,mode=mode,events=len(events),sameProvider=True,allRelocationsDeleted=True))
        program=sourcefree/'relocated'/(name+'-exe.exe')
        text=run(name+'-static',[gdb,'-batch','-ex','set pagination off','-ex','run','--args',program],program.parent,
            dict(clean,TEMP=str(out),TMP=str(out)))
        assert 'received signal'not in text and 'exited normally'in text,text[-4000:]
    report.update(complete=True,inputsUnchanged=all(sha(p)==v for p,v in inputs.items()))
    report['passed']=report['inputsUnchanged'];save();assert report['passed']

if __name__=='__main__':main()
