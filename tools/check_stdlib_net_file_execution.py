#!/usr/bin/env python3
"""Actual source-built XLIR NetFile resource/retirement graphs, offline files."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--source-build',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--native-root',type=Path);a=p.parse_args()
    root=a.root.resolve();manifest=a.source_build.resolve();out=a.out.resolve();assert not out.exists()
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    build=json.loads(manifest.read_text(encoding='utf-8-sig'));assert build['SourceXrt'] and not build['RuntimeOnly']
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    fixture=root/'test/support/net_file_execution_plan.c'
    inputs={str(p):sha(p) for p in [Path(__file__).resolve(),manifest,fixture,cc]}
    rows=[];products=[];out.mkdir(parents=True);env=dict(os.environ)
    if os.name=='nt':env['PATH']=str(root/'release')+os.pathsep+env.get('PATH','')
    report=dict(Complete=False,Inputs=inputs,SourceBuild=str(manifest),Rows=rows,Products=products,Failures=[],
        ApplicationNetworkTraffic=False,NetworkSecurityTests=False,LanguageNetFileComplete=False,ShippingAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def product(p):products.append(dict(Path=str(p),Sha256=sha(p)));save()
    def run(name,args):
        log=out/(name+'.log');cwd=out/(name+'.cwd');cwd.mkdir();start=time.monotonic()
        with log.open('wb') as stream:r=subprocess.run(list(map(str,args)),cwd=cwd,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=120)
        rows.append(dict(Name=name,Exit=r.returncode,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save()
        print(name,r.returncode,flush=True);assert r.returncode==0,str(log)
    save()
    try:
        objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
        assert len(objects)==len(build['Objects'])-1
        for mode in ['O0','O2']:
            obj=out/('plan.'+mode+'.o');binary=out/('plan.'+mode+('.exe' if os.name=='nt' else '.out'))
            flags=['-std=c11','-Wall','-Wextra','-Werror','-'+mode,'-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'include')]
            run(mode+'.build',[cc,*flags,'-c',fixture,'-o',obj]);product(obj)
            if os.name=='nt':links=[root/'release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
            else:
                assert a.native_root;lib=a.native_root.resolve()/'release'
                links=['-L'+str(lib),'-lxl','-Wl,-rpath,'+str(lib),'-Wl,--export-dynamic','-pthread','-ldl','-lm']
            response=out/(mode+'.rsp');response.write_text('\n'.join('"'+str(x).replace('\\','/')+'"' for x in [*objects,obj,*links,'-o',binary])+'\n',encoding='utf-8');product(response)
            run(mode+'.link',[cc,'@'+str(response)]);product(binary);run(mode+'.run',[binary])
        report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
        report['SourceBuildUnchanged']=all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers'])
        assert report['InputsUnchanged'] and report['SourceBuildUnchanged'];report['Complete']=True;save()
    except BaseException as e:report['Failures'].append(repr(e));save();raise
if __name__=='__main__':main()
