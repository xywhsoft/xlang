"""Verify managed TLS after deleting the old generated TLS library bodies.

Only fixed valid material, in-memory exchange and self-created literal loopback.
The Cgen unit selector covers bridge/carrier/erased-value contracts, not the
unrelated full Cgen catalog. This is not a shipping DLL/VFS/joint-release receipt.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from xlir_public_build_inputs import verify_public_build

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    build,pinned,_=verify_public_build(root,a.build.resolve().parent)
    assert build['Optimization']=='O2' and build['Complete'] and build['InputsUnchanged']
    controls=Path(__file__).resolve().parents[1]
    names=('stdlib_tls_session','stdlib_tls_session_trim','stdlib_tls_session_pair',
           'stdlib_tls_stream_fast_path','stdlib_tls_server_fast_path','stdlib_tls_server_loopback')
    cases=[controls/('test/cases/'+name+'.xl') for name in names]
    inputs=out/'inputs';inputs.mkdir();selected=[]
    for case in cases:
        destination=inputs/case.name;shutil.copy2(case,destination);selected.append(destination)
    cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();driver=Path(build['Driver'])
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    tests=[controls/('test/cases/'+name+'.c') for name in ('xlir_tls_retirement_contract','xlir_cgen')]
    ffi=controls/'test/cases/xlir_foreign_function_value_abi.xl'
    host=controls/'test/cases/xlir_foreign_function_value_abi_host.c'
    negatives={
        'mutable_config':'auto config=TlsConfig()\n',
        'raw_session':'auto session=TlsSession(point(null))\n',
        'raw_stream':'auto stream=TlsStream(point(null))\n',
        'raw_listener':'auto server=TlsServer(point(null))\n',
        'raw_handle':'export func leak(TlsStream stream) as point\n return stream.nativeHandle()\nend\n'}
    negative=[]
    for name,text in negatives.items():
        path=inputs/('reject_'+name+'.xl');path.write_text(text,encoding='utf8');negative.append(path)
    pinned.update((Path(__file__).resolve(),cc,runtime,*tests,ffi,host,*cases,*selected,*negative,
        controls/'test/cases/xlir_direct_bridge_unit.c'))
    forbidden=('xl6_xrt_tls_','XL6_TlsConfig','xl6ExtlibTlsLease','xlir_net_tls_stream_wrap_owned')
    sdk=controls/'release/include/xl_extlib_abi.h';pinned.add(sdk)
    assert 'XL6_TlsLease' not in sdk.read_text(encoding='utf8')
    for name in ('xl6_cgen_tls.c','xl6_cgen_tls_stream.c','xl6_cgen_tls.h'):
        assert not (root/'src/cgen'/name).exists()
    for directory in (root/'src',root/'release/include'):
        for path in directory.rglob('*'):
            if path.suffix not in ('.h','.c','.inc'):continue
            text=path.read_text(encoding='utf8')
            assert not any(token in text for token in forbidden),str(path)
    r=dict(Complete=False,Inputs={str(q):sha(q) for q in sorted(pinned)},Rows=[],Failures=[],
        RetiredTlsBodiesAbsent=True,OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,
        NetworkSecurityTests=False,ShippingAccepted=False,FullCgenUnitAccepted=False,
        WholeProjectStaticGuardAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def run(name,args,expected=0):
        args=list(map(str,args));command=args
        if args[0]==str(cc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+q.replace('\\','/')+'"' for q in args[1:]),encoding='utf8');command=[str(cc),'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as stream:code=subprocess.run(command,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=300).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,Expected=expected,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert code==expected,str(log);return log.read_text(encoding='utf8',errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable',
        '-Wno-unused-const-variable','-Wno-missing-field-initializers','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(driver.parent/'include'),*(['-D_GNU_SOURCE'] if os.name!='nt' else [])]
    objects=[q['Path'] for q in build['Objects'] if Path(q['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    links=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)]
    for case in selected:
        name=case.stem
        try:
            generated=out/(name+'.c');run(name+'.emit',[driver,case,'cgen-default',generated])
            text=generated.read_text(encoding='utf8');assert not any(token in text for token in forbidden)
            assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Tls\w*V1\([^;]*\)\s*\{',text,re.M)
            for stage in ('typed','lowered'):run(name+'.'+stage,[driver,case,'ir-'+stage,out/(name+'.'+stage+'.json')])
            for opt in ('O0','O2'):run(name+'.strict.'+opt,[cc,*flags,'-'+opt,'-c',generated,'-o',out/(name+'.'+opt+'.o')])
        except Exception as error:r['Failures'].append(dict(Name=name,Error=repr(error)));save()
    for case in negative:
        try:
            text=run(case.stem,[driver,case,'cgen-default',out/(case.stem+'.c')],1)
            assert 'without diagnostic' not in text and not (out/(case.stem+'.c')).exists()
        except Exception as error:r['Failures'].append(dict(Name=case.stem,Error=repr(error)));save()
    # Compile the remaining generic callable ABI with an ordinary user method,
    # not the deleted TLS callback/configuration special case.
    try:
        generated=out/'foreign.c';run('foreign.emit',[driver,ffi,'cgen-default',generated])
        for opt in ('O0','O2'):
            binary=out/('foreign.'+opt+('.exe' if os.name=='nt' else ''))
            run('foreign.'+opt+'.build',[cc,*flags,'-'+opt,'-DXLIR_GENERATED_MODULE_PREINCLUDED','-include',generated,host,*objects,runtime,*links,'-o',binary])
            run('foreign.'+opt+'.run',[binary])
    except Exception as error:r['Failures'].append(dict(Name='foreign',Error=repr(error)));save()
    if not r['Failures']:
        for case in selected:
            name=case.stem
            try:
                for mode in ('run','module'):run(name+'.'+mode,[driver,case,mode,out/(name+'.'+mode)])
                binary=out/(name+'.aot'+('.exe' if os.name=='nt' else ''));target=binary.with_suffix('') if os.name=='nt' else binary
                run(name+'.aot.build',[driver,case,'exe',target]);run(name+'.aot.run',[binary])
                imports=run(name+'.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
                assert not any(q in imports.lower() for q in ('xl.dll','xlc.dll','libxl.so','libxlc.so'))
                bundle=(Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text(encoding='utf8');assert '_MODULE_ALL' not in bundle
                if name in ('stdlib_tls_session','stdlib_tls_session_trim'):
                    assert 'XRT_MODULE_TLS_STREAM' not in bundle and 'XRT_MODULE_NET_ENGINE' not in bundle
            except Exception as error:r['Failures'].append(dict(Name=name,Error=repr(error)));save()
        for index,test in enumerate(tests):
            for opt in ('O0','O2'):
                name=test.stem+'.'+opt;obj=out/(name+'.o');binary=out/(name+('.exe' if os.name=='nt' else ''))
                try:
                    selector=['-DXL6_IR_CGEN_RUNTIME_BRIDGE_CONTRACT_ONLY'] if index==1 else []
                    run(name+'.compile',[cc,*flags,'-'+opt,*selector,'-c',test,'-o',obj]);run(name+'.link',[cc,obj,*objects,runtime,*links,'-o',binary])
                    assert 'ok' in run(name+'.run',[binary,*(selected if index==0 else [])])
                except Exception as error:r['Failures'].append(dict(Name=name,Error=repr(error)));save()
    r['InputsUnchanged']=all(sha(q)==h for q,h in r['Inputs'].items());r['Stages']=len(r['Rows'])
    r['Complete']=r['InputsUnchanged'] and not r['Failures']
    r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json'];save();assert r['Complete'],r['Failures']
if __name__=='__main__':main()
