"""Offline whole-compiler async deployment checks; no network or shipping claim."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-build',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True);args=p.parse_args()
    root=Path(__file__).resolve().parents[1];out=args.out.resolve();manifest=args.source_build.resolve()
    build=json.loads(manifest.read_text(encoding='utf-8-sig'));assert not out.exists() and build.get('Complete',True)
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    assert all(sha(r['Path'])==r['Sha256'].lower() for r in build['Inputs']+build['Objects']+build['Headers'])
    cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    fixture=root/'test/cases/xlir_continuation_deployment.c'
    sources=[root/('test/cases/xlir_continuation_'+name+'.xl') for name in ('unused','used')]
    inputs={str(path):sha(path) for path in [Path(__file__).resolve(),manifest,cc,fixture,
        root/'test/cases/xlir_tcp_managed_contract.c',*sources]}
    out.mkdir(parents=True);rows=[];products=[]
    report=dict(Complete=False,Inputs=inputs,Rows=rows,Products=products,SourceBuild=str(manifest),
        ExternalNetworkTests=False,NetworkSecurityTests=False,ShippingLibraryAcceptance=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,command):
        command=list(map(str,command));log=out/(name+'.log')
        with log.open('wb') as stream:r=subprocess.run(command,cwd=root,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
        rows.append(dict(Name=name,Command=command,Exit=r.returncode,Log=str(log),Sha256=sha(log)))
        save();print(name,r.returncode,flush=True);assert r.returncode==0,(name,str(log))
    objects=[r['Path'] for r in build['Objects'] if Path(r['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-I'+str(root),
        '-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(Path(build['Driver']).parent/'include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    libraries=[root/'release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
    if os.name!='nt':
        lib=Path(build['NativeRoot'])/'release';libraries=['-L'+str(lib),'-lxl','-Wl,-rpath,'+str(lib),'-Wl,--export-dynamic','-pthread','-ldl','-lm']
    else:shutil.copy2(root/'release/xl.dll',out/'xl.dll');product(out/'xl.dll')
    for opt in ('O0','O2'):
        obj=out/(opt+'.o');binary=out/(opt+('.exe' if os.name=='nt' else ''))
        run(opt+'.build',[cc,*flags,'-'+opt,'-c',fixture,'-o',obj]);product(obj)
        rsp=out/(opt+'.rsp');rsp.write_text('\n'.join('"'+str(arg).replace('\\','/')+'"'
            for arg in [*objects,obj,*libraries,'-o',binary])+'\n',encoding='utf8');product(rsp)
        run(opt+'.link',[cc,'@'+str(rsp)]);product(binary);run(opt+'.run',[binary,*sources])
    report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items()) and all(sha(r['Path'])==r['Sha256'].lower() for r in build['Inputs'])
    report['ProductsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in products)
    report['LogsUnchanged']=all(sha(r['Log'])==r['Sha256'] for r in rows)
    report['Complete']=all(report[k] for k in ('InputsUnchanged','ProductsUnchanged','LogsUnchanged'));save();assert report['Complete']

if __name__=='__main__':main()
