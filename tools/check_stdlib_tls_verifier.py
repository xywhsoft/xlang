"""Actual source-compiler TLS verifier declarations, ownership and pruning.
Only ordinary offline signed certificates; no TLS transport/security campaign.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--source-build',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--policy-fixture',type=Path)
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    sys.path.insert(0,str(root/'tools'));from tls_verifier_fixtures import language_fixtures
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();build=json.loads(a.source_build.read_text(encoding='utf-8-sig'))
    assert build.get('Complete',True) and build['SourceXrt'] and not build['RuntimeOnly']
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();fixtureDir=out/'fixtures';fixtureDir.mkdir();cases,contract=language_fixtures(root,fixtureDir)
    originalPolicy=root/'test/cases/stdlib_tls_verification_policy.xl';policies=(a.policy_fixture or originalPolicy).resolve()
    if policies!=originalPolicy.resolve():
        original=originalPolicy.read_text();text=contract.read_text();assert text.endswith(original)
        contract.write_text(text[:-len(original)]+policies.read_text())
    regressions=[root/'test/cases'/n for n in ['stdlib_tls_parameters.xl','stdlib_tls_parameters_array_regression.xl']]
    cases+=[policies,*regressions]
    paths=[Path(__file__),gcc,root/'tools/tls_verifier_fixtures.py',root/'tools/x509_path_fixtures.py',root/'tools/check_key_format_wire.py',
        *[root/'test/support'/('crypto_'+n+'_runtime.c') for n in ['rsa','nist','ed25519','xdh']],root/'test/support/tls_verifier_owned.c',root/'test/cases/xlir_tls_verifier_contract.c',
        root/'test/support/container_projection_artifact.c',policies,*regressions]
    r=dict(Complete=False,Rows=[],Failures=[],Plans=[],Inputs={str(p):sha(p) for p in paths},SourceBuild=str(a.source_build),NetworkTraffic=False,SecurityTests=False,MalformedCryptoTests=False,ShippingAccepted=False,FullTlsAccepted=False)
    env=dict(os.environ);runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
    def run(name,args,reject=False):
        args=list(map(str,args));command=args
        if args[0]==str(gcc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=300).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,ExpectedReject=reject,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert (code!=0 if reject else code==0),str(log);return log.read_text(errors='replace')
    flags=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include'),'-I'+str(fixtureDir)]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    strict=['-Wall','-Wextra','-Werror','-fmax-errors=4'];compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-unused-const-variable','-Wno-pointer-sign']
    for source in cases:
        name=source.stem
        try:
            generated=out/(name+'.c');run(name+'.emit',[driver,source,'cgen-default',generated]);text=generated.read_text()
            assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Tls\w+\([^;]*\)\s*\{',text,re.M)
            assert 'xl6_tls_config_create' not in text
            run(name+'.jit',[driver,source,'run',out/(name+'.jit')]);run(name+'.module',[driver,source,'module',out/(name+'.module')])
            for stage in ['typed','lowered']:run(name+'.'+stage,[driver,source,'ir-'+stage,out/(name+'.'+stage+'.json')])
            for opt in ['O0','O2']:run(name+'.'+opt,[gcc,*flags,*strict,*compat,'-'+opt,'-c',generated,'-o',out/(name+'.'+opt+'.o')])
            binary=out/(name+'.aot'+('.exe' if os.name=='nt' else ''));target=binary.with_suffix('') if os.name=='nt' else binary
            run(name+'.aot.build',[driver,source,'exe',target]);run(name+'.aot.run',[binary])
            imports=run(name+'.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
            assert not any(v in imports.lower() for v in ['xl.dll','xlc.dll','libxl.so','libxlc.so'])
            assert '_MODULE_ALL' not in (Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text()
        except Exception as e:r['Failures'].append(dict(Name=name,Error=repr(e)));save()
    objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ['xlir_public_source_driver.c','xlir_public_source_linux_driver.c']]
    assert len(objects)==len(build['Objects'])-1
    libs=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)])]
    for name,source in [('contracts','test/cases/xlir_tls_verifier_contract.c'),('owned','test/support/tls_verifier_owned.c')]:
        for opt in ['O0','O2']:
            try:
                obj=out/(name+'.'+opt+'.o');binary=out/(name+'.'+opt+('.exe' if os.name=='nt' else ''))
                run(name+'.'+opt+'.compile',[gcc,*flags,*strict,*compat,'-'+opt,'-c',root/source,'-o',obj]);run(name+'.'+opt+'.link',[gcc,*objects,obj,*libs,'-o',binary])
                text=run(name+'.'+opt+'.run',[binary,*([contract] if name=='contracts' else [])]);assert ('TLS verifier XLIR:' if name=='contracts' else 'TLS verifier owned:') in text
            except Exception as e:r['Failures'].append(dict(Name=name+'.'+opt,Error=repr(e)));save()
    try:
        obj=out/'plan.o';binary=out/('plan'+('.exe' if os.name=='nt' else ''));run('plan.compile',[gcc,*flags,*strict,*compat,'-O0','-c',root/'test/support/container_projection_artifact.c','-o',obj]);run('plan.link',[gcc,*objects,obj,*libs,'-o',binary])
        for source in cases:
            prefix=out/(source.stem+'.selected');run(source.stem+'.plan',[binary,source,prefix]);plan=json.loads(Path(str(prefix)+'.plan.json').read_text());modules=plan['NativeModules'];sources=plan['Sources']
            assert not any('TLS_CLIENT' in m or 'TLS_SERVER' in m or 'TLS_STREAM' in m or 'TLS_SESSION' in m for m in modules)
            if source==policies:
                assert not any('XRT_MODULE_TLS_' in m or 'X509_' in m or 'CRYPTO_' in m for m in modules)
                assert not any('xl6_tls_verifier.c' in s or 'xl6_tls_verifier_create.c' in s or 'xl6_tls_verifier_verify.c' in s for s in sources)
            elif source.stem=='tls.verifier.configure':
                assert 'XRT_MODULE_TLS_VERIFY' in modules and not any(m in modules for m in ['XRT_MODULE_X509_VERIFY_RSA','XRT_MODULE_X509_VERIFY_ECDSA','XRT_MODULE_X509_VERIFY_ED25519'])
                assert not any('xl6_tls_verifier_verify.c' in s for s in sources)
            elif source not in regressions:assert all(m in modules for m in ['XRT_MODULE_X509_VERIFY_RSA','XRT_MODULE_X509_VERIFY_ECDSA','XRT_MODULE_X509_VERIFY_ED25519'])
            r['Plans'].append(dict(Source=str(source),Plan=str(prefix)+'.plan.json',NativeModules=modules,Sources=sources))
    except Exception as e:r['Failures'].append(dict(Name='plans',Error=repr(e)));save()
    negatives={'trust':'func bad(TlsVerificationPolicy policy)\n auto v=TlsVerifier(policy)\nend\n',
        'certificates':'func bad(TlsVerifier v,array<bytes> data)\n v.verifyServer(data,"localhost")\nend\n',
        'policy':'func bad(TrustStoreSnapshot trust,TlsLimits limits)\n auto v=TlsVerifier(trust,limits)\nend\n',
        'private':'func bad(TlsVerifier v)\n auto raw=v.handle\nend\n'}
    for name,text in negatives.items():
        source=out/('bad.'+name+'.xl');source.write_text(text);target=out/('bad.'+name+'.c')
        try:
            message=run('bad.'+name,[driver,source,'cgen-default',target],True)
            assert not target.exists() and any(v in message for v in ['[sem]','[ir]','[checked]','[stdlib]']) and '[parse]' not in message
        except Exception as e:r['Failures'].append(dict(Name='bad.'+name,Error=repr(e)));save()
    r['Stages']=len(r['Rows']);r['InputsUnchanged']=all(sha(p)==d for p,d in r['Inputs'].items()) and all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers']) and sha(driver)==build['DriverSha256'].lower()
    r['Products']=[dict(Path=str(p),Sha256=sha(p)) for p in out.rglob('*') if p.is_file() and p.name!='results.json']
    r['Complete']=not r['Failures'] and r['InputsUnchanged'];save();assert r['Complete'],r['Failures']

if __name__=='__main__':main()
