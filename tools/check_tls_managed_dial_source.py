"""Full-source transport ABI/XLIR/JIT/module/AOT integration.

Two ordinary fixed-key loopback handshakes per run; no external network, DNS,
malformed traffic or security tests. The default TLS facade is not certified.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time,sys,queue,threading

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--build',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--peer',type=Path,required=True)
    p.add_argument('--fixtures',type=Path,help='Separately versioned test helpers; production build remains frozen')
    p.add_argument('--skip-contracts',action='store_true',help='Runtime-only retest; prior unchanged IR evidence must be independently verified');a=p.parse_args()
    root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    sha=lambda q:hashlib.sha256(Path(q).read_bytes()).hexdigest()
    build=json.loads(a.build.read_text(encoding='utf-8-sig'));assert build['Complete'] and build['InputsUnchanged'] and build['SourceXrt'] and not build['RuntimeOnly'] and build['Optimization']=='O2'
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    testTools=(a.fixtures or root/'tools').resolve()
    sys.path.insert(0,str(testTools));from tls_dial_fixtures import language_fixtures
    case,proxyCase,contractCase=language_fixtures(root,out)

    files={Path(__file__).resolve(),gcc,case,proxyCase,contractCase,a.peer.resolve(),root/'test/cases/xlir_tls_managed_dial_contract.c'}
    files.update(Path(m.__file__) for m in sys.modules.values() if getattr(m,'__file__',None) and (Path(m.__file__).is_relative_to(root/'tools') or Path(m.__file__).is_relative_to(testTools)))
    files.update(q for q in out.iterdir() if q.is_file())
    r=dict(Complete=False,SourceBuild=str(a.build),SourceBuildSha256=sha(a.build),Inputs={str(q):sha(q) for q in sorted(files)},Rows=[],Products=[],Failures=[],
        OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,NetworkSecurityTests=False,MalformedCryptoTests=False,ShippingAccepted=False,FullTlsAccepted=False,PublicLanguageApiAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def run(name,command):
        command=list(map(str,command));invocation=command
        if Path(command[0])==gcc:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in command[1:])+'\n',encoding='utf8');invocation=[command[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(invocation,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=900).returncode
        r['Rows'].append(dict(Name=name,Command=command,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert not code,str(log);return log.read_text(encoding='utf8',errors='replace')
    try:
        generated=out/'transport.c';run('emit',[driver,case,'cgen-default',generated]);text=generated.read_text()
        assert all(name not in text for name in ['XL6_TlsLease','xl6_xrt_tls_stream_','xl6_tls_stream_dial','xl6_xrt_tls_server_'])
        assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Tls(?:Stream|Listener)\w+\([^;]*\)\s*\{',text,re.M)
        r['NoLegacyCgenBody']=True
        for mode in ['typed','lowered']:run(mode,[driver,case,'ir-'+mode,out/(mode+'.json')])
        for mode in ['run','module']:
            observed=run(mode,[driver,case,mode,out/mode]);assert 'resident TLS dial: 20 direct signatures, names, owned results and cancellation: ok' in observed
        flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include'),
            *(['-D_GNU_SOURCE'] if os.name!='nt' else [])]
        compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-unused-const-variable']
        for opt in ['O0','O2']:run('strict.'+opt,[gcc,*flags,*compat,'-'+opt,'-c',generated,'-o',out/('strict.'+opt+'.o')])
        binary=out/('transport.aot'+('.exe' if os.name=='nt' else ''));target=binary.with_suffix('') if os.name=='nt' else binary
        run('aot.build',[driver,case,'exe',target]);assert 'resident TLS dial: 20 direct signatures, names, owned results and cancellation: ok' in run('aot.run',[binary])
        imports=run('aot.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
        assert not any(s in imports.lower() for s in ['xl.dll','xlc.dll','libxl.so','libxlc.so'])
        assert '_MODULE_ALL' not in (Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text()
        objects=[q['Path'] for q in build['Objects'] if Path(q['Source']).name not in ['xlir_public_source_driver.c','xlir_public_source_linux_driver.c']];assert len(objects)==len(build['Objects'])-1
        libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)]
        for opt in ([] if a.skip_contracts else ['O0','O2']):
            obj=out/('contracts.'+opt+'.o');binary=out/('contracts.'+opt+('.exe' if os.name=='nt' else ''))
            run('contracts.'+opt+'.compile',[gcc,*flags,'-DXL_STATIC_EMBED','-'+opt,'-c',root/'test/cases/xlir_tls_managed_dial_contract.c','-o',obj])
            run('contracts.'+opt+'.link',[gcc,*objects,obj,runtime,*libraries,'-o',binary])
            assert 'TLS dial XLIR: 54 targets;' in run('contracts.'+opt+'.run',[binary,contractCase])
        # A valid local CONNECT endpoint, never an external proxy/target.
        for mode in ['run','module','exe']:
            peer=subprocess.Popen([str(a.peer),'--serve-proxy'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,
                                  creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
            q=queue.Queue()
            threading.Thread(target=lambda:q.put(peer.stdout.readline()),daemon=True).start()
            try:
                port=int(q.get(timeout=15));assert 0<port<65536
                proxySource=out/('proxy.'+mode+'.xl')
                proxySource.write_text(proxyCase.read_text().replace('int __tls_proxy_port = 1','int __tls_proxy_port = '+str(port)),encoding='utf8')
                if mode=='exe':
                    target=out/'proxy.aot';run('proxy.aot.build',[driver,proxySource,'exe',target])
                    observed=run('proxy.aot.run',[str(target)+('.exe' if os.name=='nt' else '')])
                else:observed=run('proxy.'+mode,[driver,proxySource,mode,out/('proxy.'+mode)])
                assert 'resident TLS proxy dial: local CONNECT and owned TLS: ok' in observed
            finally:
                stdout,stderr=peer.communicate(b'\n',timeout=30)
                (out/('proxy.'+mode+'.peer.log')).write_bytes(stdout+stderr)
                assert peer.returncode==0,(peer.returncode,stdout,stderr)
        r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items()) and all(sha(q['Path'])==q['Sha256'].lower() for q in build['Inputs']+build['Objects']+build['Headers'])
        r['ContractChecksSkipped']=bool(a.skip_contracts)
        r['Stages']=len(r['Rows']);r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json']
        r['Complete']=r['InputsUnchanged'];save();assert r['Complete']
    except Exception as e:r['Failures'].append(repr(e));save();raise
if __name__=='__main__':main()
