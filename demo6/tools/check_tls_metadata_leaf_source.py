"""Real-source leaf accessors with a synthetic owned snapshot and OOM prefixes.

No replacement functions/objects. This complements, not replaces, the full
language loopback gate. The snapshot builder and Task queue are not faulted.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['root','build','out','certificate']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    build=json.loads(a.build.read_text(encoding='utf-8-sig'));assert build['Complete'] and build['InputsUnchanged'] and build['SourceXrt'] and not build['RuntimeOnly']
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower()
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    case=Path(__file__).resolve().parents[1]/'test/cases/stdlib_tls_metadata_oom_host.c'
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    files={Path(__file__).resolve(),case,gcc,a.build.resolve(),a.certificate.resolve()}
    r=dict(Complete=False,SourceBuild=str(a.build),SourceBuildSha256=sha(a.build),Inputs={str(q):sha(q) for q in sorted(files)},Rows=[],Failures=[],
        SyntheticSnapshot=True,RealAccessors=True,AllDomainsCatalogSmoke=True,LoopbackAcceptance=False,SnapshotBuilderFaulted=False,TaskQueueFaulted=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def run(name,args):
        args=list(map(str,args));command=args
        if Path(args[0])==gcc:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n',encoding='utf8');command=[str(gcc),'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=300).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert code==0,str(log);return log.read_text(encoding='utf8',errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    objects=[q['Path'] for q in build['Objects'] if Path(q['Source']).name not in ['xlir_public_source_driver.c','xlir_public_source_linux_driver.c']];assert len(objects)==len(build['Objects'])-1
    libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)]
    try:
        for opt in ['O0','O2']:
            obj=out/(opt+'.o');binary=out/(opt+('.exe' if os.name=='nt' else ''))
            run(opt+'.compile',[gcc,*flags,'-'+opt,'-c',case,'-o',obj])
            run(opt+'.link',[gcc,*objects,obj,runtime,*libraries,'-o',binary])
            assert 'TLS metadata leaf: 96 allocation prefixes balanced;' in run(opt+'.run',[binary,a.certificate.resolve()])
        r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items()) and all(sha(q['Path'])==q['Sha256'].lower() for q in build['Inputs']+build['Objects']+build['Headers'])
        r['Complete']=r['InputsUnchanged'];r['Stages']=len(r['Rows']);save();assert r['Complete']
    except Exception as e:r['Failures'].append(repr(e));save();raise
if __name__=='__main__':main()
