"""Actual-source default TLS facade: declarations, JIT, module, AOT and pruning.

Only ordinary literal loopback and the existing local CONNECT peer are used.
This does not certify the still-legacy WebSocket attachment or full TLS/P7.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time,queue,threading

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['root','build','out','peer']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    build=json.loads(a.build.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['SourceXrt'] and not build['RuntimeOnly'] and build['Optimization']=='O2'
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    env=dict(os.environ);runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    sys.path.insert(0,str(Path(__file__).resolve().parent));from tls_public_fixtures import fixtures
    direct,proxy,data=fixtures(root,out)
    namespace=out/'public_tls_namespace.xl'
    namespace.write_text(data.read_text(encoding='utf8').replace('TlsServer(engine,listen,serverContext())','tls.listen(engine,listen,serverContext())')
        .replace('TlsStream(engine,server.local(),clientContext(),"localhost")','tls.connect(engine,server.local(),clientContext(),"localhost")'),encoding='utf8')
    assert 'tls.listen(' in namespace.read_text() and 'tls.connect(' in namespace.read_text()
    asynchronous=out/'public_tls_namespace_async.xl'
    asynchronous.write_text(namespace.read_text(encoding='utf8').replace(
        'tls.connect(engine,server.local(),clientContext(),"localhost")',
        'await tls.connectAsync(engine,NetResolver(),"127.0.0.1",server.port,clientContext(),TlsDialOptions().withServerName("localhost").withVerifyName("localhost"))'),encoding='utf8')
    assert 'await tls.connectAsync(' in asynchronous.read_text()
    empty=out/'public_tls_declarations.xl';empty.write_text('TlsStream? stream = null\nTlsServer? server = null\nassert stream == null and server == null\n',encoding='utf8')
    negatives={
        'stream_raw_import':'auto stream = TlsStream(point(null))\n',
        'server_raw_import':'auto server = TlsServer(point(null))\n',
        'private_handle':'func expose(TlsStream stream) as point\n return stream.handle\nend\n',
        'borrowed_pointer':'func expose(TlsStream stream) as point\n return stream.nativeHandle()\nend\n',
        'legacy_config':'func mix(NetEngine engine, NetAddress remote, TlsConfig config)\n auto stream=TlsStream(engine, remote, config, "localhost")\nend\n'}
    negativePaths=[]
    for name,body in negatives.items():
        path=out/('reject_'+name+'.xl');path.write_text(body,encoding='utf8');negativePaths.append(path)
    contract=Path(__file__).resolve().parents[1]/'test/cases/xlir_tls_public_contract.c'
    files={Path(__file__).resolve(),contract,a.build,a.peer.resolve(),gcc,direct,proxy,data,namespace,asynchronous,empty,*negativePaths}
    files.update(Path(m.__file__) for m in sys.modules.values() if getattr(m,'__file__',None) and Path(m.__file__).is_relative_to(Path(__file__).resolve().parent))
    files.update(q for q in out.iterdir() if q.is_file())
    r=dict(Complete=False,SourceBuild=str(a.build),SourceBuildSha256=sha(a.build),Inputs={str(q):sha(q) for q in files},Rows=[],Products=[],Failures=[],
        OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,NetworkSecurityTests=False,MalformedCryptoTests=False,
        ShippingAccepted=False,FullTlsAccepted=False,LegacyWebSocketAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def run(name,command,expected=0):
        command=list(map(str,command));invocation=command
        if Path(command[0])==gcc:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in command[1:])+'\n',encoding='utf8');invocation=[command[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(invocation,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=900).returncode
        r['Rows'].append(dict(Name=name,Command=command,Exit=code,Expected=expected,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save()
        print(name,code,flush=True);assert code==expected,str(log);return log.read_text(encoding='utf8',errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-unused-const-variable',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include'),*(['-D_GNU_SOURCE'] if os.name!='nt' else [])]
    def checkC(path):
        text=path.read_text(encoding='utf8')
        for forbidden in ['XL6_TlsLease','xl6_xrt_tls_stream_','xl6_xrt_tls_server_','xl6_xrt_tls_connect','xl6_xrt_tls_listen']:
            assert forbidden not in text,(path,forbidden)
        assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Tls(?:Stream|Listener|Dial|ProxyDial)\w+\([^;]*\)\s*\{',text,re.M),path
    try:
        # Batch discovery before any expensive program execution: all public
        # sources must pass semantic/IR/C checks before JIT/module/AOT begins.
        cases=[direct,data,namespace,asynchronous,empty]
        for index,case in enumerate(cases):
            label=str(index)+'.'+case.stem;c=out/(label+'.c')
            run(label+'.emit',[driver,case,'cgen-default',c]);checkC(c)
            for mode in ['typed','lowered']:run(label+'.'+mode,[driver,case,'ir-'+mode,out/(label+'.'+mode+'.json')])
            for opt in ['O0','O2']:run(label+'.strict.'+opt,[gcc,*flags,'-'+opt,'-c',c,'-o',out/(label+'.'+opt+'.o')])
        objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ['xlir_public_source_driver.c','xlir_public_source_linux_driver.c']]
        assert len(objects)==len(build['Objects'])-1
        libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)]
        for opt in ['O0','O2']:
            obj=out/('contract.'+opt+'.o');binary=out/('contract.'+opt+('.exe' if os.name=='nt' else ''))
            run('contract.'+opt+'.compile',[gcc,*flags,'-DXL_STATIC_EMBED','-'+opt,'-c',contract,'-o',obj])
            run('contract.'+opt+'.link',[gcc,*objects,obj,runtime,*libraries,'-o',binary])
            assert 'public TLS XLIR: 164 targets;' in run('contract.'+opt+'.run',[binary,empty])
        for index,case in enumerate(cases):
            label=str(index)+'.'+case.stem;c=out/(label+'.c')
            for mode in ['run','module']:run(label+'.'+mode,[driver,case,mode,out/(label+'.'+mode)])
            binary=out/(label+'.aot'+('.exe' if os.name=='nt' else ''));target=binary.with_suffix('') if os.name=='nt' else binary
            run(label+'.aot.build',[driver,case,'exe',target]);run(label+'.aot.run',[binary])
            imports=run(label+'.aot.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
            assert not any(q in imports.lower() for q in ['xl.dll','xlc.dll','libxl.so','libxlc.so'])
            bundle=(Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text(encoding='utf8')
            assert '_MODULE_ALL' not in bundle
            if case==empty:
                assert not any(q in bundle for q in ['XRT_MODULE_TLS_', 'XRT_MODULE_NET_TCP', 'xl6_tls_stream_async.c', 'xl6_tls_dial_future.c'])
        for path in negativePaths:
            body=run(path.stem,[driver,path,'cgen-default',out/(path.stem+'.c')],1)
            prefix='public source compile failed:' if os.name=='nt' else 'public Linux source cgen-default failed:'
            assert prefix in body and 'without diagnostic' not in body,path
            assert not (out/(path.stem+'.c')).exists()
        for mode in ['run','module','exe']:
            peer=subprocess.Popen([str(a.peer),'--serve-proxy'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
            q=queue.Queue();threading.Thread(target=lambda:q.put(peer.stdout.readline()),daemon=True).start()
            try:
                port=int(q.get(timeout=15));assert 0<port<65536
                path=out/('public_proxy_'+mode+'.xl');path.write_text(proxy.read_text(encoding='utf8').replace('int __tls_proxy_port = 1','int __tls_proxy_port = '+str(port)),encoding='utf8')
                if mode=='exe':
                    target=out/'proxy.aot';run('proxy.aot.build',[driver,path,'exe',target]);run('proxy.aot.run',[str(target)+('.exe' if os.name=='nt' else '')])
                else:run('proxy.'+mode,[driver,path,mode,out/('proxy.'+mode)])
            finally:
                stdout,stderr=peer.communicate(b'\n',timeout=30);(out/('proxy.'+mode+'.peer.log')).write_bytes(stdout+stderr)
                assert peer.returncode==0,(peer.returncode,stdout,stderr)
        r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items())
        r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json']
        r['Complete']=r['InputsUnchanged'];r['PublicLanguageApiAccepted']=r['Complete'];r['Stages']=len(r['Rows']);save();assert r['Complete']
    except Exception as e:r['Failures'].append(repr(e));save();raise
if __name__=='__main__':main()
