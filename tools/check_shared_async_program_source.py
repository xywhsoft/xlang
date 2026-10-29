"""Actual-source shared-CFG async acceptance, no external/security tests."""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for data in iter(lambda:stream.read(1048576),b''):h.update(data)
    return h.hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['root','build','out','original-tls']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    build=json.loads(a.build.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['SourceXrt'] and not build['RuntimeOnly'] and build['Optimization']=='O2'
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    env=dict(os.environ)
    if os.name=='nt':shutil.copy2(root/'release/xl.dll',out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    sys.path.insert(0,str(root/'tools'));from async_program_fixtures import fixtures
    generated=fixtures(out)
    cases=generated+[root/'test/cases'/(name+'.xl') for name in [
        'xlir_async_cfg_liveness','async_class_receiver_lifetime','async_generic_class_method',
        'cgen_async_param_ownership','cgen_async_string_ownership','enum_async_closure',
        'protocol_async_dispatch','cgen_try_finally_pending_async']]+[a.original_tls.resolve()]
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    files={Path(__file__).resolve(),gcc,*cases,*[Path(m.__file__) for m in sys.modules.values()
        if getattr(m,'__file__',None) and Path(m.__file__).is_relative_to(root/'tools')]}
    r=dict(Complete=False,SourceBuild=str(a.build),SourceBuildSha256=sha(a.build),Inputs={str(q):sha(q) for q in files},
        Rows=[],Products=[],Cases=[],Failures=[],OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,
        NetworkSecurityTests=False,MalformedCryptoTests=False,ShippingAccepted=False,FullTlsAccepted=False,FullGoalComplete=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
    def run(name,command):
        command=list(map(str,command));invocation=command
        if Path(command[0])==gcc:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in command[1:])+'\n');invocation=[command[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(invocation,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=900).returncode
        r['Rows'].append(dict(Name=name,Command=command,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save()
        print(name,code,flush=True);assert not code,str(log);return log.read_text(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-but-set-variable','-Wno-unused-const-variable',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include'),
        *(['-D_GNU_SOURCE'] if os.name!='nt' else [])]
    try:
        for index,case in enumerate(cases):
            name=str(index)+'.'+case.stem;c=out/(name+'.c');lowered=out/(name+'.json')
            run(name+'.emit',[driver,case,'cgen-default',c]);run(name+'.ir',[driver,case,'ir-lowered',lowered])
            source=c.read_text();header=lowered.open(encoding='utf8')
            with header:summary=header.read(5000)
            blocks=int(re.search(r'"asyncMachineBlockCount":\s*(\d+)',summary).group(1))
            machines=int(re.search(r'"asyncMachineCount":\s*(\d+)',summary).group(1))
            programs=list(re.finditer(r'^static void xlir_async_machine_program_(\d+)\([^\n]+\)\n\{',source,re.M))
            # Unused imported async methods can have analyzed IR records but
            # no emitted C target. Every emitted scheduled target still has
            # exactly one shared program, regardless of suspension count.
            symbols=[q[1] for q in programs]
            assert len(programs)<=machines and len(symbols)==len(set(symbols)),(case,len(programs),machines)
            if case in generated or case==a.original_tls.resolve():assert programs,case
            labels=re.findall(r'^ap(\d+):;',source,re.M);assert len(labels)<=blocks,(case,len(labels),blocks)
            assert len(labels)==len(set(labels)),case
            wrappers=re.findall(r'^static void xlir_async_machine_resume_\d+_\d+\([^\n]+\)\n\{([^}]+)\}',source,re.M)
            assert all('xlir_async_machine_program_' in q and 'goto ap' not in q for q in wrappers)
            if case==a.original_tls.resolve():assert c.stat().st_size<276967614//5
            r['Cases'].append(dict(Path=str(case),GeneratedCBytes=c.stat().st_size,Machines=machines,Blocks=blocks,
                EmittedBlocks=len(labels),ResumeWrappers=len(wrappers)));save()
            for opt in ['O0','O2']:run(name+'.strict.'+opt,[gcc,*flags,'-'+opt,'-c',c,'-o',out/(name+'.'+opt+'.o')])
            for mode in ['run','module']:run(name+'.'+mode,[driver,case,mode,out/(name+'.'+mode)])
            if case in [generated[0],generated[-2],generated[-1],a.original_tls.resolve()] or case.stem=='xlir_async_cfg_liveness':
                target=out/(name+'.aot');binary=Path(str(target)+'.exe') if os.name=='nt' else target
                run(name+'.aot.build',[driver,case,'exe',target]);run(name+'.aot.run',[binary])
                imports=run(name+'.aot.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
                assert not any(q in imports.lower() for q in ['xl.dll','xlc.dll','libxl.so','libxlc.so'])
        r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items()) and all(sha(q['Path'])==q['Sha256'].lower() for q in build['Inputs']+build['Objects']+build['Headers'])
        r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json']
        r['Complete']=r['InputsUnchanged'];r['Stages']=len(r['Rows']);save();assert r['Complete']
    except Exception as e:r['Failures'].append(repr(e));save();raise
if __name__=='__main__':main()
