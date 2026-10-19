"""Current-IR real queued async workers: preserve the full completion/cancellation/
destructor/error populations, actual worker result-allocation faults, exact
string/NUL results and module logical quiescence/memory balance. Windows/Linux O0/O2 plus
optional selected Linux ASan/UBSan. Not worker-wide OOM or full53 acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
from xlir_public_build_inputs import verify_public_build
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    assert not out.exists() and (not a.sanitize or os.name!='nt');cc=shutil.which('gcc');assert cc
    build,pinned,compared=verify_public_build(root,public)
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    name='xlir_async_worker_completion';files=pinned|{Path(__file__).resolve(),Path(cc),runtime,root/'test/cases/xlir_artifact_runtime_contract.c'}
    files.update(root/'test/cases'/f for f in (name+'.xl',name+'_contract.c',name+'_host.c',name+'_runtime.c'))
    files.update((root/'test/cases').glob('xlir_*.h'))
    root_native=root/'test/cases/xlir_async_root_launch_native.c';files.add(root_native)
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir();rows=[];products=[];proofs=[];env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env.update(XL6_SOURCE_NATIVE_ROOT=build['NativeRoot'],XL6_TCC_LIB=str(Path(build['NativeRoot'])/'build/linux/tcc-dist'))
    report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,Products=products,Proofs=proofs,
        ComparedSourceInputs=compared,SelectedClosureSanitized=a.sanitize,SourceProviderOverrides=False,
        Full53Acceptance=False,ShippingArtifact=False,WorkerWideOomAcceptance=False,ModuleUnmapAcceptance=False)
    def run(label,command):
        command=list(map(str,command));actual=command;response=None
        if command[0]==cc:
            response=out/(label+'.rsp')
            response.write_text(''.join('"'+v.replace('\\','/').replace('"','\\"')+'"\n' for v in command[1:]),encoding='utf8')
            actual=[cc,'@'+str(response)]
        r=subprocess.run(actual,cwd=root,env=env,capture_output=True,timeout=600)
        data=r.stdout+r.stderr;log=out/(label+'.log');log.write_bytes(data)
        rows.append(dict(Name=label,Command=command,ActualCommand=actual,ResponseSha256=sha(response) if response else None,
            Exit=r.returncode,Log=str(log),LogSha256=sha(log)))
        assert r.returncode==0,(label,data[-6000:]);return data.decode(errors='replace')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)))
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(root/'lib/xrt'),'-I'+str(root/'lib/xruntime'),'-I'+str(root/'lib/tcc'),
        '-I'+str(public/'include'),'-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    links=systems+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)]);ext='.exe' if os.name=='nt' else ''
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    try:
        tool=out/('contract'+ext);run('contract-build',[cc,*flags,'-O0',root/'test/cases'/(name+'_contract.c'),*objects,runtime,*links,'-o',tool]);product(tool)
        for repeat in ('','-repeat'):
            prefix=out/('worker'+repeat);run('generate'+repeat,[tool,str(prefix)+'.c',str(prefix)+'.h',str(prefix)+'.json'])
        for suffix in ('c','h','h.bind.h','json'):
            first=out/('worker.'+suffix);other=out/('worker-repeat.'+suffix);assert sha(first)==sha(other);product(first);product(other)
        planner=out/('artifact'+ext);run('artifact-build',[cc,*flags,'-O0',root/'test/cases/xlir_artifact_runtime_contract.c',*objects,runtime,*links,'-o',planner]);product(planner)
        native=out/'native.c';text=run('plan',[planner,'--emit-native','test/cases/'+name+'.xl',native])
        plans=[json.loads(line) for line in text.splitlines() if line.startswith('{"Fixture":')];assert len(plans)==1
        sources=plans[0]['Sources'];raw=native.read_text();assert 'XRT_MODULE_ALL' not in raw and 'XRUNTIME_MODULE_ALL' not in raw
        native.write_text('#define XRT_MODULE_MEMORY_DEBUG\n#define XRT_MODULE_SEM\n'+raw);product(native)
        configs=[('O0',[]),('O2',[])]+([('san',['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie'])] if a.sanitize else [])
        for opt,extra in configs:
            level='-O1' if opt=='san' else '-'+opt;sanlinks=['-fsanitize=address,undefined','-no-pie'] if opt=='san' else []
            if opt=='san':env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            providers=[]
            for i,s in enumerate([native,*[root/f for f in sources]]):
                obj=out/(opt+'-provider-'+str(i)+'.o');run(opt+'-provider-'+str(i),[cc,*flags,*extra,level,'-c',s,'-o',obj]);providers.append(obj);product(obj)
            root_binary=out/('root-'+opt+ext)
            run(opt+'-root-build',[cc,*flags,*extra,level,root_native,*providers,*systems,*sanlinks,'-o',root_binary]);product(root_binary)
            root_text=run(opt+'-root-execute',[root_binary])
            assert '32 real single-worker barriers' in root_text and 'complete allocation prefixes' in root_text and 'exact memory balance' in root_text
            binary=out/('worker-'+opt+ext)
            definitions=['-DXLIR_WORKER_COMPLETION_SOURCE="'+(out/'worker.c').as_posix()+'"',
                '-DXLIR_WORKER_COMPLETION_BINDING="'+(out/'worker.h.bind.h').as_posix()+'"']
            run(opt+'-build',[cc,*flags,*extra,level,*definitions,root/'test/cases'/(name+'_host.c'),root/'test/cases'/(name+'_runtime.c'),*providers,*systems,*sanlinks,'-o',binary]);product(binary)
            text=run(opt+'-execute',[binary]);assert '3978 executions, 90 real worker result allocation failures' in text and '972 discarded Futures' in text and 'balanced allocations' in text
            assert 'worker allocation prefixes: 5 complete lanes' in text and 'real worker TLS failures, module logical quiescence and exact memory balance' in text
            proofs.append(dict(Optimization=opt,Output=text,FullOriginalPopulation=True))
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs);report['Complete']=report['InputsUnchanged']
    finally:report['PhaseCount']=len(rows);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'];print('Actual worker completion population and exact string results passed',flush=True)
if __name__=='__main__':main()
