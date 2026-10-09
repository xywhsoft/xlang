"""Actual shipping DLL/SO Tree/Queue graph retirement and Queue continuations.
Public JIT uses embedded VFS without source/header/library fallback or provider
overrides. Continuation hosts complete genuine Promises after resident Watch or
Commit using explicit test-only call-site interposition, not fake notifications.
Not independent AOT graph retirement, runtime-wide sanitizers, module-unmap
proof, scheduler-wide OOM, release promotion or full53 acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, sys, time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); root,out=args.root.resolve(),args.out.resolve()
    manifest=args.build_report.resolve(); assert not out.exists()
    build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    project=Path(__file__).resolve().parents[1]; windows=os.name=='nt'
    cc=shutil.which('gcc'); assert cc
    products=[root/'release'/name for name in
        (('xl.dll','xlc.dll','xl.exe') if windows else ('libxl.so','libxlc.so','xl'))]
    cases=project/'test/cases'
    families=[('tree','stdlib_tree_ownership',5,12),
        ('queue','stdlib_concurrent_queue_ownership',6,25)]
    fixtures=[cases/(stem+'.xl') for _,stem,_,_ in families]
    hosts=[cases/(stem+'_public.c') for _,stem,_,_ in families]
    continuation=cases/'stdlib_concurrent_queue_continuation.xl'
    continuation_host=cases/'stdlib_concurrent_queue_continuation_host.c'
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),manifest,
        *fixtures,*hosts,continuation,continuation_host,
        cases/'xlir_multi_await_host_prelude.h',cases/'xlir_multi_await_host_support.h'}
    recorded={Path(row['Path']).resolve():row['Sha256'] for row in build['Products']}
    assert all(recorded[path]==sha(path) for path in products)
    for row in build['Inputs']+build['Products']:
        assert sha(row['Path'])==row['Sha256'],row['Path']; files.add(Path(row['Path']))
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(parents=True); isolated=out/'compiler'
    (isolated/'release').mkdir(parents=True); (isolated/'inputs').mkdir()
    tmp=out/'tmp'; tmp.mkdir(); headers=out/'headers'
    (headers/'__xl_runtime').mkdir(parents=True)
    for product in products:
        shutil.copy2(product,isolated/'release'/product.name)
        if windows: shutil.copy2(product,isolated/product.name)
    for fixture in [*fixtures,continuation]:
        shutil.copy2(fixture,isolated/'inputs'/fixture.name)
    for directory in (root/'release/include',root/'src/runtime'):
        for header in directory.glob('*.h'):
            shutil.copy2(header,headers/'__xl_runtime'/header.name)
    assert not any((isolated/name).exists() for name in ('src','lib','res','include'))
    env={key:value for key,value in os.environ.items() if not key.upper().startswith('XL6_')
        and key not in ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp))
    rows=[]; proofs=[]; failures=[]
    report={'Scope':__doc__,'Complete':False,'InputsUnchanged':False,'Inputs':inputs,
        'Rows':rows,'Proofs':proofs,'Failures':failures,'Platform':sys.platform,
        'SourceProductAcceptance':True,'ProviderOverrides':False,
        'EmbeddedVfsJitWithoutFallback':True,'ContinuationCallSiteInterposition':True,
        'RuntimeInstrumented':False,'Full53Acceptance':False,
        'IndependentAotGraphRetirementAcceptance':False,'ModuleUnmapAcceptance':False,
        'SchedulerWideOomAcceptance':False}
    def save():
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=list(map(str,command)); start=time.monotonic()
        try:
            result=subprocess.run(command,cwd=isolated,env=env,capture_output=True,timeout=240)
            data,code=result.stdout+result.stderr,result.returncode
        except subprocess.TimeoutExpired as error:
            data,code=(error.stdout or b'')+(error.stderr or b''),'timeout-240s'
        (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':code,'Seconds':time.monotonic()-start}); save()
        assert code==0,(name,data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    def batch(name,action):
        try: action()
        except Exception as error:
            failures.append({'Name':name,'Error':repr(error)}); save()
            print(name+': FAILED '+repr(error),flush=True)
    suffix='.exe' if windows else ''; cli=isolated/'release'/products[2].name
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',
        '-I'+str(root),'-I'+str(root/'release/include'),'-I'+str(headers)]
    if not windows: flags+=['-D_GNU_SOURCE']
    generated_compat=['-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable',
        '-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers']
    def libraries(product):
        if windows: return [isolated/'release'/product.name]
        return ['-L'+str(isolated/'release'),'-l'+product.stem[3:],
            '-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
    def graph(label,stem,count,lifetimes):
        source=isolated/'inputs'/(stem+'.xl'); generated=out/(label+'.c')
        repeated=out/(label+'.repeat.c')
        for name,target in [('emit',generated),('repeat',repeated)]:
            run(label+'.'+name,[cli,'cgen',source,'--emit',target])
        assert generated.read_bytes()==repeated.read_bytes()
        text=generated.read_text(encoding='utf-8')
        if label=='tree': assert 'xlir_avl_' not in text and 'xl6TreeCreateTakeV1(' in text
        else:
            assert 'xlir_special_queue_' not in text
            assert all('xl6'+name+'QueueCreateTakeV1(' in text for name in ('Spsc','Mpsc','Mpmc'))
        modes=[(0,0)]+[(mode,family) for mode in (1,2,3) for family in range(count)]
        for level in ('O0','O2'):
            run(label+'.'+level+'.strict-C',[cc,*flags,*generated_compat,'-'+level,
                '-c',generated,'-o',out/(label+'.'+level+'.o')])
            for product in products[:2]:
                name=label+'.'+product.stem+'.'+level; binary=isolated/(name+suffix)
                run(name+'.build',[cc,*flags,'-Wshadow','-'+level,'-DXLIR_GRAPH_EMBEDDED_VFS',
                    cases/(stem+'_public.c'),*libraries(product),'-o',binary])
                for mode,family in modes:
                    tag=name+'.mode'+str(mode)+'.family'+str(family)
                    def check(m=mode,f=family,n=tag,b=binary,p=product,l=level):
                        text=run(n,[b,m,f,'',out/(n+'.temps'),isolated/'release'/p.name,'',source])
                        assert str(lifetimes)+' '+('measured actual' if label=='tree' else 'actual')+\
                            ' compile/JIT/retire lifetimes' in text and 'balanced' in text
                        proofs.append({'Case':label,'Product':p.name,'Optimization':l,
                            'Mode':m,'Family':f,'MeasuredLifetimes':lifetimes}); save()
                        print(n+': balanced '+str(lifetimes)+' measured JIT lifetimes',flush=True)
                    batch(tag,check)
    for family in families: batch(family[0],lambda f=family:graph(*f))
    def continuations():
        source=isolated/'inputs'/continuation.name; generated=out/'continuation.c'
        repeated=out/'continuation.repeat.c'; ir=out/'continuation.ir.json'
        for label,target in [('emit',generated),('repeat',repeated)]:
            run('continuation.'+label,[cli,'cgen',source,'--emit',target])
        assert generated.read_bytes()==repeated.read_bytes()
        run('continuation.ir',[cli,'ir',source,'--lowered','--format','json','--emit',ir])
        data=json.loads(ir.read_text(encoding='utf-8'))
        assert len(data['asyncMachines'])==1 and len(data['asyncSuspensions'])==3 and len(data['asyncMachineEntries'])==13
        assert all(row['futureOutcomeAbi']==1 for row in data['instructions'] if row['kindName']=='await')
        code=generated.read_text(encoding='utf-8'); assert 'xlir_special_queue_' not in code
        assert all('xl6'+name+'QueueCreateTakeV1(' in code for name in ('Spsc','Mpsc','Mpmc'))
        for product in products[:2]:
            for level in ('O0','O2'):
                for terminal in ('success','reject','cancel','close'):
                    for timing in ('ready','pending-watch','pending-commit'):
                        name='continuation.'+product.stem+'.'+level+'.'+terminal+'.'+timing
                        def check(n=name,p=product,l=level,t=terminal,when=timing):
                            binary=isolated/(n+suffix)
                            defines=['-DXLIR_MULTI_AWAIT_GENERATED_SOURCE="'+generated.as_posix()+'"']
                            if t!='success': defines+=['-DXLIR_CHAIN_THIRD_'+t.upper()]
                            if when!='ready': defines+=['-DXLIR_MULTI_AWAIT_PENDING']
                            if when=='pending-commit': defines+=['-DXLIR_MULTI_AWAIT_AFTER_COMMIT']
                            run(n+'.build',[cc,*flags,*generated_compat,'-'+l,*defines,
                                continuation_host,*libraries(p),'-o',binary])
                            text=run(n+'.run',[binary])
                            assert 'two complete language lifecycles and memory balance' in text and 'terminal cleanup balanced' in text
                            if when!='ready': assert 'every input completed after actual pending Watch registration' in text
                            if when=='pending-commit': assert 'deferred completion after actual activation Commit' in text
                            proofs.append({'Case':'continuation','Product':p.name,'Optimization':l,
                                'Terminal':t,'Timing':when,'Activations':2,'FinallyCallsPerActivation':1}); save()
                            print(n+': two balanced activations, once-only finally',flush=True)
                        batch(name,check)
    batch('continuations',continuations)
    report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
    report['Complete']=not failures and report['InputsUnchanged'] and len(proofs)==4*(16+19)+48
    save(); print(f'Container graph shipping: {len(rows)} phases, {len(proofs)} modes, complete={report["Complete"]}',flush=True)
    return 0 if report['Complete'] else 1


if __name__=='__main__': raise SystemExit(main())
