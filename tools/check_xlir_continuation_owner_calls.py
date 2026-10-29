"""Current-source consuming-call ownership contract and real queued execution.
Private scaffolding constructs owned physical parameters, not source syntax.
The Watch observer includes the production transport, never an inline executor.
Focused source-object proof only: not shipping libraries, VFS, or JIT/AOT.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--compiler',default='gcc');a=p.parse_args()
    root,public,out=(v.resolve() for v in (a.root,a.public_directory,a.out));assert not out.exists(),'Choose fresh evidence'
    windows=os.name=='nt';cc=Path(shutil.which(a.compiler) or a.compiler).resolve()
    b=json.loads((public/'build.json').read_text(encoding='utf-8-sig'))
    assert b.get('Complete',windows) and b['SourceXrt'] and not b['RuntimeOnly']
    driver=Path(b['Driver']);assert sha(driver)==b['DriverSha256'].lower()
    driver_names=('xlir_public_source_driver','xlir_public_source_linux_driver')
    mains=[Path(r['Path']) for r in b['Inputs'] if Path(r['Path']).stem in driver_names];assert len(mains)==1
    compiled=mains[0].parents[2];compiled_sources={Path(s) if Path(s).is_absolute() else compiled/s for s in b['Sources']}
    pinned={Path(__file__).resolve(),root/'tools/check_xlir_continuation_owner_calls.ps1',
        root/'tools/check_xlir_contract.ps1',root/'test/suites/03_core_function_abi.bat',public/'build.json',cc,driver};production=set()
    for row in b['Inputs']+b['Headers']+b['Objects']:
        path=Path(row['Path']);assert sha(path)==row['Sha256'].lower(),str(path);pinned.add(path)
        if path.is_relative_to(compiled):
            relative=path.relative_to(compiled)
            if path in compiled_sources or relative.parts[0] in ('src','lib','res','include') or relative.parts[:2]==('release','include'):
                current=root/relative
                assert path.read_bytes().replace(b'\r\n',b'\n')==current.read_bytes().replace(b'\r\n',b'\n'),str(current)
                pinned.add(current);production.add(relative.as_posix())
    assert {'src/ir/xl6_ir.c','src/ir/xl6_ir_ownership.c','src/runtime/xl6_continuation_future.c'}<=production
    objects=[r['Path'] for r in b['Objects'] if Path(r['Path']).stem not in driver_names];assert len(objects)>600
    observed=[v for v in objects if Path(v).name!='xl6_continuation_future.o'];assert len(observed)==len(objects)-1
    runtime=root/'release/xl.dll' if windows else Path(b['NativeRoot'])/'release/libxl.so';pinned.add(runtime)
    if windows:assert sha(runtime)==b['RuntimeSha256'].lower()
    case=root/'test/cases';contract=case/'xlir_continuation_owner_calls_contract.c'
    runtime_sources=[case/name for name in ('xlir_continuation_owner_calls_runtime.c','xlir_continuation_owner_calls_host.c',
        'xlir_async_source_sync.c','xlir_async_source_transport.c')]
    pinned.update(runtime_sources);pinned.add(contract);pinned.add(case/'xlir_continuation_owner_calls.xl')
    pinned.update(case/name for name in ('xlir_continuation_owner_test.h','xlir_synthetic_owner_edit.h','xlir_internal_owned_params.h',
        'xlir_owner_fault_runtime.h','xlir_async_source_sync.h','xlir_launch_test.h'))
    inputs={str(v):sha(v) for v in sorted(pinned)};out.mkdir(parents=True);rows=[];products=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,ProductsUnchanged=False,Scope=__doc__,
        ProductionFiles=len(production),SourceObjects=len(objects),Inputs=inputs,Rows=rows,Products=products,Failures=failures,
        SyntheticOwnedAbi=True,RealQueuedExecutor=True,TransportObservationOnly=True,ShippingAcceptance=False,
        JitAotAcceptance=False,FullLongTermTaskAcceptance=False,NetworkTests=False)
    env=dict(os.environ)
    if windows:shutil.copy2(runtime,out/'xl.dll')
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+os.pathsep+env.get('LD_LIBRARY_PATH','')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-DXL_ENABLE_EXE_COMPILER=1','-DTCC_DISABLE_BUILTIN_VFS',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(public/'include'),
        '-Wno-unused-function','-Wno-unused-label','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation']
    libs=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)])]
    if not windows:flags+=['-D_GNU_SOURCE']
    suffix='.exe' if windows else ''
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,command,reject=False):
        command=list(map(str,command));started=time.monotonic();log=out/(name+'.log')
        with log.open('wb') as stream:r=subprocess.run(command,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
        rows.append(dict(Name=name,Command=command,Exit=r.returncode,ExpectedRejection=reject,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started));save()
        print(name+': '+str(r.returncode),flush=True);text=log.read_text(encoding='utf8',errors='replace')
        assert (r.returncode!=0 if reject else r.returncode==0),(name,text[-4000:])
        if reject:assert 'assert' in text.lower(),(name,'not a runtime assertion rejection')
        return text
    def compile_to(name,selected,target,extra=()):
        command=[*flags,*extra,*selected,*libs,'-o',target];response=out/(name+'.rsp')
        response.write_text('\n'.join('"'+str(v).replace('\\','/').replace('"','\\"')+'"' for v in command)+'\n',encoding='utf8');product(response)
        run(name,[cc,'@'+str(response)]);product(target)
    try:
        save()
        for opt in ('O0','O2'):
            prefix=out/opt;binary=out/('contract-'+opt+suffix)
            compile_to(opt+'-contract-build',[contract,*objects],binary,['-'+opt])
            for variant in ('','.repeat'):
                args=[Path(str(prefix)+variant+'.'+s) for s in ('c','h','json')]
                text=run(opt+'-contract'+variant,[binary,*args])
                assert re.search(r'3 verified SSA owner regions, 6 exact consumer actions, 331 mutations rejected',text),text
                for s in ('c','h','json','h.bind.h'):product(Path(str(prefix)+variant+'.'+s))
            for s in ('c','h','json','h.bind.h'):
                assert Path(str(prefix)+'.'+s).read_bytes()==Path(str(prefix)+'.repeat.'+s).read_bytes(),s
            for variant in ('normal','no-drop'):
                generated=Path(str(prefix)+'.c')
                if variant=='no-drop':
                    # Keep the drop implementation and real worker algorithm;
                    # omit only explicit generated SSA cleanup calls. It must
                    # compile, then fail the precise memory ledger at runtime.
                    text=generated.read_text(encoding='utf8');needle='xlir_string_drop(xlir_released);';assert needle in text
                    generated=out/(opt+'.no-drop.c');generated.write_text(text.replace(needle,'(void)xlir_released;'),encoding='utf8');product(generated)
                binary=out/(opt+'-'+variant+suffix)
                compile_to(opt+'-'+variant+'-build',[*runtime_sources,*observed],binary,['-'+opt,
                    '-DOWNER_CALLS_GENERATED_C="'+generated.as_posix()+'"','-DOWNER_CALLS_BINDING_H="'+Path(str(prefix)+'.h.bind.h').as_posix()+'"'])
                text=run(opt+'-'+variant,[binary],variant=='no-drop')
                if variant=='normal':
                    for proof in ('1920 real setup allocation failures','1600 real result-box allocation failures','640 null-source cases',
                        '8500 executions, 7200 genuine suspensions','every acquisition prefix'):assert proof in text,proof
                    run(opt+'-smoke',[binary,'--smoke'])
        report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
        report['LogsUnchanged']=all(sha(r['Log'])==r['Sha256'] for r in rows)
        report['ProductsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in products)
        assert report['InputsUnchanged'] and report['LogsUnchanged'] and report['ProductsUnchanged']
        report['Complete']=True;save()
    except BaseException as error:failures.append(repr(error));save();raise

if __name__=='__main__':main()
