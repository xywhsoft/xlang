"""Actual-source owning TLS peer predicates: IR, C, JIT/module/AOT and leases.
Normal offline trust only; no transport, malformed crypto or security tests.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['root','build','out']:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--fixtures',type=Path);p.add_argument('--preflight-only',action='store_true')
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    build=json.loads(a.build.read_text(encoding='utf-8-sig'));assert build['Complete'] and build['InputsUnchanged'] and build['SourceXrt'] and not build['RuntimeOnly'] and build['Optimization'] in ['O0','O2']
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    controls=Path(__file__).resolve().parents[1];sys.path.insert(0,str(controls/'tools'))
    fixtureDir=a.fixtures.resolve() if a.fixtures else out/'fixtures'
    if a.fixtures:
        cases=[fixtureDir/('tls.peer-policy.'+s+'.xl') for s in ['rsa','p256','p384','ed25519','reject','throw','cycle']]
        empty=fixtureDir/'tls.peer-policy.empty.xl'
    else:
        fixtureDir.mkdir();from tls_peer_policy_fixtures import fixtures;cases,empty=fixtures(root,fixtureDir)
    cases.append(empty)
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    contract=controls/'test/cases/xlir_tls_peer_policy_contract.c'
    callbackContract=controls/'test/cases/xlir_native_class_callback.c'
    negativeBodies={
        'peer_raw':'auto value=TlsVerifiedPeer(point(null))\n',
        'peer_write':'func change(TlsVerifiedPeer peer)\n peer.verificationName="changed"\nend\n',
        'callback_arity':'func bad(TrustStoreSnapshot trust,TlsVerificationPolicy policy) as TlsVerifier\n return TlsVerifier(trust,policy,func() as bool\n return true\n end)\nend\n',
        'callback_result':'func bad(TrustStoreSnapshot trust,TlsVerificationPolicy policy) as TlsVerifier\n return TlsVerifier(trust,policy,func(TlsVerifiedPeer peer) as int\n return 1\n end)\nend\n',
        'callback_nullable':'func bad(TrustStoreSnapshot trust,TlsVerificationPolicy policy) as TlsVerifier\n return TlsVerifier(trust,policy,func(TlsVerifiedPeer peer) as bool?\n return null\n end)\nend\n'}
    negatives=[]
    for name,body in negativeBodies.items():
        path=out/('reject_'+name+'.xl');path.write_text(body,encoding='utf8');negatives.append(path)
    files={Path(__file__).resolve(),a.build.resolve(),gcc,contract,callbackContract,*cases,*negatives}
    files.update(q for q in fixtureDir.rglob('*') if q.is_file())
    files.update(Path(m.__file__) for m in sys.modules.values() if getattr(m,'__file__',None) and Path(m.__file__).is_relative_to(controls/'tools'))
    r=dict(Complete=False,SourceBuild=str(a.build),SourceBuildSha256=sha(a.build),Inputs={str(q):sha(q) for q in sorted(files)},Rows=[],Products=[],Failures=[],
        NetworkTraffic=False,NetworkSecurityTests=False,MalformedCryptoTests=False,PublicSourcePeerPredicateAccepted=False,ShippingAccepted=False,FullTlsAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def run(name,args,expected=0):
        args=list(map(str,args));command=args
        if Path(args[0])==gcc:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n',encoding='utf8');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=900).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,Expected=expected,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert code==expected,str(log);return log.read_text(encoding='utf8',errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-fmax-errors=4','-DXL_STATIC_EMBED',
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-missing-field-initializers',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include'),*(['-D_GNU_SOURCE'] if os.name!='nt' else [])]
    # Discover the batch's semantic/IR/generated-C defects before execution.
    for case in cases:
        name=case.stem
        try:
            generated=out/(name+'.c');run(name+'.emit',[driver,case,'cgen-default',generated]);text=generated.read_text(encoding='utf8')
            assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Tls\w+\([^;]*\)\s*\{',text,re.M)
            if case!=empty:assert 'XL6_NativeClassResultContextV1' in text and 'xl6TlsVerifierCreateCallbackV1(' in text
            for stage in ['typed','lowered']:run(name+'.'+stage,[driver,case,'ir-'+stage,out/(name+'.'+stage+'.json')])
            for opt in ['O0','O2']:run(name+'.strict.'+opt,[gcc,*flags,'-'+opt,'-c',generated,'-o',out/(name+'.'+opt+'.o')])
        except Exception as e:r['Failures'].append(dict(Name=name,Error=repr(e)));save()
    for path in negatives:
        try:
            log=run(path.stem,[driver,path,'cgen-default',out/(path.stem+'.c')],1)
            assert 'without diagnostic' not in log and not (out/(path.stem+'.c')).exists()
        except Exception as e:r['Failures'].append(dict(Name=path.stem,Error=repr(e)));save()
    if not r['Failures'] and not a.preflight_only:
        for case in cases:
            name=case.stem
            try:
                for mode in ['run','module']:run(name+'.'+mode,[driver,case,mode,out/(name+'.'+mode)])
                binary=out/(name+'.aot'+('.exe' if os.name=='nt' else ''));target=binary.with_suffix('') if os.name=='nt' else binary
                run(name+'.aot.build',[driver,case,'exe',target]);run(name+'.aot.run',[binary])
                imports=run(name+'.aot.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
                assert not any(s in imports.lower() for s in ['xl.dll','xlc.dll','libxl.so','libxlc.so'])
                bundle=(Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text(encoding='utf8');assert '_MODULE_ALL' not in bundle
                if case==empty:assert not any(s in bundle for s in ['xl6_tls_peer_policy','xl6_native_class_callback','XRT_MODULE_TLS_'])
            except Exception as e:r['Failures'].append(dict(Name=name,Error=repr(e)));save()
        objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ['xlir_public_source_driver.c','xlir_public_source_linux_driver.c']]
        libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)]
        for opt in ['O0','O2']:
            try:
                obj=out/('contract.'+opt+'.o');binary=out/('contract.'+opt+('.exe' if os.name=='nt' else ''))
                run('contract.'+opt+'.compile',[gcc,*flags,'-'+opt,'-c',contract,'-o',obj]);run('contract.'+opt+'.link',[gcc,*objects,obj,runtime,*libraries,'-o',binary])
                assert 'TLS peer predicate XLIR: exact bool/class/arity/borrow/context/physical/error;' in run('contract.'+opt+'.run',[binary,cases[1]])
            except Exception as e:r['Failures'].append(dict(Name='contract.'+opt,Error=repr(e)));save()
        for opt in ['O0','O2']:
            try:
                label='callback.'+opt;obj=out/(label+'.o');binary=out/(label+('.exe' if os.name=='nt' else ''))
                run(label+'.compile',[gcc,*flags,'-'+opt,'-c',callbackContract,'-o',obj]);run(label+'.link',[gcc,*objects,obj,runtime,*libraries,'-o',binary])
                assert '256 OOM prefixes: ok' in run(label+'.run',[binary])
            except Exception as e:r['Failures'].append(dict(Name='callback.'+opt,Error=repr(e)));save()
    r['InputsUnchanged']=all(sha(q)==h for q,h in r['Inputs'].items())
    r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json']
    r['Stages']=len(r['Rows']);r['PreflightOnly']=a.preflight_only;r['Complete']=r['InputsUnchanged'] and not r['Failures']
    r['PublicSourcePeerPredicateAccepted']=r['Complete'] and not a.preflight_only;save();assert r['Complete'],r['Failures']
if __name__=='__main__':main()
