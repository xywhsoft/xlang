"""Actual-source immutable TLS metadata contracts and JIT/module/standalone AOT.

Ordinary fixed-key self-loopback only. This is not a shipping or security gate.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['root','build','out']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    build=json.loads(a.build.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['SourceXrt'] and not build['RuntimeOnly'] and build['Optimization']=='O2'
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    env=dict(os.environ);runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    sys.path.insert(0,str(Path(__file__).resolve().parent));from tls_metadata_fixtures import fixtures
    mainCase,empty=fixtures(root,out)
    negatives={
        'info_raw_import':'auto info=TlsConnectionInfo(point(null))\n',
        'stats_raw_import':'auto stats=TlsServerStats(point(null))\n',
        'info_private_handle':'func expose(TlsConnectionInfo info) as point\n return info.handle\nend\n',
        'info_immutable':'func change(TlsConnectionInfo info)\n info.ready=false\nend\n',
        'stats_immutable':'func change(TlsServerStats stats)\n stats.accepted=0\nend\n',
        'role_nominal':'func change(TlsConnectionInfo info) as TlsRole\n return info.role | info.state\nend\n'}
    negativePaths=[]
    for name,body in negatives.items():
        path=out/('reject_'+name+'.xl');path.write_text(body,encoding='utf8');negativePaths.append(path)
    contracts=[root/'test/cases/xlir_tls_metadata_contract.c',root/'test/cases/xlir_tls_public_contract.c']
    files={Path(__file__).resolve(),a.build.resolve(),gcc,mainCase,empty,*negativePaths,*contracts}
    files.update(Path(m.__file__) for m in sys.modules.values() if getattr(m,'__file__',None) and Path(m.__file__).is_relative_to(Path(__file__).resolve().parent))
    files.update(q for q in out.iterdir() if q.is_file())
    r=dict(Complete=False,SourceBuild=str(a.build),SourceBuildSha256=sha(a.build),Inputs={str(q):sha(q) for q in sorted(files)},Rows=[],Products=[],Failures=[],
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
    try:
        # Discover semantic/IR/C problems for the whole batch before runtime.
        for index,case in enumerate([mainCase,empty]):
            label=str(index);c=out/(label+'.c');run(label+'.emit',[driver,case,'cgen-default',c]);text=c.read_text(encoding='utf8')
            assert not any(q in text for q in ['XL6_TlsLease','xl6_xrt_tls_stream_','xl6_xrt_tls_server_'])
            assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Tls(?:Info|ServerStats|StreamSnapshot|ListenerSnapshot)\w+\([^;]*\)\s*\{',text,re.M)
            if case==mainCase:assert 'xl6TlsStreamSnapshotAsyncV1(' in text and 'xl6TlsInfoCertificateAtV1(' in text
            for mode in ['typed','lowered']:run(label+'.'+mode,[driver,case,'ir-'+mode,out/(label+'.'+mode+'.json')])
            for opt in ['O0','O2']:run(label+'.strict.'+opt,[gcc,*flags,'-'+opt,'-c',c,'-o',out/(label+'.'+opt+'.o')])
        objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ['xlir_public_source_driver.c','xlir_public_source_linux_driver.c']]
        assert len(objects)==len(build['Objects'])-1
        libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)]
        for number,contract in enumerate(contracts):
            for opt in ['O0','O2']:
                label='contract'+str(number)+'.'+opt;obj=out/(label+'.o');binary=out/(label+('.exe' if os.name=='nt' else ''))
                run(label+'.compile',[gcc,*flags,'-DXL_STATIC_EMBED','-'+opt,'-c',contract,'-o',obj])
                run(label+'.link',[gcc,*objects,obj,runtime,*libraries,'-o',binary])
                expected='TLS metadata XLIR: 42 targets;' if number==0 else 'public TLS XLIR: 167 targets;'
                assert expected in run(label+'.run',[binary,empty])
        for path in negativePaths:
            body=run(path.stem,[driver,path,'cgen-default',out/(path.stem+'.c')],1)
            prefix='public source compile failed:' if os.name=='nt' else 'public Linux source cgen-default failed:'
            assert prefix in body and 'without diagnostic' not in body and not (out/(path.stem+'.c')).exists()
        for index,case in enumerate([mainCase,empty]):
            label=str(index)
            for mode in ['run','module']:
                observed=run(label+'.'+mode,[driver,case,mode,out/(label+'.'+mode)])
                if case==mainCase:assert 'TLS metadata: immutable owned snapshots, certificates, stats and Future: ok' in observed
            binary=out/(label+'.aot'+('.exe' if os.name=='nt' else ''));target=binary.with_suffix('') if os.name=='nt' else binary
            run(label+'.aot.build',[driver,case,'exe',target]);run(label+'.aot.run',[binary])
            imports=run(label+'.aot.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
            assert not any(q in imports.lower() for q in ['xl.dll','xlc.dll','libxl.so','libxlc.so'])
            bundle=(Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text(encoding='utf8')
            assert '_MODULE_ALL' not in bundle
            # Observing connection state must not enable ticket machinery.
            assert not re.search(r'^\s*#\s*define\s+(?:XRT|XRUNTIME)_(?:MODULE|FEATURE)_TLS_(?:CLIENT|SERVER)_RESUME\b',bundle,re.M)
            if case==empty:assert not any(q in bundle for q in ['XRT_MODULE_TLS_', 'XRT_MODULE_NET_TCP','xl6_tls_info','xl6_tls_server_stats'])
        r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items()) and all(sha(q['Path'])==q['Sha256'].lower() for q in build['Inputs']+build['Objects']+build['Headers'])
        r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json']
        r['Complete']=r['InputsUnchanged'];r['MetadataLanguageApiAccepted']=r['Complete'];r['Stages']=len(r['Rows']);save();assert r['Complete']
    except Exception as e:r['Failures'].append(repr(e));save();raise
if __name__=='__main__':main()
