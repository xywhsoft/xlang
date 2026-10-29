"""Frozen generic materialized-value and normalized-result inference gate.
Real complete-source compiler, strict C, JIT/module/AOT, generic/resource
regressions and invalid IR result refusals; not shipping or full P1 acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-build',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();root=Path(__file__).resolve().parents[1];manifest=a.source_build.resolve();out=a.out.resolve()
    assert not out.exists();b=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert b['SourceXrt'] and not b['RuntimeOnly'] and b['EnableExeCompiler']==1
    for item in b['Inputs']+b['Headers']+b['Objects']: assert sha(item['Path'])==item['Sha256'].lower(),item['Path']
    driver=Path(b['Driver']);assert sha(driver)==b['DriverSha256'].lower()
    native=root/'release/xl.dll' if os.name=='nt' else Path(b['NativeRoot'])/'release/libxl.so'
    cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    cases=('xlir_generic_value_inference','xlir_generic_async_value_inference','xlir_generic_owning_value_inference')
    regression=('generic_managed_ownership','generics','xlir_generic_inferred_list','xlir_generic_inferred_array',
        'xlir_generic_inferred_scalar','xlir_generic_static_calls','xlir_generic_tuple_returns','xlir_generic_closure_field',
        'xlir_generic_binary_callback_contract','xlir_generic_field_initializer','xlir_u64_generic_intrinsic',
        'xlir_generic_callable_convention','xlir_product_generic_result','xlir_index_write_generic_abi',
        'cgen_generic_infer','cgen_generic_overload_priority','cgen_generic_signature_suffix','async_generic_class_method',
        'stdlib_channel_native_codecs','stdlib_channel_native_codec_dynamic','stdlib_channel_native_codec_states',
        'stdlib_channel_native_codec_lifetime','stdlib_channel_native_codec_oom','stdlib_channel_native_codec_projection_oom',
        'xlir_channel_codec_readonly','stdlib_future_union_value','stdlib_task_pool','stdlib_task_pool_results',
        'stdlib_task_pool_exit_wait','xlir_nullable_union_member','stdlib_channel_cases',
        'stdlib_channel_direct_future','stdlib_channel_finite_future')
    support=[root/('test/cases/'+name) for name in ('xlir_expect_support.c','xlir_generic_inference_text_support.c')]
    contract=root/'test/cases/xlir_generic_value_inference_contract.c'
    files={Path(__file__).resolve(),manifest,driver,cc,native,contract,*support,
        *[root/('test/cases/'+name+'.xl') for name in cases+regression]}
    files.update(Path(v['Path']) for v in b['Inputs']+b['Headers']+b['Objects'])
    if b.get('LinkResponse'): files.add(Path(b['LinkResponse']))
    inputs={str(f):sha(f) for f in sorted(files)};out.mkdir();rows=[];products=[];failures=[]
    r=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,
        Failures=failures,Scope=__doc__,SourceBuild=str(manifest),ShippingAcceptance=False,FullP1Acceptance=False,
        NetworkTests=False,NetworkSecurityTests=False,SourceCases=len(cases),Regressions=len(regression))
    def save(): (out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def product(path): products.append(dict(Path=str(path),Sha256=sha(path)));save()
    env=dict(os.environ)
    if os.name!='nt': env['XL6_SOURCE_NATIVE_ROOT']=b['NativeRoot']
    def run(name,command):
        command=list(map(str,command));actual=command;log=out/(name+'.log');start=time.monotonic()
        if command[0]==str(cc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+v.replace('\\','/').replace('"','\\"')+'"' for v in command[1:])+'\n',encoding='utf8')
            product(rsp);actual=[str(cc),'@'+str(rsp)]
        with log.open('wb') as stream:
            result=subprocess.run(actual,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240)
        rows.append(dict(Name=name,Command=command,Exit=result.returncode,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save()
        print(name+': '+str(result.returncode),flush=True);assert result.returncode==0,(name,str(log))
        return log.read_text(encoding='utf8',errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),
        '-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include')]
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable',
        '-Wno-unused-label','-Wno-missing-field-initializers']
    ext='.exe' if os.name=='nt' else '.out';save()
    try:
        for name in cases:
            source=root/('test/cases/'+name+'.xl');generated=out/(name+'.c');lowered=out/(name+'.json')
            run(name+'.emit',[driver,source,'cgen-default',generated]);product(generated)
            run(name+'.ir',[driver,source,'ir-lowered',lowered]);product(lowered)
            for opt in ('O0','O2'):
                obj=out/(name+'.'+opt+'.o');run(name+'.'+opt,[cc,*flags,*compat,'-'+opt,'-c',generated,'-o',obj]);product(obj)
            for mode in ('run','module'): run(name+'.'+mode,[driver,source,mode,out/(name+'.'+mode)])
            target=out/(name+'.aot');binary=Path(str(target)+ext);argument=target if os.name=='nt' else binary
            run(name+'.aot.build',[driver,source,'exe',argument]);product(binary);run(name+'.aot.run',[binary])
            implementation=Path(str(argument)+'.temps')/'xl6_xrt_impl.c';product(implementation)
            assert '_MODULE_ALL' not in implementation.read_text(encoding='utf8')
        for name in regression:
            source=root/('test/cases/'+name+'.xl')
            for mode in ('run','module'): run(name+'.'+mode,[driver,source,mode,out/(name+'.'+mode),*support])
        objects=[v['Path'] for v in b['Objects'] if Path(v['Source']).name not in
            ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
        assert len(objects)==len(b['Objects'])-1
        systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else \
            ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(native.parent)]
        if os.name=='nt': shutil.copy2(native,out/native.name);product(out/native.name)
        for opt in ('O0','O2'):
            binary=out/('contract-'+opt+ext)
            run('contract.'+opt+'.build',[cc,*flags,*compat,'-'+opt,contract,*objects,native,*systems,'-o',binary]);product(binary)
            text=run('contract.'+opt+'.run',[binary])
            assert 'invalid-results=4 zero-output=8' in text
        r['IRProof']=[4,8]
        r['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
        r['LogsUnchanged']=all(sha(v['Log'])==v['Sha256'] for v in rows)
        assert len(rows)==94 and r['InputsUnchanged'] and r['LogsUnchanged']
        r['Complete']=True;save()
    except BaseException as error: failures.append(repr(error));save();raise
if __name__=='__main__': main()
