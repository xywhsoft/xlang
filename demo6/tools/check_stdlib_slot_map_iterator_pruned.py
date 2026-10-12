"""Actual independently linked minimal SlotMap cursor lanes, not language loops.
Factory closure from the production component planner; no MODULE_ALL, compiler
or released runtime in the resulting executables. O0/O2 execution and nm prove
that values/count do not retain keyed projection/boxing/pair producers.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args(); root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    cc,nm=shutil.which('gcc'),shutil.which('nm'); assert cc and nm and not out.exists()
    build=json.loads((public/'build.json').read_text(encoding='utf-8-sig')); assert build['SourceXrt'] and not build['RuntimeOnly']
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    planner=root/'test/cases/xlir_slot_map_iterator_plan.c'; host=root/'test/cases/stdlib_slot_map_iterator_pruned_host.c'
    memory=root/'test/support/module_product_memory.c'
    files={Path(__file__).resolve(),Path(cc),Path(nm),public/'build.json',runtime,planner,host,memory}
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    for directory in ('src/runtime','src/api','src/type','lib/xrt','lib/xruntime','release/include'):
        files.update(f for f in (root/directory).rglob('*') if f.is_file() and f.suffix in ('.h','.c','.inc'))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir(); rows=[]; products=[]; plans=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Plans=plans,
        IndependentlyPrunedNativeCursors=False,LanguagePairedIterationAcceptance=False,Full53Acceptance=False,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include','-I'+str(public/'include'),
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-DXL_STATIC_EMBED',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    ext='.exe' if os.name=='nt' else ''; env=dict(os.environ)
    if os.name=='nt': shutil.copy2(runtime,out/'xl.dll')
    def run(name,cmd):
        p=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=600)
        log=out/(name+'.log'); log.write_bytes(p.stdout+p.stderr)
        rows.append(dict(Name=name,Exit=p.returncode,Log=str(log),LogSha256=sha(log)))
        assert not p.returncode,(name,(p.stdout+p.stderr)[-6500:]); return (p.stdout+p.stderr).decode(errors='replace')
    try:
        objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
        tool=out/('plan'+ext)
        runtimeLinks=[] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)]
        run('plan-build',[cc,*flags,'-Ilib/tcc','-O0',planner,*objects,runtime,*systems,*runtimeLinks,'-o',tool])
        for mode in range(4):
            implementation=out/(str(mode)+'-native.c')
            sources=run(str(mode)+'-plan',[tool,mode,implementation]).splitlines()
            assert sources and all(s.startswith(('src/','tools/')) for s in sources)
            assert 'XRT_MODULE_ALL' not in implementation.read_text() and 'XRUNTIME_MODULE_ALL' not in implementation.read_text()
            plans.append(dict(Mode=mode,Sources=sources))
            for opt in ('O0','O2'):
                providers=[]
                for index,source in enumerate([implementation,*[root/s for s in sources],memory]):
                    obj=out/(str(mode)+'-'+opt+'-'+str(index)+'.o')
                    run(str(mode)+'-'+opt+'-provider-'+str(index),[cc,*flags,'-'+opt,'-c',source,'-o',obj]); providers.append(obj)
                    products.append(dict(Path=str(obj),Sha256=sha(obj)))
                binary=out/(str(mode)+'-'+opt+ext)
                run(str(mode)+'-'+opt+'-link',[cc,*flags,'-'+opt,'-DMODE='+str(mode),host,*providers,*systems,'-o',binary])
                assert 'balanced' in run(str(mode)+'-'+opt+'-execute',[binary])
                symbols=run(str(mode)+'-'+opt+'-symbols',[nm,'--defined-only',binary])
                forbidden=[]
                if not mode&1: forbidden+=['xl6SlotMapKeyIteratorOpsV1Get','xl6SlotMapIteratorKeyLayoutV1','xl6SlotMapKeyBoxV1','xl6SequenceSlotMapKeyCreateV1']
                if mode!=3: forbidden+=['xl6SlotMapPairIteratorCreateV1','xl6SlotMapPairsV1']
                for name in forbidden: assert not re.search(r'\b'+name+r'\b',symbols),name
                products.append(dict(Path=str(binary),Sha256=sha(binary)))
            products.append(dict(Path=str(implementation),Sha256=sha(implementation)))
        products.append(dict(Path=str(tool),Sha256=sha(tool))); report['IndependentlyPrunedNativeCursors']=True
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        report['Complete']=report['InputsUnchanged']
    finally:
        report['PhaseCount']=len(rows)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete']; print('Four native cursor lanes: eight independent programs executed with exact symbol pruning',flush=True)
if __name__=='__main__': main()
