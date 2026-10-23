"""Current-source identity calls, exact contracts, nullable ownership and pruning.
Only ordinary offline fixed-key certificates; no traffic or security suite.
Not complete trust validation, TLS or shipping acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
from x509_identity_language_fixtures import language_fixtures
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-build',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    root=Path(__file__).resolve().parents[1];out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest();build=json.loads(a.source_build.read_text(encoding='utf-8-sig'))
    assert build.get('Complete',True) and build['SourceXrt'] and not build['RuntimeOnly']
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower();gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    fixtureDir=out/'fixtures';fixtureDir.mkdir();cases,contract=language_fixtures(root,fixtureDir)
    declared=set(re.findall(r'XL6_TEXT_API [^;\n]*?\b(\w+)\(',(root/'src/runtime/xl6_x509_identity_abi.h').read_text()))
    exported=set(re.findall(r'XL6_TEXT_EXPORT\((\w+),',(root/'src/runtime/xl6_x509_identity_symbols.inc').read_text()))
    contracted=set(re.findall(r'^\{"(\w+)"',(root/'src/ir/xl6_ir_x509_identity_contracts.inc').read_text(),re.M));assert len(declared)==3 and declared==exported==contracted
    r=dict(Complete=False,Rows=[],Failures=[],Plans=[],ResidentEntries=3,SourceBuild=str(a.source_build),
        Inputs={str(f):sha(f) for f in [Path(__file__),gcc,root/'tools/x509_identity_language_fixtures.py',root/'tools/x509_identity_fixtures.py',root/'tools/check_key_format_wire.py',
            *[root/'test/support'/('crypto_'+n+'_runtime.c') for n in ('rsa','nist','ed25519','xdh')],root/'test/cases/xlir_x509_identity_contract.c',root/'test/support/container_projection_artifact.c']},
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
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log)));save();print(name,code,flush=True);assert code==(1 if reject else 0),(name,str(log));return log.read_text(errors='replace')
    flags=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include')]+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
    strict=['-Wall','-Wextra','-Werror','-fmax-errors=4'];compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-unused-label','-Wno-unused-const-variable','-Wno-missing-field-initializers','-Wno-pointer-sign'];suffix='.exe' if os.name=='nt' else ''
    for source in cases:
        name=source.stem
        try:
            generated=out/(name+'.c');run(name+'.emit',[driver,source,'cgen-default',generated]);text=generated.read_text()
            assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6X509\w+\([^;]*\)\s*\{',text,re.M)
            run(name+'.jit',[driver,source,'run',out/(name+'.jit')])
            for stage in ('typed','lowered'):run(name+'.'+stage,[driver,source,'ir-'+stage,out/(name+'.'+stage+'.json')])
            if name=='identity.dns':assert 'X509GeneralName' not in (out/(name+'.typed.json')).read_text()
            for opt in ('O0','O2'):run(name+'.'+opt,[gcc,*flags,*strict,*compat,'-'+opt,'-c',generated,'-o',out/(name+'.'+opt+'.o')])
            run(name+'.module',[driver,source,'module',out/(name+'.module')]);binary=out/(name+'.aot'+suffix);target=binary.with_suffix('') if os.name=='nt' else binary
            run(name+'.aot.build',[driver,source,'exe',target]);run(name+'.aot.run',[binary])
            imports=run(name+'.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary]);assert not any(v in imports.lower() for v in ('xl.dll','xlc.dll','libxl.so','libxlc.so'))
            assert '_MODULE_ALL' not in (Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text()
        except Exception as error:r['Failures'].append(dict(Name=name,Error=repr(error)));save()
    objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')];assert len(objects)==len(build['Objects'])-1
    libs=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)])]
    for opt in ('O0','O2'):
        try:
            obj=out/('contract.'+opt+'.o');binary=out/('contract.'+opt+suffix)
            run('contract.'+opt+'.compile',[gcc,*flags,*strict,'-'+opt,'-c',root/'test/cases/xlir_x509_identity_contract.c','-o',obj]);run('contract.'+opt+'.link',[gcc,*objects,obj,*libs,'-o',binary]);assert 'X509 identity XLIR:' in run('contract.'+opt+'.run',[binary,contract])
        except Exception as error:r['Failures'].append(dict(Name='contract.'+opt,Error=repr(error)));save()
    try:
        obj=out/'plan.o';binary=out/('plan'+suffix);run('plan.compile',[gcc,*flags,*strict,'-O0','-c',root/'test/support/container_projection_artifact.c','-o',obj]);run('plan.link',[gcc,*objects,obj,*libs,'-o',binary])
        for source in cases:
            prefix=out/(source.stem+'.selected');run(source.stem+'.plan',[binary,source,prefix]);plan=json.loads(Path(str(prefix)+'.plan.json').read_text());sources=plan['Sources'];modules=plan['NativeModules']
            assert 'XRT_MODULE_X509_IDENTITY' in modules
            assert not any(any(word in m for word in ('_MODULE_ALL','TLS','X509_VERIFY','X509_STORE','X509_PATH','NET_SOCKET','NET_DNS','NET_ENGINE')) for m in modules)
            assert not any('xl6_x509_store_' in s or 'path_validate' in s for s in sources)
            if source.stem=='identity.dns':
                assert not any('x509_certificate.c' in s or 'x509_general_name.c' in s or 'x509_identity_host.c' in s or 'x509_identity_match.c' in s for s in sources)
            if source.stem=='identity.bool':assert not any('x509_general_name.c' in s or 'x509_identity_match.c' in s for s in sources)
            r['Plans'].append(dict(Name=source.stem,**plan));save()
    except Exception as error:r['Failures'].append(dict(Name='pruning',Error=repr(error)));save()
    negatives={'dns-type':'auto x=x509.matchesDns(12,"localhost")\n','host-bytes':'func bad(Certificate certificate)\n auto x=certificate.matchesHost(b"localhost")\nend\n',
        'nonnullable':'func bad(Certificate certificate) as X509GeneralName\n return certificate.matchHost("localhost")\nend\n',
        'private':'func bad(Certificate certificate)\n auto name=certificate.matchHost("localhost")\n if name != null then\n  auto raw=name.handle\n end\nend\n'}
    for name,body in negatives.items():
        try:
            source=fixtureDir/('bad.'+name+'.xl');source.write_text(body);target=out/('bad.'+name+'.c');message=run('bad.'+name,[driver,source,'cgen-default',target],True)
            assert not target.exists() and any(v in message for v in ('[sem]','[ir]','[checked]','[stdlib]'))
        except Exception as error:r['Failures'].append(dict(Name='bad.'+name,Error=repr(error)));save()
    r['InputsUnchanged']=all(sha(f)==d for f,d in r['Inputs'].items()) and all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers']) and sha(driver)==build['DriverSha256'].lower()
    r['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in r['Rows']);r['Products']=[dict(Path=str(f),Sha256=sha(f)) for f in out.rglob('*') if f.is_file() and f.name!='results.json'];r['Complete']=not r['Failures'] and r['InputsUnchanged'] and r['LogsUnchanged'];save();assert r['Complete'],r['Failures']
if __name__=='__main__':main()
