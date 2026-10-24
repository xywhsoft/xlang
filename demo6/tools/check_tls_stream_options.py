"""Configuration/ABI/ownership/pruning only: no sockets or TLS handshakes.

The resident subset is built without any TLS/TCP backend. With --source-build,
also use a genuine full-source compiler for XLIR, JIT, module and standalone
AOT, and compare defaults with the actual backend initializer.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--source-build',type=Path);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    sha=lambda q:hashlib.sha256(Path(q).read_bytes()).hexdigest()
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();tcc=Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve()
    paths=[Path(__file__),gcc,tcc,*root.glob('src/runtime/*.h'),*root.glob('src/runtime/*.inc'),*root.glob('src/compile/*.h'),*root.glob('src/compile/*.inc'),
        root/'src/compile/xl6_compile_unit.c',root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h',root/'release/include/xrt_decl.h',root/'release/include/xruntime_decl.h',
        *[root/'test/support'/('tls_stream_options_'+n+'.c') for n in ['plan','owned','defaults']],root/'test/cases/xlir_tls_stream_options_contract.c']
    cases=[root/'test/cases'/('stdlib_tls_stream_options'+s+'.xl') for s in ['', '_owned']];paths+=cases
    r=dict(Complete=False,Inputs={str(q):sha(q) for q in paths},Rows=[],Failures=[],Plans=[],NetworkTraffic=False,SecurityTests=False,MalformedCryptoTests=False,ShippingAccepted=False,FullTlsAccepted=False)
    env=dict(os.environ)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
    def run(name,args,reject=False):
        args=list(map(str,args));command=args
        if args[0] in [str(gcc),str(tcc)]:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=900 if name.startswith('contracts.') else 300).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,ExpectedReject=reject,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert (code!=0 if reject else code==0),str(log);return log.read_text(errors='replace')
    flags=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release')]+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
    strict=['-Wall','-Wextra','-Werror','-fmax-errors=4']
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    # Match the real declaration-only package instead of accidentally finding
    # an implementation .inc through the complete repository include path.
    sdk=out/'sdk';sdk.mkdir()
    for header in (root/'src/runtime').glob('*.h'):shutil.copy2(header,sdk/header.name)
    probe=out/'sdk.c';probe.write_text('#include "xl6_tls_stream_options_abi.h"\nint main(void){return 0;}\n')
    for name,cc in [('gcc',gcc),('tcc',tcc)]:
        run('sdk.'+name,[cc,'-DXL_STATIC_EMBED','-I'+str(sdk),'-c',probe,'-o',out/('sdk.'+name+'.o')])
    planner=out/('resident-plan'+('.exe' if os.name=='nt' else ''))
    run('resident.plan.build',[gcc,*flags,*strict,'-Wno-missing-field-initializers','-O0','-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables',
        root/'test/support/tls_stream_options_plan.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',planner])
    sources=[root/q for q in run('resident.plan.sources',[planner,'--sources']).splitlines()];modules=run('resident.plan.modules',[planner,'--modules'])
    assert not any(s in modules for s in ['_MODULE_ALL','MODULE_TLS_STREAM','MODULE_TLS_CLIENT','MODULE_TLS_SERVER','MODULE_TLS_VERIFY','MODULE_CRYPTO_','MODULE_NET_TCP','MODULE_NET_ENGINE','MODULE_NET_SOCKET'])
    r['Closure']=dict(Sources=list(map(str,sources)),Modules=modules)
    for q in sources:r['Inputs'][str(q)]=sha(q)
    impl=out/'native.c';impl.write_text(modules+'\n#define XRT_MODULE_MEMORY_DEBUG\n'+
        ('#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n' if '#define XRUNTIME_MODULE_' in modules else '#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n'))
    modes=[('gcc.O0',gcc,['-O0']),('gcc.O2',gcc,['-O2']),('tcc',tcc,[])]
    if os.name!='nt':modes.append(('asan',gcc,['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
    for name,cc,options in modes:
        try:
            binary=out/('resident.'+name+('.exe' if os.name=='nt' else ''))
            run('resident.'+name+'.build',[cc,*flags,*options,*(strict if cc==gcc else ()),root/'test/support/tls_stream_options_owned.c',impl,*sources,*libs,'-o',binary])
            assert 'TLS stream options owned:' in run('resident.'+name+'.run',[binary])
        except Exception as e:r['Failures'].append(dict(Name='resident.'+name,Error=repr(e)));save()
    build=None
    if a.source_build:
        build=json.loads(a.source_build.read_text(encoding='utf-8-sig'));assert build['Complete'] and build['SourceXrt'] and not build['RuntimeOnly'] and build['Optimization']=='O2'
        for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
        driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower();r['SourceBuild']=str(a.source_build);r['SourceBuildSha256']=sha(a.source_build)
        runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
        if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
        else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
        flags+=['-I'+str(root/'release/include'),'-I'+str(driver.parent/'include')]
        compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-unused-const-variable']
        for source in cases:
            name=source.stem
            try:
                generated=out/(name+'.c');run(name+'.emit',[driver,source,'cgen-default',generated]);text=generated.read_text()
                assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Tls\w+\([^;]*\)\s*\{',text,re.M)
                run(name+'.jit',[driver,source,'run',out/(name+'.jit')]);run(name+'.module',[driver,source,'module',out/(name+'.module')])
                for stage in ['typed','lowered']:run(name+'.'+stage,[driver,source,'ir-'+stage,out/(name+'.'+stage+'.json')])
                for opt in ['O0','O2']:run(name+'.'+opt,[gcc,*flags,*strict,*compat,'-'+opt,'-c',generated,'-o',out/(name+'.'+opt+'.o')])
                binary=out/(name+'.aot'+('.exe' if os.name=='nt' else ''));target=binary.with_suffix('') if os.name=='nt' else binary
                run(name+'.aot.build',[driver,source,'exe',target]);run(name+'.aot.run',[binary])
                imports=run(name+'.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary]);assert not any(q in imports.lower() for q in ['xl.dll','xlc.dll','libxl.so','libxlc.so'])
                assert '_MODULE_ALL' not in (Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text()
            except Exception as e:r['Failures'].append(dict(Name=name,Error=repr(e)));save()
        objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ['xlir_public_source_driver.c','xlir_public_source_linux_driver.c']];assert len(objects)==len(build['Objects'])-1
        nativeLibs=[runtime,*libs]+([] if os.name=='nt' else ['-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)])
        for name,source in [('contracts',root/'test/cases/xlir_tls_stream_options_contract.c'),('defaults',root/'test/support/tls_stream_options_defaults.c')]:
            for opt in ['O0','O2']:
                try:
                    obj=out/(name+'.'+opt+'.o');binary=out/(name+'.'+opt+('.exe' if os.name=='nt' else ''))
                    run(name+'.'+opt+'.compile',[gcc,*flags,*strict,*compat,'-'+opt,'-c',source,'-o',obj]);run(name+'.'+opt+'.link',[gcc,*objects,obj,*nativeLibs,'-o',binary])
                    assert ('TLS stream options XLIR:' if name=='contracts' else 'TLS stream options defaults:') in run(name+'.'+opt+'.run',[binary,*([cases[0]] if name=='contracts' else [])])
                except Exception as e:r['Failures'].append(dict(Name=name+'.'+opt,Error=repr(e)));save()
        try:
            obj=out/'plan.o';binary=out/('plan'+('.exe' if os.name=='nt' else ''));source=root/'test/support/container_projection_artifact.c';r['Inputs'][str(source)]=sha(source)
            run('plan.compile',[gcc,*flags,*strict,*compat,'-O0','-c',source,'-o',obj]);run('plan.link',[gcc,*objects,obj,*nativeLibs,'-o',binary])
            for source in cases:
                prefix=out/(source.stem+'.selected');run(source.stem+'.plan',[binary,source,prefix]);plan=json.loads(Path(str(prefix)+'.plan.json').read_text())
                assert not any(q.startswith('XRT_MODULE_TLS_') or 'XRT_MODULE_CRYPTO_' in q or q in ['XRT_MODULE_NET_TCP','XRT_MODULE_NET_ENGINE','XRT_MODULE_NET_SOCKET'] for q in plan['NativeModules'])
                assert 'src/runtime/xl6_tls_stream_options.c' in plan['Sources'] and not any('xl6_tls_session_' in q for q in plan['Sources'])
                r['Plans'].append(dict(Source=str(source),Plan=str(prefix)+'.plan.json',**plan))
        except Exception as e:r['Failures'].append(dict(Name='plans',Error=repr(e)));save()
        negatives={'private':'auto p=TlsStreamOptions().handle\n','bad-timeout':'auto p=TlsStreamOptions().withTimeoutsMillis("one",1)\n',
            'bad-bytes':'auto p=TlsStreamOptions().withAsyncLimits("one",1)\n','bad-batch':'auto p=TlsStreamOptions().withAsyncLimits(1,1,"one")\n'}
        for name,text in negatives.items():
            source=out/('bad.'+name+'.xl');source.write_text(text);target=out/('bad.'+name+'.c')
            try:
                message=run('bad.'+name,[driver,source,'cgen-default',target],True);assert not target.exists() and any(v in message for v in ['[sem]','[ir]','[checked]','[stdlib]']) and '[parse]' not in message
            except Exception as e:r['Failures'].append(dict(Name='bad.'+name,Error=repr(e)));save()
    r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items())
    if build:r['InputsUnchanged'] &= all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers']) and sha(driver)==build['DriverSha256'].lower()
    r['Stages']=len(r['Rows']);r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json']
    r['Complete']=not r['Failures'] and r['InputsUnchanged'];save();assert r['Complete'],r['Failures']

if __name__=='__main__':main()
