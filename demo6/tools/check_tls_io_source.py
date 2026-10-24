"""Full-source transport ABI/XLIR/JIT/module/AOT integration.

Two ordinary fixed-key loopback handshakes per run; no external network, DNS,
malformed traffic or security tests. The default TLS facade is not certified.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time,sys

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--build',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    sha=lambda q:hashlib.sha256(Path(q).read_bytes()).hexdigest()
    build=json.loads(a.build.read_text(encoding='utf-8-sig'));assert build['Complete'] and build['InputsUnchanged'] and build['SourceXrt'] and not build['RuntimeOnly'] and build['Optimization']=='O2'
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    sys.path.insert(0,str(root/'tools'));from tls_io_fixtures import io_language_fixture
    case=io_language_fixture(root,out)
    contractCase=out/'resident_tls_io_contract.xl';contractCase.write_text(case.read_text().split('func clientContext()',1)[0]+'\nprintln("contracts")\n')
    files={Path(__file__).resolve(),gcc,case,contractCase,root/'test/cases/xlir_tls_io_contract.c'}
    files.update(Path(m.__file__) for m in sys.modules.values() if getattr(m,'__file__',None) and Path(m.__file__).is_relative_to(root/'tools'))
    files.update(q for q in out.iterdir() if q.is_file())
    r=dict(Complete=False,SourceBuild=str(a.build),SourceBuildSha256=sha(a.build),Inputs={str(q):sha(q) for q in sorted(files)},Rows=[],Products=[],Failures=[],
        OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,NetworkSecurityTests=False,MalformedCryptoTests=False,ShippingAccepted=False,FullTlsAccepted=False,PublicLanguageApiAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def run(name,command):
        command=list(map(str,command));invocation=command
        if Path(command[0])==gcc:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in command[1:])+'\n',encoding='utf8');invocation=[command[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(invocation,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=300).returncode
        r['Rows'].append(dict(Name=name,Command=command,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert not code,str(log);return log.read_text(encoding='utf8',errors='replace')
    try:
        generated=out/'transport.c';run('emit',[driver,case,'cgen-default',generated]);text=generated.read_text()
        assert all(name not in text for name in ['XL6_TlsLease','xl6_xrt_tls_stream_','xl6_tls_stream_dial','xl6_xrt_tls_server_'])
        assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Tls(?:Stream|Listener)\w+\([^;]*\)\s*\{',text,re.M)
        r['NoLegacyCgenBody']=True
        for mode in ['typed','lowered']:run(mode,[driver,case,'ir-'+mode,out/(mode+'.json')])
        for mode in ['run','module']:
            observed=run(mode,[driver,case,mode,out/mode]);assert 'resident TLS I/O: owned bytes, sync/async, class accept, cancellation and deadlines: ok' in observed
        flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include'),
            *(['-D_GNU_SOURCE'] if os.name!='nt' else [])]
        compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-unused-const-variable']
        for opt in ['O0','O2']:run('strict.'+opt,[gcc,*flags,*compat,'-'+opt,'-c',generated,'-o',out/('strict.'+opt+'.o')])
        binary=out/('transport.aot'+('.exe' if os.name=='nt' else ''));target=binary.with_suffix('') if os.name=='nt' else binary
        run('aot.build',[driver,case,'exe',target]);assert 'resident TLS I/O: owned bytes, sync/async, class accept, cancellation and deadlines: ok' in run('aot.run',[binary])
        imports=run('aot.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
        assert not any(s in imports.lower() for s in ['xl.dll','xlc.dll','libxl.so','libxlc.so'])
        assert '_MODULE_ALL' not in (Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text()
        objects=[q['Path'] for q in build['Objects'] if Path(q['Source']).name not in ['xlir_public_source_driver.c','xlir_public_source_linux_driver.c']];assert len(objects)==len(build['Objects'])-1
        libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)]
        for opt in ['O0','O2']:
            obj=out/('contracts.'+opt+'.o');binary=out/('contracts.'+opt+('.exe' if os.name=='nt' else ''))
            run('contracts.'+opt+'.compile',[gcc,*flags,'-DXL_STATIC_EMBED','-'+opt,'-c',root/'test/cases/xlir_tls_io_contract.c','-o',obj])
            run('contracts.'+opt+'.link',[gcc,*objects,obj,runtime,*libraries,'-o',binary])
            assert 'TLS IO XLIR: 76 targets;' in run('contracts.'+opt+'.run',[binary,contractCase])
        r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items()) and all(sha(q['Path'])==q['Sha256'].lower() for q in build['Inputs']+build['Objects']+build['Headers'])
        r['Stages']=len(r['Rows']);r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json']
        r['Complete']=r['InputsUnchanged'];save();assert r['Complete']
    except Exception as e:r['Failures'].append(repr(e));save();raise
if __name__=='__main__':main()
