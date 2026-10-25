"""Actual-source client credential contexts, in-memory/normal literal-loopback TLS.

No external endpoints, DNS, probes, malformed crypto or security tests.
Ticket delivery and shipping acceptance are separate, not certified here.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
from xlir_public_build_inputs import verify_public_build
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','build','out'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    build,pinned,_=verify_public_build(root,a.build.resolve().parent);assert build['Complete'] and build['InputsUnchanged'] and build['Optimization']=='O2'
    controls=Path(__file__).resolve().parents[1];fixtureDir=out/'fixtures';fixtureDir.mkdir()
    from tls_client_resume_fixtures import fixtures
    cases=fixtures(root,fixtureDir)
    tests=[controls/('test/cases/'+n+'.c') for n in ('xlir_tls_client_resume_contract','xlir_tls_client_resume_owned','xlir_tls_resume_catalog')]
    negatives={
        'raw':'auto value=TlsClientContext(point(null))\n',
        'write':'func bad(TlsClientContext context,TlsResume value)\n context.resume=value\nend\n',
        'nullable':'func bad(TlsClientContext context,TlsResume? value)\n auto result=context.withResume(value)\nend\n'}
    negativeCases=[]
    for name,text in negatives.items():
        file=fixtureDir/('reject_'+name+'.xl');file.write_text(text,encoding='utf8');negativeCases.append(file)
    cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();driver=Path(build['Driver'])
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    libraries=root/'res/tcc/win64/x86_64' if os.name=='nt' else Path(build['NativeRoot'])/'build/linux/tcc-dist'
    env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    pinned.update((Path(__file__).resolve(),cc,runtime,*tests,*cases))
    pinned.update(q for q in fixtureDir.rglob('*') if q.is_file())
    pinned.update(Path(m.__file__) for m in sys.modules.values() if getattr(m,'__file__',None) and Path(m.__file__).is_relative_to(controls/'tools'))
    r=dict(Complete=False,Inputs={str(q):sha(q) for q in sorted(pinned)},Rows=[],Failures=[],ClientResumeBindingAccepted=False,
        OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,NetworkSecurityTests=False,MalformedCryptoTests=False,ShippingAccepted=False,TicketOperationsAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def run(name,args,expected=0):
        args=list(map(str,args));command=args
        if args[0]==str(cc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+q.replace('\\','/')+'"' for q in args[1:]),encoding='utf8');command=[str(cc),'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=240).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,Expected=expected,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert code==expected,str(log);return log.read_text(encoding='utf8',errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-fmax-errors=4','-DXL_STATIC_EMBED',
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-missing-field-initializers',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include'),*(['-D_GNU_SOURCE'] if os.name!='nt' else [])]
    for case in cases:
        name=case.stem
        try:
            generated=out/(name+'.c');run(name+'.emit',[driver,case,'cgen-default',generated]);text=generated.read_text(encoding='utf8')
            assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6TlsClientContext\w*\([^;]*\)\s*\{',text,re.M)
            for stage in ('typed','lowered'):run(name+'.'+stage,[driver,case,'ir-'+stage,out/(name+'.'+stage+'.json')])
            for opt in ('O0','O2'):run(name+'.strict.'+opt,[cc,*flags,'-'+opt,'-c',generated,'-o',out/(name+'.'+opt+'.o')])
        except Exception as e:r['Failures'].append(dict(Name=name,Error=repr(e)));save()
    for case in negativeCases:
        try:
            text=run(case.stem,[driver,case,'cgen-default',out/(case.stem+'.c')],1);assert 'without diagnostic' not in text and not (out/(case.stem+'.c')).exists()
        except Exception as e:r['Failures'].append(dict(Name=case.stem,Error=repr(e)));save()
    objects=[q['Path'] for q in build['Objects'] if Path(q['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    links=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)]
    if not r['Failures']:
        for case in cases:
            name=case.stem
            try:
                for mode in ('run','module'):run(name+'.'+mode,[driver,case,mode,out/(name+'.'+mode)])
                binary=out/(name+'.aot'+('.exe' if os.name=='nt' else ''));target=binary.with_suffix('') if os.name=='nt' else binary
                run(name+'.aot.build',[driver,case,'exe',target]);run(name+'.aot.run',[binary])
                imports=run(name+'.aot.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
                assert not any(q in imports.lower() for q in ('xl.dll','xlc.dll','libxl.so','libxlc.so'))
                bundle=(Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text(encoding='utf8');assert '_MODULE_ALL' not in bundle
                assert ('XRT_MODULE_TLS_CLIENT_RESUME' in bundle)==(case in cases[:2])
            except Exception as e:r['Failures'].append(dict(Name=name,Error=repr(e)));save()
        for test in tests:
            for opt in ('O0','O2'):
                name=test.stem+'.'+opt;obj=out/(name+'.o');binary=out/(name+('.exe' if os.name=='nt' else ''))
                try:
                    run(name+'.compile',[cc,*flags,'-'+opt,'-c',test,'-o',obj]);run(name+'.link',[cc,obj,*objects,runtime,*links,'-o',binary])
                    args=cases if test==tests[0] else [cases[0],driver.parent/'include',runtime,libraries,out/(name+'.temps')] if test==tests[1] else []
                    assert 'ok' in run(name+'.run',[binary,*args])
                except Exception as e:r['Failures'].append(dict(Name=name,Error=repr(e)));save()
    r['InputsUnchanged']=all(sha(q)==h for q,h in r['Inputs'].items());r['Stages']=len(r['Rows']);r['ClientResumeBindingAccepted']=r['InputsUnchanged'] and not r['Failures']
    r['Complete']=r['ClientResumeBindingAccepted'];r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json'];save();assert r['Complete'],r['Failures']
if __name__=='__main__':main()
