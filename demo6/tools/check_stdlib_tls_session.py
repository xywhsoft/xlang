"""Real-source normal in-memory TLS and exact managed-product contracts.
No sockets, probes, malformed crypto or native security test executables.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True);p.add_argument('--controls',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--source-build',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();controls=a.controls.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    sys.path.insert(0,str(controls/'tools'));sys.path.insert(1,str(root/'tools'));from tls_session_fixtures import language_fixtures
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();build=json.loads(a.source_build.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['SourceXrt'] and not build['RuntimeOnly']
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower();gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    fixtureDir=out/'fixtures';fixtureDir.mkdir();cases,contract=language_fixtures(root,fixtureDir)
    cases += [root/'test/cases/stdlib_tls_session_product.xl']
    if (controls/'test/cases/cgen_owned_native_product.xl').exists():cases.append(controls/'test/cases/cgen_owned_native_product.xl')
    contractSource=controls/'test/cases/xlir_tls_session_contract.c'
    paths=[Path(__file__),gcc,contractSource,root/'test/support/container_projection_artifact.c',root/'test/cases/stdlib_tls_session_product.xl',
        *[Path(sys.modules[n].__file__) for n in ['tls_session_fixtures','tls_role_context_fixtures','tls_identity_fixtures','tls_verifier_fixtures','x509_path_fixtures','x509_identity_fixtures','x509_identity_language_fixtures','check_key_format_wire']],
        *[root/'test/support'/('crypto_'+n+'_runtime.c') for n in ['rsa','nist','ed25519','xdh']]]
    r=dict(Complete=False,Rows=[],Failures=[],Plans=[],Inputs={str(q):sha(q) for q in paths},SourceBuild=str(a.source_build),NetworkTraffic=False,SecurityTests=False,MalformedCryptoTests=False,ShippingAccepted=False,FullTlsAccepted=False,NormalInMemoryHandshake=True)
    env=dict(os.environ);runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
    def run(name,args,reject=False):
        args=list(map(str,args));command=args
        if args[0]==str(gcc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=900 if name.startswith('contracts.') else 300).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,ExpectedReject=reject,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True);assert (code!=0 if reject else code==0),str(log);return log.read_text(errors='replace')
    flags=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include'),'-I'+str(fixtureDir)]+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
    strict=['-Wall','-Wextra','-Werror','-fmax-errors=4'];compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-unused-const-variable','-Wno-pointer-sign']
    for source in cases:
        name=source.stem
        try:
            generated=out/(name+'.c');run(name+'.emit',[driver,source,'cgen-default',generated]);text=generated.read_text()
            assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Tls\w+\([^;]*\)\s*\{',text,re.M)
            assert 'xl6_tls_config_create' not in text and 'xl6_xrt_tls_session_create' not in text
            run(name+'.jit',[driver,source,'run',out/(name+'.jit')]);run(name+'.module',[driver,source,'module',out/(name+'.module')])
            for stage in ['typed','lowered']:run(name+'.'+stage,[driver,source,'ir-'+stage,out/(name+'.'+stage+'.json')])
            for opt in ['O0','O2']:run(name+'.'+opt,[gcc,*flags,*strict,*compat,'-'+opt,'-c',generated,'-o',out/(name+'.'+opt+'.o')])
            binary=out/(name+'.aot'+('.exe' if os.name=='nt' else ''));target=binary.with_suffix('') if os.name=='nt' else binary
            run(name+'.aot.build',[driver,source,'exe',target]);run(name+'.aot.run',[binary])
            imports=run(name+'.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary]);assert not any(v in imports.lower() for v in ['xl.dll','xlc.dll','libxl.so','libxlc.so'])
            assert '_MODULE_ALL' not in (Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text()
        except Exception as e:r['Failures'].append(dict(Name=name,Error=repr(e)));save()
    objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ['xlir_public_source_driver.c','xlir_public_source_linux_driver.c']];assert len(objects)==len(build['Objects'])-1
    libs=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)])]
    for opt in ['O0','O2']:
        try:
            obj=out/('contracts.'+opt+'.o');binary=out/('contracts.'+opt+('.exe' if os.name=='nt' else ''))
            run('contracts.'+opt+'.compile',[gcc,*flags,*strict,*compat,'-'+opt,'-c',contractSource,'-o',obj]);run('contracts.'+opt+'.link',[gcc,*objects,obj,*libs,'-o',binary]);assert 'TLS session XLIR:' in run('contracts.'+opt+'.run',[binary,contract])
        except Exception as e:r['Failures'].append(dict(Name='contracts.'+opt,Error=repr(e)));save()
    try:
        obj=out/'plan.o';binary=out/('plan'+('.exe' if os.name=='nt' else ''));run('plan.compile',[gcc,*flags,*strict,*compat,'-O0','-c',root/'test/support/container_projection_artifact.c','-o',obj]);run('plan.link',[gcc,*objects,obj,*libs,'-o',binary])
        for source in cases:
            prefix=out/(source.stem+'.selected');run(source.stem+'.plan',[binary,source,prefix]);plan=json.loads(Path(str(prefix)+'.plan.json').read_text());modules=plan['NativeModules'];sources=plan['Sources']
            assert not any(m in modules for m in ['XRT_MODULE_TLS_STREAM','XRT_MODULE_NET_SOCKET','XRT_MODULE_NET_TCP','XRT_MODULE_NET_ENGINE'])
            if source.stem in ['tls.session.client-only','tls.session.server-only']:
                assert not any(m.startswith('XRT_MODULE_TLS_') for m in modules)
                assert not any('xl6_tls_session_' in q for q in sources)
            if source.stem=='tls.session.client-live':assert 'XRT_MODULE_TLS_CLIENT_VERIFY' in modules and 'XRT_MODULE_TLS_SERVER' not in modules and not any('xl6_tls_session_server.c' in q for q in sources)
            if source.stem=='tls.session.server-live':assert 'XRT_MODULE_TLS_SERVER' in modules and 'XRT_MODULE_TLS_CLIENT' not in modules and not any('xl6_tls_session_client.c' in q for q in sources)
            r['Plans'].append(dict(Source=str(source),Plan=str(prefix)+'.plan.json',NativeModules=modules,Sources=sources))
    except Exception as e:r['Failures'].append(dict(Name='plans',Error=repr(e)));save()
    negatives={'owner':'func bad(TlsClientContext c)\n auto a=c.read()\nend\n',
        'private':'func bad(TlsSession s)\n auto p=s.handle\nend\n',
        'legacy':'auto c=TlsConfig()\nauto s=c.session()\n',
        'raw':'point p=null\nauto s=TlsSession(p,true,p)\n',
        'untyped-feed':'func bad(TlsSession s,Buffer b)\n s.feed(b)\nend\n'}
    for name,text in negatives.items():
        source=out/('bad.'+name+'.xl');source.write_text(text);target=out/('bad.'+name+'.c')
        try:
            message=run('bad.'+name,[driver,source,'cgen-default',target],True);assert not target.exists() and any(v in message for v in ['[sem]','[ir]','[checked]','[stdlib]']) and '[parse]' not in message
        except Exception as e:r['Failures'].append(dict(Name='bad.'+name,Error=repr(e)));save()
    r['Stages']=len(r['Rows']);r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items()) and all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers']) and sha(driver)==build['DriverSha256'].lower()
    r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json'];r['Complete']=not r['Failures'] and r['InputsUnchanged'];save();assert r['Complete'],r['Failures']
if __name__=='__main__':main()
