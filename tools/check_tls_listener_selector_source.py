"""Actual-source owned Listener selector, JIT/module/AOT and checked cleanup.

Only fixed offline certificates, self-created literal loopback and allocator/
ownership checks. No external endpoints, scans or crypto security campaigns.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--fixtures',type=Path)
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    build=json.loads(a.build.read_text(encoding='utf-8-sig'));assert build['Complete'] and build['InputsUnchanged'] and build['SourceXrt'] and not build['RuntimeOnly'] and build['Optimization']=='O2'
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    controls=Path(__file__).resolve().parents[1];fixtureDir=a.fixtures.resolve() if a.fixtures else out/'fixtures'
    if a.fixtures:cases=[fixtureDir/name for name in ('tls.listener.selector.xl','tls.listener.selector.namespace.xl','public_tls_metadata.xl','public_tls_metadata_declarations.xl')]
    else:
        fixtureDir.mkdir();from tls_listener_selector_fixtures import fixtures;cases=fixtures(root,fixtureDir)
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so';env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();contract=controls/'test/cases/xlir_tls_listener_selector_owned.c'
    planContract=controls/'test/cases/xlir_tls_listener_selector_plan.c'
    files={Path(__file__).resolve(),a.build.resolve(),cc,contract,planContract,*cases}
    files.update(q for q in fixtureDir.rglob('*') if q.is_file())
    files.update(Path(m.__file__) for m in sys.modules.values() if getattr(m,'__file__',None) and Path(m.__file__).is_relative_to(controls/'tools'))
    r=dict(Complete=False,SourceBuild=str(a.build.resolve()),SourceBuildSha256=sha(a.build),Inputs={str(q):sha(q) for q in sorted(files)},Rows=[],Products=[],Failures=[],
        OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,NetworkSecurityTests=False,MalformedCryptoTests=False,ShippingAccepted=False,FullTlsAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def run(name,args):
        args=list(map(str,args));command=args
        if args[0]==str(cc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:]),encoding='utf8');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=900).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert code==0,str(log);return log.read_text(encoding='utf8',errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-missing-field-initializers','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),
        '-I'+str(driver.parent/'include'),*(['-D_GNU_SOURCE'] if os.name!='nt' else [])]
    # Native delayed-credit/checked-tail checks are independent of language discovery.
    objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    links=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)]
    for opt in ('O0','O2'):
        try:
            label='owned.'+opt;obj=out/(label+'.o');binary=out/(label+('.exe' if os.name=='nt' else ''))
            run(label+'.compile',[cc,*flags,'-'+opt,'-c',contract,'-o',obj]);run(label+'.link',[cc,*objects,obj,runtime,*links,'-o',binary])
            assert 'deferred checked cleanup with/without diagnostic, balanced: ok' in run(label+'.run',[binary])
        except Exception as e:r['Failures'].append(dict(Name='owned.'+opt,Error=repr(e)));save()
    # Resident units are compiled separately, not listed in xl6_xrt_impl.c.
    # Inspect the actual lowered executable plan; source-name searches in that
    # native-only file cannot prove either resident presence or pruning.
    for opt in ('O0','O2'):
        try:
            label='plan.'+opt;obj=out/(label+'.o');binary=out/(label+('.exe' if os.name=='nt' else ''))
            run(label+'.compile',[cc,*flags,'-'+opt,'-c',planContract,'-o',obj]);run(label+'.link',[cc,*objects,obj,runtime,*links,'-o',binary])
            assert 'exact resident sources and empty/static exclusion: ok' in run(label+'.run',[binary,*cases])
        except Exception as e:r['Failures'].append(dict(Name='plan.'+opt,Error=repr(e)));save()
    for case in cases:
        name=case.stem
        try:
            generated=out/(name+'.c');run(name+'.emit',[driver,case,'cgen-default',generated]);text=generated.read_text(encoding='utf8')
            assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Tls\w+\([^;]*\)\s*\{',text,re.M)
            for mode in ('typed','lowered'):run(name+'.'+mode,[driver,case,'ir-'+mode,out/(name+'.'+mode+'.json')])
            for opt in ('O0','O2'):run(name+'.strict.'+opt,[cc,*flags,'-'+opt,'-c',generated,'-o',out/(name+'.'+opt+'.o')])
        except Exception as e:r['Failures'].append(dict(Name=name,Error=repr(e)));save()
    if not r['Failures']:
        for index,case in enumerate(cases):
            name=case.stem
            try:
                for mode in ('run','module'):run(name+'.'+mode,[driver,case,mode,out/(name+'.'+mode)])
                binary=out/(name+'.aot'+('.exe' if os.name=='nt' else ''));target=binary.with_suffix('') if os.name=='nt' else binary
                run(name+'.aot.build',[driver,case,'exe',target]);run(name+'.aot.run',[binary])
                imports=run(name+'.aot.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
                assert not any(q in imports.lower() for q in ('xl.dll','xlc.dll','libxl.so','libxlc.so'))
                bundle=(Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text(encoding='utf8');assert '_MODULE_ALL' not in bundle
                if index==3:assert 'XRT_MODULE_TLS_' not in bundle
            except Exception as e:r['Failures'].append(dict(Name=name,Error=repr(e)));save()
    r['InputsUnchanged']=all(sha(q)==h for q,h in r['Inputs'].items())
    r['Stages']=len(r['Rows']);r['Complete']=r['InputsUnchanged'] and not r['Failures'];r['ListenerSelectorAccepted']=r['Complete']
    r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json'];save();assert r['Complete'],r['Failures']
if __name__=='__main__':main()
