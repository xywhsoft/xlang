"""Pending module stop and monotonic owned-pool completion source gate.
Real ThreadKey tails, deterministic unrelated-mutation exclusion, checked
failures, strict C/JIT/module/AOT and two behavioral negative controls.
Not shipping, full P1 or network/security acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-build',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();root=Path(__file__).resolve().parents[1];manifest=a.source_build.resolve();out=a.out.resolve()
    assert not out.exists();b=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert b['SourceXrt'] and not b['RuntimeOnly'] and b['EnableExeCompiler']==1
    for v in b['Inputs']+b['Headers']+b['Objects']:assert sha(v['Path'])==v['Sha256'].lower(),v['Path']
    driver=Path(b['Driver']);assert sha(driver)==b['DriverSha256'].lower()
    cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    native=root/'release/xl.dll' if os.name=='nt' else Path(b['NativeRoot'])/'release/libxl.so'
    cases=('xlir_fixed_executor_language','stdlib_task_pool_exit_wait','stdlib_task_pool_thread_local')
    regression=('stdlib_task_pool','stdlib_task_pool_results','stdlib_task_pool_control','stdlib_task_pool_errors',
        'stdlib_task_pool_self_release','stdlib_task_pool_unused','stdlib_task_pool_borrowed','stdlib_future_c_scalars',
        'stdlib_executor','stdlib_executor_control','stdlib_executor_errors','stdlib_executor_thread_local',
        'stdlib_executor_batch','stdlib_executor_batch_errors','stdlib_executor_self_release','stdlib_executor_mixed_services',
        'stdlib_promise_unit','stdlib_promise_dynamic_null','stdlib_promise_values','stdlib_promise_terminal',
        'stdlib_task_group_start','stdlib_cancellation','stdlib_channel_direct_future','stdlib_channel_finite_future',
        'stdlib_channel_message_objects','stdlib_channel_message_containers','stdlib_channel_native_codec_states',
        'stdlib_channel_native_codec_lifetime','xlir_null_value_comparison','xlir_null_value_comparison_async',
        'xlir_null_comparison_cleanup')
    failures=('stdlib_task_pool_unobserved_exit','stdlib_executor_unobserved_error','stdlib_executor_unobserved_exit')
    programs=(('native','xlir_task_pool_completion_native'),('contract','xlir_module_task_pool_contract'))
    runtime=root/'src/runtime/xl6_task_pool.c'
    binding=root/'test/cases/xlir_one_shot_tcc_host.c';guard=root/'tools/check_xlir_contract.ps1'
    files={Path(__file__).resolve(),manifest,driver,cc,native,runtime,
        *[root/('test/cases/'+n+'.xl') for n in cases+regression+failures],
        *[root/('test/cases/'+n+'.c') for _,n in programs],root/'test/cases/xlir_module_task_pool.xl',binding,guard}
    files.update(Path(v['Path']) for v in b['Inputs']+b['Headers']+b['Objects'])
    if b.get('LinkResponse'):files.add(Path(b['LinkResponse']))
    inputs={str(f):sha(f) for f in sorted(files)};out.mkdir();rows=[];products=[];errors=[]
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,
        Failures=errors,Scope=__doc__,SourceBuild=str(manifest),ShippingAcceptance=False,FullP1Acceptance=False,
        NetworkTests=False,NetworkSecurityTests=False,ExpectedLanguageFailures=len(failures)*2,
        NegativeControls=2,Regressions=len(regression),PendingStops=32,CompletedWaits=6144)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    env=dict(os.environ)
    if os.name!='nt':env['XL6_SOURCE_NATIVE_ROOT']=b['NativeRoot']
    def run(name,command,reject=False,proof=None):
        command=list(map(str,command));actual=command;log=out/(name+'.log');start=time.monotonic()
        if command[0]==str(cc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+v.replace('\\','/').replace('"','\\"')+'"' for v in command[1:])+'\n',encoding='utf8')
            product(rsp);actual=[str(cc),'@'+str(rsp)]
        with log.open('wb') as stream:
            result=subprocess.run(actual,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
        rows.append(dict(Name=name,Command=command,Exit=result.returncode,ExpectedRejection=reject,
            Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save()
        print(name+': '+str(result.returncode),flush=True)
        assert (result.returncode!=0 if reject else result.returncode==0),(name,str(log))
        text=log.read_text(encoding='utf8',errors='replace')
        if proof:assert proof in text,(name,text[-2000:])
        return text
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(driver.parent/'include'),'-I'+str(root/'src/runtime')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable',
        '-Wno-unused-label','-Wno-missing-field-initializers']
    ext='.exe' if os.name=='nt' else '.out';save()
    try:
        assert binding.read_text(encoding='utf8').count('BIND(xl6TaskPoolModulePrepareV1);')==1
        assert "!$cEmitter.Contains('xl6TaskPoolModulePrepareV1(xlir_async_pool, teardown, wait)')" in guard.read_text(encoding='utf-8-sig')
        for name in cases:
            source=root/('test/cases/'+name+'.xl');generated=out/(name+'.c');lowered=out/(name+'.json')
            run(name+'.emit',[driver,source,'cgen-default',generated]);product(generated)
            run(name+'.ir',[driver,source,'ir-lowered',lowered]);product(lowered)
            body=generated.read_text(encoding='utf8')
            assert 'xl6TaskPoolTryQuiesceV1(xlir_async_pool)' not in body
            assert ('xl6TaskPoolModulePrepareV1(xlir_async_pool, teardown, wait)' in body)==(name==cases[0])
            for opt in ('O0','O2'):
                obj=out/(name+'.'+opt+'.o');run(name+'.'+opt,[cc,*flags,*compat,'-'+opt,'-c',generated,'-o',obj]);product(obj)
            for mode in ('run','module'):run(name+'.'+mode,[driver,source,mode,out/(name+'.'+mode)])
            target=out/(name+'.aot');binary=Path(str(target)+ext);argument=target if os.name=='nt' else binary
            run(name+'.aot.build',[driver,source,'exe',argument]);product(binary);run(name+'.aot.run',[binary])
            implementation=Path(str(argument)+'.temps')/'xl6_xrt_impl.c';product(implementation)
            assert '_MODULE_ALL' not in implementation.read_text(encoding='utf8')
        for name in regression:
            for mode in ('run','module'):run(name+'.'+mode,[driver,root/('test/cases/'+name+'.xl'),mode,out/(name+'.'+mode)])
        for name in failures:
            for mode in ('run','module'):
                text=run(name+'.'+mode,[driver,root/('test/cases/'+name+'.xl'),mode,out/(name+'.'+mode)],True)
                assert rows[-1]['Exit']==1 and 'source compile failed' not in text and 'implicit declaration' not in text
                if name.endswith('_exit'):
                    assert 'retirement incomplete or failed' in text or 'uncaught xlang error: ValueError' in text
        objects=[v['Path'] for v in b['Objects'] if Path(v['Source']).name not in
            ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
        assert len(objects)==len(b['Objects'])-1
        systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else \
            ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(native.parent)]
        if os.name=='nt':shutil.copy2(native,out/native.name);product(out/native.name)
        for opt in ('O0','O2'):
            for name,source in programs:
                binary=out/(name+'-'+opt+ext)
                run(name+'.'+opt+'.build',[cc,*flags,*compat,'-'+opt,root/('test/cases/'+source+'.c'),*objects,native,*systems,'-o',binary]);product(binary)
                args=[] if name=='native' else [out/('module-'+opt+'.'+s) for s in ('c','h','json')]
                proof='32 pending stops resumed outside graph admission, 6144 stable completed waits' if name=='native' else '9 ABI partitions, 114 rejected IR mutations'
                run(name+'.'+opt+'.run',[binary,*args],proof=proof)
                for path in args:product(path)
        kernel=next(v['Path'] for v in b['Objects'] if Path(v['Source']).name=='xl6_task_pool.c')
        others=[v for v in objects if v!=kernel];assert len(others)==len(objects)-1
        bads=(('lost-pending','teardown||pending?xl6TaskPoolTryQuiesceV1(pool):XL6_MODULE_PREPARE_READY',
            'teardown?xl6TaskPoolTryQuiesceV1(pool):XL6_MODULE_PREPARE_READY',
            'xl6TaskPoolModulePrepareV1(pool,false,false)==XL6_MODULE_PREPARE_BUSY'),
            ('lost-completion','pool->exitComplete=true;','pool->exitComplete=false;',
            'api->WaitFor(pool,0,NULL,&status)&&status==XL6_TASK_POOL_READY_V1'))
        for name,needle,replacement,proof in bads:
            body=runtime.read_text(encoding='utf8');assert body.count(needle)==1
            bad=out/(name+'.c');bad.write_text(body.replace(needle,replacement),encoding='utf8');product(bad)
            binary=out/(name+ext)
            run(name+'.build',[cc,*flags,*compat,'-O0',root/'test/cases/xlir_task_pool_completion_native.c',bad,*others,native,*systems,'-o',binary]);product(binary)
            run(name+'.run',[binary],True,proof)
        report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
        report['LogsUnchanged']=all(sha(v['Log'])==v['Sha256'] for v in rows)
        assert len(rows)==len(cases)*8+len(regression)*2+len(failures)*2+12
        assert report['InputsUnchanged'] and report['LogsUnchanged'];report['Complete']=True;save()
    except BaseException as error:errors.append(repr(error));save();raise
if __name__=='__main__':main()
