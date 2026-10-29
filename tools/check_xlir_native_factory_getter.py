"""Offline native getter IR/public JIT/module/AOT regression, no network tests.

Requires an immutable full-source compiler build, hashes all actual objects,
headers, logs and products. No replaced compiler objects or automatic retry.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--source-build',type=Path,required=True);p.add_argument('--native-root',type=Path)
    p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    root=a.root.resolve();manifest=a.source_build.resolve();out=a.out.resolve();assert not out.exists()
    build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
    records=build['Inputs']+build['Objects']+build['Headers']
    assert all(sha(row['Path'])==row['Sha256'].lower() for row in records)
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    cases=('xlir_native_factory_getter','xlir_native_factory_getter_cleanup','xlir_nullable_getter','xlir_nullable_getter_alias',
           'xlir_nullable_generic_getter','xlir_nullable_inherited_getter','xlir_nullable_protocol_getter',
           'xlir_internal_property_call_abi')
    contract=root/'test/cases/xlir_native_factory_getter_contract.c'
    inputs={str(f):sha(f) for f in (Path(__file__).resolve(),manifest,driver,cc,contract,
           *[root/('test/cases/'+n+'.xl') for n in cases])}
    out.mkdir(parents=True);rows=[];products=[];environment=dict(os.environ)
    if os.name!='nt':assert a.native_root;environment['XL6_SOURCE_NATIVE_ROOT']=str(a.native_root.resolve())
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,
                SourceBuild=str(manifest),NetworkTests=False,SourceProductAcceptance=True,ShippingLibraryAcceptance=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,args):
        log=out/(name+'.log');args=list(map(str,args));started=time.monotonic()
        with log.open('wb') as stream:r=subprocess.run(args,cwd=root,env=environment,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
        rows.append(dict(Name=name,Command=args,Exit=r.returncode,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started));save()
        print(name+': '+str(r.returncode),flush=True);assert r.returncode==0,(name,str(log))
        return log.read_text(encoding='utf8',errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root/'release'),
           '-I'+str(root/'release/include'),'-I'+str(driver.parent/'include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-unused-label','-Wno-missing-field-initializers']
    save()
    try:
        for case in cases:
            source=root/('test/cases/'+case+'.xl');generated=out/(case+'.c');ir=out/(case+'.json')
            run(case+'.emit',[driver,source,'cgen-default',generated]);product(generated)
            run(case+'.ir',[driver,source,'ir-lowered',ir]);product(ir)
            for opt in ('O0','O2'):
                obj=out/(case+'.'+opt+'.o');run(case+'.'+opt,[cc,*flags,*compat,'-'+opt,'-c',generated,'-o',obj]);product(obj)
            for mode in ('run','module'):run(case+'.'+mode,[driver,source,mode,out/(case+'.'+mode)])
            prefix=out/(case+'.aot');binary=Path(str(prefix)+('.exe' if os.name=='nt' else '.out'))
            arg=prefix if os.name=='nt' else binary
            run(case+'.aot.build',[driver,source,'exe',arg]);product(binary);run(case+'.aot.run',[binary])
            imports=run(case+'.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
            assert not any(name in imports.lower() for name in ('xl.dll','xlc.dll','libxl.so','libxlc.so'))
            if case=='xlir_native_factory_getter':
                native=Path(str(arg)+'.temps')/'xl6_xrt_impl.c';product(native);text=native.read_text()
                for module in ('NET_SOCKET','NET_DNS','NET_ENGINE','NET_RESOLVER','EXECUTOR','TASK_POOL','TLS'):
                    assert '#define XRT_MODULE_'+module+'\n' not in text
        objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in
                 ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
        assert len(objects)==len(build['Objects'])-1
        for opt in ('O0','O2'):
            obj=out/('contract.'+opt+'.o');binary=out/('contract.'+opt+('.exe' if os.name=='nt' else ''))
            run('contract.'+opt+'.build',[cc,*flags,'-'+opt,'-c',contract,'-o',obj]);product(obj)
            if os.name=='nt':
                link=[*objects,obj,root/'release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32','-o',binary]
                if not (out/'xl.dll').exists():shutil.copy2(root/'release/xl.dll',out/'xl.dll');product(out/'xl.dll')
            else:
                lib=a.native_root.resolve()/'release'
                link=[*objects,obj,'-L'+str(lib),'-lxl','-Wl,-rpath,'+str(lib),'-Wl,--export-dynamic','-pthread','-ldl','-lm','-o',binary]
            response=out/(opt+'.rsp');response.write_text('\n'.join('"'+str(v).replace('\\','/')+'"' for v in link)+'\n',encoding='utf8');product(response)
            run('contract.'+opt+'.link',[cc,'@'+str(response)]);product(binary)
            text=run('contract.'+opt+'.run',[binary,root/'test/cases/xlir_native_factory_getter.xl']);assert 'Native getter XLIR:' in text
        report['InputsUnchanged']=all(sha(f)==h for f,h in inputs.items()) and all(sha(row['Path'])==row['Sha256'].lower() for row in records)
        report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
        assert report['InputsUnchanged'] and report['LogsUnchanged'] and all(sha(row['Path'])==row['Sha256'] for row in products)
        report['Complete']=True
    except BaseException as error:report['Error']=repr(error);raise
    finally:save()

if __name__=='__main__':main()
