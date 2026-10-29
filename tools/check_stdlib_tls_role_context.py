"""Real source compiler TLS role configuration declarations and ownership.
Offline valid fixed-key certificates only; not handshake/transport acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time

def command_timeout(name):
    # The negative matrix repeatedly checks the entire debug-built module.
    # Ordinary language/compile/run stages keep their original bounded limit.
    return 900 if name.startswith('contracts.') else 300

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--controls',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--source-build',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();controls=a.controls.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    sys.path.insert(0,str(controls/'tools'));sys.path.insert(1,str(root/'tools'));from tls_role_context_fixtures import language_fixtures
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();build=json.loads(a.source_build.read_text(encoding='utf-8-sig'))
    assert build.get('Complete',True) and build['SourceXrt'] and not build['RuntimeOnly']
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();fixtureDir=out/'fixtures';fixtureDir.mkdir();cases,contract=language_fixtures(root,fixtureDir)
    regressions=[root/'test/cases'/n for n in ['stdlib_tls_parameters.xl','stdlib_tls_verification_policy.xl','stdlib_tls_parameters_array_regression.xl']];cases+=regressions
    support=controls/'test/support/tls_role_context_owned.c';contractSource=controls/'test/cases/xlir_tls_role_context_contract.c'
    paths=[Path(__file__),gcc,support,contractSource,root/'test/support/container_projection_artifact.c',*[Path(sys.modules[n].__file__) for n in ['tls_role_context_fixtures','tls_identity_fixtures','tls_verifier_fixtures','x509_path_fixtures','x509_identity_fixtures','x509_identity_language_fixtures','check_key_format_wire']],
        *[root/'test/support'/('crypto_'+n+'_runtime.c') for n in ['rsa','nist','ed25519','xdh']],*regressions]
    r=dict(Complete=False,Rows=[],Failures=[],Plans=[],Inputs={str(q):sha(q) for q in paths},SourceBuild=str(a.source_build),NetworkTraffic=False,SecurityTests=False,MalformedCryptoTests=False,ShippingAccepted=False,FullTlsAccepted=False)
    env=dict(os.environ);runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
    def run(name,args,reject=False):
        args=list(map(str,args));command=args
        if args[0]==str(gcc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=command_timeout(name)).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,ExpectedReject=reject,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert (code!=0 if reject else code==0),str(log);return log.read_text(errors='replace')
    flags=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include'),'-I'+str(fixtureDir)]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    strict=['-Wall','-Wextra','-Werror','-fmax-errors=4'];compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-unused-const-variable','-Wno-pointer-sign']
    for source in cases:
        name=source.stem
        try:
            generated=out/(name+'.c');run(name+'.emit',[driver,source,'cgen-default',generated]);text=generated.read_text()
            assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Tls\w+\([^;]*\)\s*\{',text,re.M) and 'xl6_tls_config_create' not in text
            run(name+'.jit',[driver,source,'run',out/(name+'.jit')]);run(name+'.module',[driver,source,'module',out/(name+'.module')])
            for stage in ['typed','lowered']:run(name+'.'+stage,[driver,source,'ir-'+stage,out/(name+'.'+stage+'.json')])
            for opt in ['O0','O2']:run(name+'.'+opt,[gcc,*flags,*strict,*compat,'-'+opt,'-c',generated,'-o',out/(name+'.'+opt+'.o')])
            binary=out/(name+'.aot'+('.exe' if os.name=='nt' else ''));target=binary.with_suffix('') if os.name=='nt' else binary
            run(name+'.aot.build',[driver,source,'exe',target]);run(name+'.aot.run',[binary])
            imports=run(name+'.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
            assert not any(v in imports.lower() for v in ['xl.dll','xlc.dll','libxl.so','libxlc.so'])
            assert '_MODULE_ALL' not in (Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text()
        except Exception as e:r['Failures'].append(dict(Name=name,Error=repr(e)));save()
    objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ['xlir_public_source_driver.c','xlir_public_source_linux_driver.c']];assert len(objects)==len(build['Objects'])-1
    libs=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)])]
    for name,source in [('contracts',contractSource),('owned',support)]:
        for opt in ['O0','O2']:
            try:
                obj=out/(name+'.'+opt+'.o');binary=out/(name+'.'+opt+('.exe' if os.name=='nt' else ''))
                run(name+'.'+opt+'.compile',[gcc,*flags,*strict,*compat,'-'+opt,'-c',source,'-o',obj]);run(name+'.'+opt+'.link',[gcc,*objects,obj,*libs,'-o',binary])
                text=run(name+'.'+opt+'.run',[binary,*([contract] if name=='contracts' else [])]);assert ('TLS role XLIR:' if name=='contracts' else 'TLS role contexts:') in text
            except Exception as e:r['Failures'].append(dict(Name=name+'.'+opt,Error=repr(e)));save()
    try:
        obj=out/'plan.o';binary=out/('plan'+('.exe' if os.name=='nt' else ''));run('plan.compile',[gcc,*flags,*strict,*compat,'-O0','-c',root/'test/support/container_projection_artifact.c','-o',obj]);run('plan.link',[gcc,*objects,obj,*libs,'-o',binary])
        for source in cases:
            prefix=out/(source.stem+'.selected');run(source.stem+'.plan',[binary,source,prefix]);plan=json.loads(Path(str(prefix)+'.plan.json').read_text());modules=plan['NativeModules'];sources=plan['Sources']
            assert not any(m in modules for m in ['XRT_MODULE_TLS_CLIENT','XRT_MODULE_TLS_SERVER','XRT_MODULE_TLS_SESSION','XRT_MODULE_TLS_STREAM','XRT_MODULE_NET_TCP','XRT_MODULE_NET_SOCKET'])
            if source.stem=='tls.context.client-only':
                assert not any('TLS_IDENTITY' in m or 'X509_VERIFY_' in m for m in modules)
                assert not any('xl6_tls_server_context' in q or 'xl6_tls_protocols_input.c' in q for q in sources)
            if source.stem=='tls.context.server-only':
                assert not any('TLS_VERIFY' in m or 'X509_VERIFY_' in m for m in modules)
                assert not any('xl6_tls_client_context' in q or 'xl6_tls_protocols_input.c' in q for q in sources)
            if source.stem.startswith('tls.context.') and source.stem not in ['tls.context.client-only','tls.context.server-only']:
                for family,feature in [('rsa','RSA'),('p256','P256'),('p384','P384'),('ed25519','ED25519')]:
                    expected='p256' if source.stem=='tls.context.owned' else source.stem.split('.')[-1]
                    assert ('XRT_MODULE_TLS_IDENTITY_'+feature in modules)==(family==expected)
            r['Plans'].append(dict(Source=str(source),Plan=str(prefix)+'.plan.json',NativeModules=modules,Sources=sources))
    except Exception as e:r['Failures'].append(dict(Name='plans',Error=repr(e)));save()
    negatives={'client-material':'func bad(TlsIdentity identity)\n auto c=TlsClientContext(identity)\nend\n',
        'server-material':'func bad(TlsVerifier verifier)\n auto s=TlsServerContext(verifier)\nend\n',
        'protocols':'func bad(TlsClientContext client,array<string> protocols)\n auto c=client.withProtocols(protocols)\nend\n',
        'role-swap':'func bad(TlsClientContext client,TlsServerContext server)\n auto c=client.withVerifier(server)\nend\n',
        'private':'func bad(TlsClientContext client)\n auto raw=client.handle\nend\n'}
    for name,text in negatives.items():
        source=out/('bad.'+name+'.xl');source.write_text(text);target=out/('bad.'+name+'.c')
        try:
            message=run('bad.'+name,[driver,source,'cgen-default',target],True)
            assert not target.exists() and any(v in message for v in ['[sem]','[ir]','[checked]','[stdlib]']) and '[parse]' not in message
        except Exception as e:r['Failures'].append(dict(Name='bad.'+name,Error=repr(e)));save()
    r['Stages']=len(r['Rows']);r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items()) and all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers']) and sha(driver)==build['DriverSha256'].lower()
    r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json']
    r['Complete']=not r['Failures'] and r['InputsUnchanged'];save();assert r['Complete'],r['Failures']
if __name__=='__main__':main()
