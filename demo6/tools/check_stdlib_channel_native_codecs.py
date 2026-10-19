"""Frozen offline Channel native-call representation and exact union Future ABI.
Complete-source compiler, JIT/module/AOT, strict C, real allocation prefixes and
independent IR refusals; not whole P1 or fresh shipping product acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-build',type=Path,required=True);parser.add_argument('--out',type=Path,required=True)
    a=parser.parse_args();root=Path(__file__).resolve().parents[1];manifest=a.source_build.resolve();out=a.out.resolve()
    assert not out.exists();build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
    for item in build['Inputs']+build['Headers']+build['Objects']:assert sha(item['Path'])==item['Sha256'].lower(),item['Path']
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    native=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    cases=('stdlib_channel_native_codecs','stdlib_channel_native_codec_dynamic','xlir_channel_codec_readonly',
        'stdlib_channel_native_codec_oom','stdlib_channel_native_codec_states','stdlib_channel_native_codec_projection_oom',
        'stdlib_future_union_value','stdlib_channel_native_codec_lifetime')
    regression=('cgen_channel','cgen_channel_string','stdlib_channel_codec_matrix','stdlib_channel_codec_thread',
        'stdlib_channel_cases','stdlib_channel_direct_future','stdlib_channel_finite_future','stdlib_promise',
        'stdlib_promise_dynamic','async_generic_class_method','xlir_nullable_union_member','stdlib_task_pool',
        'stdlib_task_pool_results','stdlib_task_pool_exit_wait')
    contract=root/'test/cases/xlir_native_call_value_contract.c'
    files={Path(__file__).resolve(),manifest,driver,cc,native,contract,
        *[root/('test/cases/'+name+'.xl') for name in cases+regression]}
    files.update(Path(item['Path']) for item in build['Inputs'])
    inputs={str(f):sha(f) for f in sorted(files)};out.mkdir();rows=[];products=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,
        Failures=failures,Scope=__doc__,SourceBuild=str(manifest),ShippingAcceptance=False,FullP1Acceptance=False,
        NetworkTests=False,NetworkSecurityTests=False,TrueDynamicWitness=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    env=dict(os.environ)
    if os.name!='nt':env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    def run(name,command):
        command=list(map(str,command));actual=command;log=out/(name+'.log');start=time.monotonic()
        if command[0]==str(cc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+v.replace('\\','/').replace('"','\\"')+'"' for v in command[1:])+'\n',encoding='utf8')
            product(rsp);actual=[str(cc),'@'+str(rsp)]
        with log.open('wb') as stream:result=subprocess.run(actual,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240)
        rows.append(dict(Name=name,Command=command,Exit=result.returncode,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save()
        print(name+': '+str(result.returncode),flush=True);assert result.returncode==0,(name,str(log))
        return log.read_text(encoding='utf8',errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),
        '-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable',
        '-Wno-unused-label','-Wno-missing-field-initializers']
    ext='.exe' if os.name=='nt' else '.out';save()
    try:
        for name in cases:
            source=root/('test/cases/'+name+'.xl');generated=out/(name+'.c');lowered=out/(name+'.json')
            run(name+'.emit',[driver,source,'cgen-default',generated]);product(generated)
            run(name+'.ir',[driver,source,'ir-lowered',lowered]);product(lowered)
            body=generated.read_text(encoding='utf8')
            assert 'static void xlir_channel_' not in body and 'static xvalue* xlir_channel_' not in body
            if name in ('stdlib_channel_native_codec_dynamic','stdlib_channel_native_codec_projection_oom'):
                adapters=re.findall(r'^static bool xlir_dynamic_method_dm\d+\(.*?^\}\n',body,re.M|re.S)
                assert any(all(token in adapter for token in ('native_value_output','&callResult.value','&nativeScope',
                    'XL6_FUTURE_VALUE_XVALUE, sizeof(xvalue*)','&native_value_output')) for adapter in adapters)
                report['TrueDynamicWitness']=True
            for opt in ('O0','O2'):
                obj=out/(name+'.'+opt+'.o');run(name+'.'+opt,[cc,*flags,*compat,'-'+opt,'-c',generated,'-o',obj]);product(obj)
            for mode in ('run','module'):run(name+'.'+mode,[driver,source,mode,out/(name+'.'+mode)])
            target=out/(name+'.aot');binary=Path(str(target)+ext);argument=target if os.name=='nt' else binary
            run(name+'.aot.build',[driver,source,'exe',argument]);product(binary);text=run(name+'.aot.run',[binary])
            implementation=Path(str(argument)+'.temps')/'xl6_xrt_impl.c';product(implementation)
            xrt=implementation.read_text(encoding='utf8');assert '_MODULE_ALL' not in xrt
            if name=='stdlib_future_union_value':assert '#define XRT_MODULE_CHANNEL\n' not in xrt
            if name=='stdlib_channel_native_codec_oom':
                prefixes=[list(map(int,m)) for m in re.findall(r'actual prefixes mode=(\d+): closure=(\d+), union=(\d+)',text)]
                assert prefixes==[[0,15,12],[1,18,15],[2,19,16]],prefixes;report['InputPrefixes']=prefixes
            if name=='stdlib_channel_native_codec_projection_oom':
                prefixes=[list(map(int,m)) for m in re.findall(r'projection actual prefixes mode=(\d+): (\d+)',text)]
                assert prefixes==[[0,2],[1,7]],prefixes;report['ProjectionPrefixes']=prefixes
        for name in regression:
            source=root/('test/cases/'+name+'.xl')
            for mode in ('run','module'):run(name+'.'+mode,[driver,source,mode,out/(name+'.'+mode)])
        objects=[item['Path'] for item in build['Objects'] if Path(item['Source']).name not in
            ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
        assert len(objects)==len(build['Objects'])-1
        systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else \
            ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(native.parent)]
        if os.name=='nt':shutil.copy2(native,out/native.name);product(out/native.name)
        for opt in ('O0','O2'):
            binary=out/('contract-'+opt+ext)
            run('contract.'+opt+'.build',[cc,*flags,*compat,'-'+opt,contract,*objects,native,*systems,'-o',binary]);product(binary)
            text=run('contract.'+opt+'.run',[binary,*[root/('test/cases/'+name+'.xl') for name in cases]])
            matched=re.search(r'plans=(\d+) inputs=(\d+) outputs=(\d+) stored-profiles=(\d+) readonly=(\d+) union-workers=(\d+) union-awaits=(\d+) mutations=(\d+) zero-output=(\d+)',text)
            assert matched;proof=list(map(int,matched.groups()));assert proof[-1]==proof[-2]*2
            if 'IRProof' in report:assert report['IRProof']==proof
            report['IRProof']=proof
        report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
        report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
        assert len(rows)==96 and report['TrueDynamicWitness'] and report['InputsUnchanged'] and report['LogsUnchanged']
        report['Complete']=True;save()
    except BaseException as error:failures.append(repr(error));save();raise
if __name__=='__main__':main()
