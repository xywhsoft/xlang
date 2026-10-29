"""Frozen offline synchronous Channel closure/union codec acceptance.

Real public-source JIT/module/standalone, strict C, allocation failure and IR
refusals. This is not codec-enabled Case/Future, fresh shipping or whole P1.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--source-build',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();manifest=a.source_build.resolve();out=a.out.resolve()
    assert not out.exists(),'Preserve prior evidence; choose a fresh directory'
    build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
    for row in build['Inputs']+build['Headers']+build['Objects']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    assert sha(build['Driver'])==build['DriverSha256'].lower()
    cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();driver=Path(build['Driver'])
    native=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    cases=('stdlib_channel_closure','stdlib_channel_union','stdlib_channel_codec_matrix',
        'stdlib_channel_codec_lifetime','stdlib_channel_closure_signatures','stdlib_channel_codec_oom','stdlib_channel_codec_thread')
    regression=('cgen_channel','cgen_channel_string','stdlib_channel_cancellation','stdlib_channel_cases',
        'stdlib_channel_direct_future','stdlib_channel_finite_future','stdlib_channel_generic_owned',
        'stdlib_channel_owned_future','stdlib_channel_owned_native','stdlib_concurrent_queue_callable_future')
    files={Path(__file__).resolve(),manifest,cc,native,driver,root/'test/cases/xlir_channel_codec_contract.c',
        *[root/('test/cases/'+name+'.xl') for name in cases+regression]}
    files.update(Path(row['Path']) for row in build['Inputs'])
    inputs={str(f):sha(f) for f in sorted(files)};out.mkdir();rows=[];products=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Failures=failures,
        SourceBuild=str(manifest),SourceBuildSha256=sha(manifest),SynchronousCodecsAccepted=False,
        EncodedCaseFutureAccepted=False,ShippingLibraryAcceptance=False,FullP1Acceptance=False,
        NetworkTests=False,NetworkSecurityTests=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    env=dict(os.environ)
    if os.name!='nt':env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    def run(name,args,proof=None):
        log=out/(name+'.log');start=time.monotonic();command=list(map(str,args));actual=command
        if command[0]==str(cc):
            response=out/(name+'.rsp');response.write_text('\n'.join('"'+v.replace('\\','/').replace('"','\\"')+'"' for v in command[1:])+'\n',encoding='utf8');product(response)
            actual=[str(cc),'@'+str(response)]
        with log.open('wb') as stream:r=subprocess.run(actual,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240)
        rows.append(dict(Name=name,Command=command,Exit=r.returncode,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save()
        print(name+': '+str(r.returncode),flush=True);assert r.returncode==0,(name,str(log))
        text=log.read_text(encoding='utf8',errors='replace');assert not proof or proof in text,(name,'missing proof')
        return text
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(driver.parent/'include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-unused-label','-Wno-missing-field-initializers']
    ext='.exe' if os.name=='nt' else '.out'
    def standalone(name,source):
        prefix=out/(name+'.aot');binary=Path(str(prefix)+ext);arg=prefix if os.name=='nt' else binary
        run(name+'.aot.build',[driver,source,'exe',arg]);product(binary);text=run(name+'.aot.run',[binary])
        implementation=Path(str(arg)+'.temps')/'xl6_xrt_impl.c';product(implementation)
        body=implementation.read_text(encoding='utf8');assert '_MODULE_ALL' not in body
        assert '#define XRT_MODULE_CHANNEL\n' in body
        assert '#define XRT_MODULE_EXECUTOR\n' not in body and '#define XRT_MODULE_TASK_POOL\n' not in body
        return text
    save()
    try:
        for name in cases:
            source=root/('test/cases/'+name+'.xl');generated=out/(name+'.c');ir=out/(name+'.json')
            run(name+'.emit',[driver,source,'cgen-default',generated]);product(generated)
            run(name+'.ir',[driver,source,'ir-lowered',ir]);product(ir)
            data=json.loads(ir.read_text(encoding='utf8'));assert data['moduleTaskPoolAbi']=='none' and data['moduleContinuationAbi']==0
            body=generated.read_text(encoding='utf8');assert 'xl6ChannelSend' in body
            assert ('xl6ChannelReceive' in body)==(name!='stdlib_channel_codec_lifetime'), 'send-only cleanup must remain receive-pruned'
            assert 'static void xlir_channel_' not in body and 'static xvalue* xlir_channel_' not in body
            assert 'channel_slot != NULL && xl6ChannelSend' in body,'encoding failure must not call native with a null carrier'
            for opt in ('O0','O2'):
                obj=out/(name+'.'+opt+'.o');run(name+'.'+opt,[cc,*flags,*compat,'-'+opt,'-c',generated,'-o',obj]);product(obj)
            for mode in ('run','module'):run(name+'.'+mode,[driver,source,mode,out/(name+'.'+mode)])
            text=standalone(name,source)
            if name=='stdlib_channel_codec_oom':
                matched=re.search(r'actual allocation prefixes: closure=(\d+), union=(\d+)',text);assert matched
                report['ActualAllocationPrefixes']=list(map(int,matched.groups()));assert min(report['ActualAllocationPrefixes'])>0
        for name in regression:
            source=root/('test/cases/'+name+'.xl')
            for mode in ('run','module'):run(name+'.'+mode,[driver,source,mode,out/(name+'.'+mode)])
            if name in ('stdlib_channel_cases','stdlib_channel_direct_future','stdlib_channel_finite_future'):standalone(name,source)
        objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
        assert len(objects)==len(build['Objects'])-1
        systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(native.parent)]
        if os.name=='nt':shutil.copy2(native,out/native.name);product(out/native.name)
        for opt in ('O0','O2'):
            binary=out/('contract-'+opt+ext)
            run('contract.'+opt+'.build',[cc,*flags,*compat,'-'+opt,root/'test/cases/xlir_channel_codec_contract.c',*objects,native,*systems,'-o',binary]);product(binary)
            text=run('contract.'+opt+'.run',[binary,*[root/('test/cases/'+name+'.xl') for name in cases]],'Channel value codec plans:')
            match=re.search(r'(\d+) closed logical/storage domains, (\d+) schema refusals, (\d+) generic-control refusals, (\d+) zero-output',text);assert match
            counts=list(map(int,match.groups()));assert counts==[14,126,48,348],counts
            report['IRProof']=counts
        report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
        report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
        assert report['InputsUnchanged'] and report['LogsUnchanged'];report['SynchronousCodecsAccepted']=True;report['Complete']=True;save()
    except BaseException as error:failures.append(repr(error));save();raise
if __name__=='__main__':main()
