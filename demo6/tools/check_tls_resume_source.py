"""Offline actual-source TLS credential ABI/JIT/module/AOT/pruning acceptance.

This does not certify role-context binding, ticket delivery or TLS resumption.
No network traffic or crypto-security tests are performed.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from xlir_public_build_inputs import verify_public_build

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    build,pinned,_=verify_public_build(root,a.build.resolve().parent)
    assert build['Complete'] and build['InputsUnchanged'] and build['SourceXrt'] and not build['RuntimeOnly'] and build['Optimization']=='O2'
    controls=Path(__file__).resolve().parents[1]
    case=controls/'test/cases/stdlib_tls_resume.xl';contract=controls/'test/cases/xlir_tls_resume_contract.c';owned=controls/'test/cases/xlir_tls_resume_owned.c'
    catalog=controls/'test/cases/xlir_tls_resume_catalog.c'
    # Keep language inputs away from emitted C: the supported native companion
    # discovery would otherwise import our generated empty.c a second time.
    inputs=out/'inputs';inputs.mkdir()
    empty=inputs/'empty.xl';empty.write_text('assert 2 + 2 == 4\n',encoding='utf8')
    negativeBodies={
        'raw':'auto value=TlsResume(point(null))\n',
        'write':'func bad(TlsResume value)\n value.ticket=b"changed"\nend\n',
        'native':'func bad(TlsResume value)\n auto raw=value.nativeHandle()\nend\n',
        'wrong_enum':'auto value=TlsResume.fromParts(TlsCipherSuite.aes128GcmSha256,TlsCipherSuite.aes128GcmSha256,b"t",b"01234567890123456789012345678901",time.now(),120)\n'}
    negatives=[]
    for name,text in negativeBodies.items():
        file=out/('reject_'+name+'.xl');file.write_text(text,encoding='utf8');negatives.append(file)
    cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    env=dict(os.environ)
    if os.name!='nt':env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    pinned.update((Path(__file__).resolve(),case,contract,owned,catalog,cc,runtime,empty,*negatives))
    r=dict(Complete=False,Inputs={str(q):sha(q) for q in sorted(pinned)},Rows=[],Failures=[],ShippingAccepted=False,RoleResumeAccepted=False,NetworkTraffic=False,NetworkSecurityTests=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def run(name,args,expected=0):
        command=list(map(str,args));actual=command
        if command[0]==str(cc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/')+'"' for s in command[1:]),encoding='utf8');actual=[str(cc),'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(actual,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=900).returncode
        r['Rows'].append(dict(Name=name,Command=command,Exit=code,Expected=expected,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert code==expected,str(log);return log.read_text(encoding='utf8',errors='replace')
    driver=Path(build['Driver']);objects=[q['Path'] for q in build['Objects'] if Path(q['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    links=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)]
    flags=['-std=c11','-Wall','-Wextra','-Werror','-fmax-errors=4','-DXL_STATIC_EMBED',
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-missing-field-initializers',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include'),*(['-D_GNU_SOURCE'] if os.name!='nt' else [])]
    # Batch discovery before running generated programs.
    for fixture in (case,empty):
        name=fixture.stem
        try:
            generated=out/(name+'.c');run(name+'.emit',[driver,fixture,'cgen-default',generated]);text=generated.read_text(encoding='utf8')
            assert 'xl6_xrt_tls_resume_' not in text and not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6TlsResume\w+\([^;]*\)\s*\{',text,re.M)
            for stage in ('typed','lowered'):run(name+'.'+stage,[driver,fixture,'ir-'+stage,out/(name+'.'+stage+'.json')])
            for opt in ('O0','O2'):run(name+'.strict.'+opt,[cc,*flags,'-'+opt,'-c',generated,'-o',out/(name+'.'+opt+'.o')])
        except Exception as e:r['Failures'].append(dict(Name=name,Error=repr(e)));save()
    for fixture in negatives:
        try:
            log=run(fixture.stem,[driver,fixture,'cgen-default',out/(fixture.stem+'.c')],1)
            assert 'without diagnostic' not in log and not (out/(fixture.stem+'.c')).exists()
        except Exception as e:r['Failures'].append(dict(Name=fixture.stem,Error=repr(e)));save()
    if not r['Failures']:
        for fixture in (case,empty):
            name=fixture.stem
            try:
                for mode in ('run','module'):run(name+'.'+mode,[driver,fixture,mode,out/(name+'.'+mode)])
                binary=out/(name+'.aot'+('.exe' if os.name=='nt' else ''));target=binary.with_suffix('') if os.name=='nt' else binary
                run(name+'.aot.build',[driver,fixture,'exe',target]);run(name+'.aot.run',[binary])
                imports=run(name+'.aot.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
                assert not any(s in imports.lower() for s in ('xl.dll','xlc.dll','libxl.so','libxlc.so'))
                bundle=(Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text(encoding='utf8');assert '_MODULE_ALL' not in bundle
                if fixture==empty:assert 'XRT_MODULE_TLS_' not in bundle
                else:assert 'XRT_MODULE_TLS_RESUME' in bundle and all(s not in bundle for s in ('XRT_MODULE_TLS_CLIENT','XRT_MODULE_TLS_SERVER','XRT_MODULE_TLS_STREAM'))
            except Exception as e:r['Failures'].append(dict(Name=name,Error=repr(e)));save()
        for test in (contract,owned,catalog):
            for opt in ('O0','O2'):
                name=test.stem+'.'+opt;obj=out/(name+'.o');binary=out/(name+('.exe' if os.name=='nt' else ''))
                try:
                    run(name+'.compile',[cc,*flags,'-'+opt,'-c',test,'-o',obj]);run(name+'.link',[cc,obj,*objects,runtime,*links,'-o',binary])
                    result=run(name+'.run',[binary,*([case,empty] if test==contract else [])]);assert 'ok' in result
                except Exception as e:r['Failures'].append(dict(Name=name,Error=repr(e)));save()
    r['InputsUnchanged']=all(sha(q)==h for q,h in r['Inputs'].items())
    r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json']
    r['Stages']=len(r['Rows']);r['Complete']=r['InputsUnchanged'] and not r['Failures'];save();assert r['Complete'],r['Failures']
if __name__=='__main__':main()
