"""Source-compiler TLS identity APIs, exact ownership contracts and pruning.
Ordinary offline fixed keys/certificates only; not TLS session or shipping proof.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
from tls_identity_language_fixtures import language_fixtures

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-build',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    root=Path(__file__).resolve().parents[1];out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest();build=json.loads(a.source_build.read_text(encoding='utf-8-sig'))
    assert build.get('Complete',True) and build['SourceXrt'] and not build['RuntimeOnly']
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower();gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    fixtureDir=out/'fixtures';fixtureDir.mkdir();cases,contract=language_fixtures(root,fixtureDir)
    paths=[Path(__file__),gcc,*[root/'tools'/name for name in ('tls_identity_language_fixtures.py','tls_identity_fixtures.py','x509_identity_language_fixtures.py','x509_identity_fixtures.py','check_key_format_wire.py')],
        *[root/'test/support'/('crypto_'+name+'_runtime.c') for name in ('rsa','nist','ed25519','xdh')],root/'test/cases/xlir_tls_identity_contract.c',root/'test/support/container_projection_artifact.c']
    r=dict(Complete=False,Rows=[],Failures=[],Plans=[],ResidentEntries=8,SourceBuild=str(a.source_build),Inputs={str(f):sha(f) for f in paths},
        NormalFamilies=4,EverySignatureIndependentlyVerified=True,NetworkTraffic=False,SecurityTests=False,MalformedCryptoTests=False,
        TrustAcceptance=False,TlsAcceptance=False,ShippingAcceptance=False)
    env=dict(os.environ);runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
    def run(name,args,reject=False):
        args=list(map(str,args));command=args
        if args[0]==str(gcc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log')
        with log.open('wb') as stream:code=subprocess.run(command,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=300).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log)));save();print(name,code,flush=True)
        assert code==(1 if reject else 0),(name,str(log));return log.read_text(errors='replace')
    flags=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include')]+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
    strict=['-Wall','-Wextra','-Werror','-fmax-errors=4'];suffix='.exe' if os.name=='nt' else ''
    compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-unused-label','-Wno-unused-const-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    for source in cases:
        name=source.stem
        try:
            generated=out/(name+'.c');run(name+'.emit',[driver,source,'cgen-default',generated]);text=generated.read_text()
            assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Tls\w+\([^;]*\)\s*\{',text,re.M)
            assert 'xl6_tls_config_create' not in text and 'xl6_tls_session_create' not in text
            run(name+'.jit',[driver,source,'run',out/(name+'.jit')])
            for stage in ('typed','lowered'):run(name+'.'+stage,[driver,source,'ir-'+stage,out/(name+'.'+stage+'.json')])
            for opt in ('O0','O2'):run(name+'.'+opt,[gcc,*flags,*strict,*compat,'-'+opt,'-c',generated,'-o',out/(name+'.'+opt+'.o')])
            run(name+'.module',[driver,source,'module',out/(name+'.module')]);binary=out/(name+'.aot'+suffix);target=binary.with_suffix('') if os.name=='nt' else binary
            run(name+'.aot.build',[driver,source,'exe',target]);run(name+'.aot.run',[binary])
            imports=run(name+'.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
            assert not any(v in imports.lower() for v in ('xl.dll','xlc.dll','libxl.so','libxlc.so'))
            assert '_MODULE_ALL' not in (Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text()
        except Exception as error:r['Failures'].append(dict(Name=name,Error=repr(error)));save()
    objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')];assert len(objects)==len(build['Objects'])-1
    libs=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)])]
    for opt in ('O0','O2'):
        try:
            obj=out/('contract.'+opt+'.o');binary=out/('contract.'+opt+suffix)
            run('contract.'+opt+'.compile',[gcc,*flags,*strict,'-'+opt,'-c',root/'test/cases/xlir_tls_identity_contract.c','-o',obj])
            run('contract.'+opt+'.link',[gcc,*objects,obj,*libs,'-o',binary]);assert 'TLS identity XLIR:' in run('contract.'+opt+'.run',[binary,contract])
        except Exception as error:r['Failures'].append(dict(Name='contract.'+opt,Error=repr(error)));save()
    try:
        obj=out/'plan.o';binary=out/('plan'+suffix);run('plan.compile',[gcc,*flags,*strict,'-O0','-c',root/'test/support/container_projection_artifact.c','-o',obj]);run('plan.link',[gcc,*objects,obj,*libs,'-o',binary])
        for source in cases:
            prefix=out/(source.stem+'.selected');run(source.stem+'.plan',[binary,source,prefix]);plan=json.loads(Path(str(prefix)+'.plan.json').read_text())
            modules=plan['NativeModules'];sources=plan['Sources']
            forbidden=('_MODULE_ALL','TLS_SESSION','TLS_STREAM','TLS_CLIENT','TLS_SERVER','TLS_VERIFY','X509_VERIFY','X509_PATH','X509_STORE','NET_SOCKET','NET_DNS','NET_ENGINE')
            assert not any(any(word in module for word in forbidden) for module in modules)
            if source.stem in ('tls.identity.rsa','tls.identity.p256','tls.identity.p384','tls.identity.ed25519'):
                family=source.stem.rsplit('.',1)[-1].upper()
                for candidate in ('RSA','P256','P384','ED25519'):
                    assert ('XRT_MODULE_TLS_IDENTITY_'+candidate in modules)==(candidate==family)
                    assert any('xl6_tls_identity_'+candidate.lower()+'.c' in name for name in sources)==(candidate==family)
            if source.stem=='tls.identity.enums':
                assert not any('TLS_IDENTITY' in module for module in modules)
                assert not any('/xl6_tls_' in name for name in sources)
            r['Plans'].append(dict(Name=source.stem,**plan));save()
    except Exception as error:r['Failures'].append(dict(Name='pruning',Error=repr(error)));save()
    negatives={'key-owner':'func bad(array<Certificate> chain,P256Key key)\n auto x=TlsIdentity.fromRsa(chain,key)\nend\n',
        'chain-type':'func bad(array<bytes> chain,P256Key key)\n auto x=TlsIdentity.fromP256(chain,key)\nend\n',
        'private':'func bad(TlsIdentity identity)\n auto h=identity.handle\nend\n',
        'result-type':'func bad(TlsIdentity identity) as point\n return identity\nend\n'}
    for name,body in negatives.items():
        try:
            source=fixtureDir/('bad.'+name+'.xl');source.write_text(body);target=out/('bad.'+name+'.c');message=run('bad.'+name,[driver,source,'cgen-default',target],True)
            assert not target.exists() and any(v in message for v in ('[sem]','[ir]','[checked]','[stdlib]')) and '[parse]' not in message
        except Exception as error:r['Failures'].append(dict(Name='bad.'+name,Error=repr(error)));save()
    r['InputsUnchanged']=all(sha(f)==digest for f,digest in r['Inputs'].items()) and all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers']) and sha(driver)==build['DriverSha256'].lower()
    r['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in r['Rows']);r['Products']=[dict(Path=str(f),Sha256=sha(f)) for f in out.rglob('*') if f.is_file() and f.name!='results.json']
    r['Complete']=not r['Failures'] and r['InputsUnchanged'] and r['LogsUnchanged'];save();assert r['Complete'],r['Failures']
if __name__=='__main__':main()
