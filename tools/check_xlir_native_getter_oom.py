"""Complete logical current-thread allocation prefixes for native getters.

Executes unchanged compiler-emitted C against actual full-source objects. Warm
global lazy state first; no substituted providers, cold-runtime, other-thread,
shipping-library or general concurrency/OOM certification. Entirely offline.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,struct,subprocess,time
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--source-build',type=Path,required=True);p.add_argument('--native-root',type=Path)
    p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    root=a.root.resolve();manifest=a.source_build.resolve();out=a.out.resolve();assert not out.exists()
    build=json.loads(manifest.read_text(encoding='utf-8-sig'));assert build['SourceXrt'] and not build['RuntimeOnly']
    records=build['Inputs']+build['Objects']+build['Headers'];assert all(sha(r['Path'])==r['Sha256'].lower() for r in records)
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    source=root/'test/cases/xlir_native_factory_getter_oom.xl';host=root/'test/support/native_getter_oom_host.c'
    cleanupSource=root/'test/cases/xlir_native_factory_getter_cleanup.xl'
    cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    inputs={str(f):sha(f) for f in (Path(__file__).resolve(),manifest,driver,source,cleanupSource,host,cc)}
    out.mkdir(parents=True);rows=[];products=[];failures=[];env=dict(os.environ)
    if os.name!='nt':assert a.native_root;env['XL6_SOURCE_NATIVE_ROOT']=str(a.native_root.resolve())
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,
        Failures=failures,SourceBuild=str(manifest),NetworkTests=False,ShippingLibraryAcceptance=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(f):products.append(dict(Path=str(f),Sha256=sha(f)));save()
    def run(name,args,allowFailure=False):
        log=out/(name+'.log');args=list(map(str,args));start=time.monotonic()
        with log.open('wb') as stream:r=subprocess.run(args,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
        rows.append(dict(Name=name,Command=args,Exit=r.returncode,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save()
        print(name+': '+str(r.returncode),flush=True)
        if not allowFailure:assert r.returncode==0,(name,str(log))
        return r.returncode,log.read_text(encoding='utf8',errors='replace')
    save()
    try:
        generated=out/'getter.c';run('emit',[driver,source,'cgen-default',generated]);product(generated)
        flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root/'release'),
            '-I'+str(root/'release/include'),'-I'+str(driver.parent/'include'),'-include',str(generated),
            '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-unused-label','-Wno-missing-field-initializers']
        if os.name!='nt':flags+=['-D_GNU_SOURCE']
        objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in
            ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
        assert len(objects)==len(build['Objects'])-1
        if os.name=='nt':
            libs=[root/'release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
            shutil.copy2(root/'release/xl.dll',out/'xl.dll');product(out/'xl.dll')
        else:
            lib=a.native_root.resolve()/'release';libs=['-L'+str(lib),'-lxl','-Wl,-rpath,'+str(lib),'-Wl,--export-dynamic','-pthread','-ldl','-lm']
        terminal={}
        for opt in ('O0','O2'):
            obj=out/(opt+'.o');binary=out/(opt+('.exe' if os.name=='nt' else ''))
            run(opt+'.compile',[cc,*flags,'-'+opt,'-c',host,'-o',obj]);product(obj)
            link=[*objects,obj,*libs,'-o',binary];response=out/(opt+'.rsp')
            response.write_text('\n'.join('"'+str(item).replace('\\','/')+'"' for item in link)+'\n',encoding='utf8');product(response)
            run(opt+'.link',[cc,'@'+str(response)]);product(binary)
            for point in range(512):
                code,text=run(opt+'.point.'+str(point),[binary,str(point)],True)
                if code:failures.append(dict(Optimization=opt,Point=point,Exit=code));save()
                elif 'hit=0 balanced' in text:terminal[opt]=point;break
            assert opt in terminal,(opt,'logical prefix incomplete')
        report['TerminalPoints']=terminal
        cleanupGenerated=out/'cleanup.c'
        run('cleanup.emit',[driver,cleanupSource,'cgen-default',cleanupGenerated]);product(cleanupGenerated)
        for opt in ('O0','O2'):
            obj=out/('cleanup.'+opt+'.o');binary=out/('cleanup.'+opt+('.exe' if os.name=='nt' else ''))
            cleanupFlags=[str(cleanupGenerated) if item==str(generated) else item for item in flags]
            run('cleanup.'+opt+'.compile',[cc,*cleanupFlags,'-'+opt,'-c',host,'-o',obj]);product(obj)
            response=out/('cleanup.'+opt+'.rsp');link=[*objects,obj,*libs,'-o',binary]
            response.write_text('\n'.join('"'+str(item).replace('\\','/')+'"' for item in link)+'\n',encoding='utf8');product(response)
            run('cleanup.'+opt+'.link',[cc,'@'+str(response)]);product(binary)
            # No injection: prove memory balance for the deliberate receiver
            # cleanup error and original RangeError precedence in this module.
            _,proof=run('cleanup.'+opt+'.run',[binary,str((1<<(8*struct.calcsize('P')))-1)])
            assert 'hit=0 balanced' in proof
        report['InputsUnchanged']=all(sha(f)==h for f,h in inputs.items()) and all(sha(r['Path'])==r['Sha256'].lower() for r in records)
        report['LogsUnchanged']=all(sha(r['Log'])==r['Sha256'] for r in rows)
        assert report['InputsUnchanged'] and report['LogsUnchanged'] and not failures
        assert all(sha(r['Path'])==r['Sha256'] for r in products);report['Complete']=True
    except BaseException as error:report['Error']=repr(error);raise
    finally:save()
if __name__=='__main__':main()
