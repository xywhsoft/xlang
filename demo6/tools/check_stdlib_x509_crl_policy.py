"""Current-source immutable X.509 profile APIs, ABI, ordinary lifetime and pruning.
Does not certify trust validation, TLS, shipping, or network/security tests.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
from cryptography import x509
from cryptography.hazmat.primitives import serialization
from x509_crl_policy_language_fixtures import language_fixtures


def literal(data,text=False):
    return ('"' if text else 'b"')+''.join('\\x%02x'%b for b in data)+'"'


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-build',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    build=json.loads(a.source_build.read_text(encoding='utf-8-sig'));assert build.get('Complete',True) and build['SourceXrt'] and not build['RuntimeOnly']
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    fixture_dir=out/'fixtures';fixture_dir.mkdir();cases,contract=language_fixtures(root,fixture_dir)
    exported=set(re.findall(r'XL6_TEXT_EXPORT\((\w+),',(root/'src/runtime/xl6_x509_crl_policy_symbols.inc').read_text()))
    declared=set(re.findall(r'XL6_TEXT_API [^;\n]*?\b(\w+)\(',(root/'src/runtime/xl6_x509_crl_policy_abi.h').read_text()))
    contracted=set(re.findall(r'^\{"(\w+)"',(root/'src/ir/xl6_ir_x509_crl_policy_contracts.inc').read_text(),re.M));assert len(exported)==30 and exported==declared==contracted
    report=dict(Complete=False,Rows=[],Failures=[],Products=[],Plans=[],ResidentEntries=30,SourceBuild=str(a.source_build),
        Inputs={str(path):sha(path) for path in [Path(__file__),gcc,root/'tools/check_x509_crl_policy.py',root/'tools/x509_crl_policy_language_fixtures.py',root/'tools/x509_crl_policy_fixtures.py',root/'test/cases/xlir_x509_crl_policy_contract.c',root/'test/support/container_projection_artifact.c']},
        TrustValidationAcceptance=False,TlsAcceptance=False,ShippingAcceptance=False,NetworkTraffic=False,NetworkSecurityTests=False,Scope=__doc__)
    env=dict(os.environ);runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command,reject=False):
        command=list(map(str,command));invocation=command
        if command[0]==str(gcc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+arg.replace('\\','/').replace('"','\\"')+'"' for arg in command[1:])+'\n',encoding='utf8');invocation=[command[0],'@'+str(rsp)]
        log=out/(name+'.log')
        with log.open('wb') as stream:code=subprocess.run(invocation,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240).returncode
        report['Rows'].append(dict(Name=name,Command=command,Invocation=invocation,Exit=code,Log=str(log),Sha256=sha(log)));save();print(name,code,flush=True)
        assert code==(1 if reject else 0),(name,str(log));return log.read_text(encoding='utf8',errors='replace')
    flags=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    strict=['-Wall','-Wextra','-Werror','-fmax-errors=4'];compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-unused-label','-Wno-unused-const-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    suffix='.exe' if os.name=='nt' else ''
    for source in cases:
        name=source.stem
        try:
            generated=out/(name+'.c');run(name+'.emit',[driver,source,'cgen-default',generated])
            text=generated.read_text();assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6X509\w+\([^;]*\)\s*\{',text,re.M)
            run(name+'.jit',[driver,source,'run',out/(name+'.jit')])
            for stage in ('typed','lowered'):run(name+'.'+stage,[driver,source,'ir-'+stage,out/(name+'.'+stage+'.json')])
            if name=='policy.enums':assert '__X509CrlCriticalContext' not in (out/(name+'.typed.json')).read_text()
            for opt in ('O0','O2'):run(name+'.'+opt,[gcc,*flags,*strict,*compat,'-'+opt,'-c',generated,'-o',out/(name+'.'+opt+'.o')])
            run(name+'.module',[driver,source,'module',out/(name+'.module')])
            binary=out/(name+'.aot'+suffix);target=binary.with_suffix('') if os.name=='nt' else binary
            run(name+'.aot.build',[driver,source,'exe',target]);run(name+'.aot.run',[binary])
            imports=run(name+'.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary]);assert not any(v in imports.lower() for v in ('xl.dll','xlc.dll','libxl.so','libxlc.so'))
            assert '_MODULE_ALL' not in (Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text()
        except Exception as error:report['Failures'].append(dict(Name=name,Error=repr(error)));save()
    objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')];assert len(objects)==len(build['Objects'])-1
    libs=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)])]
    for opt in ('O0','O2'):
        try:
            obj=out/('contract.'+opt+'.o');binary=out/('contract.'+opt+suffix)
            run('contract.'+opt+'.compile',[gcc,*flags,*strict,'-'+opt,'-c',root/'test/cases/xlir_x509_crl_policy_contract.c','-o',obj])
            run('contract.'+opt+'.link',[gcc,*objects,obj,*libs,'-o',binary]);assert 'X509 CRL policy XLIR:' in run('contract.'+opt+'.run',[binary,contract])
        except Exception as error:report['Failures'].append(dict(Name='contract.'+opt,Error=repr(error)));save()
    try:
        obj=out/'plan.o';binary=out/('plan'+suffix)
        run('plan.compile',[gcc,*flags,*strict,'-O0','-c',root/'test/support/container_projection_artifact.c','-o',obj]);run('plan.link',[gcc,*objects,obj,*libs,'-o',binary])
        for source in cases:
            prefix=out/(source.stem+'.selected');run(source.stem+'.plan',[binary,source,prefix]);plan=json.loads(Path(str(prefix)+'.plan.json').read_text())
            assert not any(any(word in module for word in ('_MODULE_ALL','TLS','X509_PATH','X509_STORE')) for module in plan['NativeModules'])
            modules=plan['NativeModules'];sources=plan['Sources']
            if source.stem=='policy.enums':
                assert not any('x509_' in s for s in sources) and not any('X509' in m or 'CRYPTO' in m for m in modules)
            if source.stem=='policy.empty':
                assert not any('X509_CRL_POLICY' in m or 'CRYPTO' in m for m in modules)
                assert not any('crl_validate' in s or 'crl_critical' in s or 'revocation_update' in s for s in sources)
            if source.stem in ('policy.normal','policy.partial'):
                assert 'XRT_MODULE_X509_CRL_POLICY' in modules
                assert not any('crl_critical' in s or 'crl_validate_callback' in s for s in sources)
            if source.stem=='policy.callback':assert any('crl_validate_callback' in s for s in sources)
            report['Plans'].append(dict(Name=source.stem,**plan));save()
    except Exception as error:report['Failures'].append(dict(Name='pruning',Error=repr(error)));save()
    negatives={'default-validated':'auto value=X509ValidatedCrl()\n',
        'default-result':'auto value=X509RevocationResult()\n',
        'wrong-flags':'func bad(CertificateRevocationList crl,Certificate issuer,time at)\n auto x=X509ValidatedCrl.fromCrl(crl,issuer,at,0,X509ReasonFlags.all)\nend\n',
        'wrong-target':'auto value=X509RevocationCheck(b"certificate")\n',
        'raw-result':'func bad(X509RevocationCheck value,X509RevocationResult result)\n auto x=value.withCrl(result)\nend\n',
        'private':'func bad(X509ValidatedCrl value)\n auto raw=value.handle\nend\n',
        'status-bool':'func bad(X509RevocationResult value) as bool\n return value.status\nend\n',
        'callback-result':'func bad(CertificateRevocationList crl,Certificate issuer,time at)\n auto x=X509ValidatedCrl.fromCrl(crl,issuer,at,func(X509CrlEntry? e,X509Extension x) as int\n return 1\nend)\nend\n'}
    for name,body in negatives.items():
        try:
            source=fixture_dir/('bad.'+name+'.xl');source.write_text(body);target=out/('bad.'+name+'.c');message=run('bad.'+name,[driver,source,'cgen-default',target],True)
            assert not target.exists() and any(tag in message for tag in ('[sem]','[ir]','[checked]','[stdlib]'))
        except Exception as error:report['Failures'].append(dict(Name='bad.'+name,Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(path)==digest for path,digest in report['Inputs'].items()) and all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers']) and sha(driver)==build['DriverSha256'].lower()
    report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in report['Rows'])
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in out.rglob('*') if path.is_file() and path.name!='results.json']
    report['Complete']=not report['Failures'] and report['InputsUnchanged'] and report['LogsUnchanged'];save();assert report['Complete'],report['Failures']


if __name__=='__main__':main()
