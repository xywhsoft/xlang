"""Physical two-root C ABI proof only. The network operation is never invoked."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();tcc=Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve()
    case=root/'test/cases/xlir_tls_combined_linkage.c'
    paths=[Path(__file__).resolve(),gcc,tcc,case,*[root/q for q in ['lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/include/xrt_decl.h','release/include/xruntime_decl.h']]]
    r=dict(Complete=False,Inputs={str(q):sha(q) for q in paths},Rows=[],Failures=[],NetworkTraffic=False,SecurityTests=False,CalledNetworkOperation=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def run(name,args):
        args=list(map(str,args));command=args
        if Path(args[0]) in [gcc,tcc]:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n',encoding='utf8');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=900).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True);assert not code,str(log)
    flags=['-std=c11','-I'+str(root),'-I'+str(root/'release'),'-DXRT_MODULE_TLS_STREAM_DIAL_FUTURE','-DXRT_MODULE_TLS_STREAM_DIAL_PROXY']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    try:
        for product in ['core','runtime']:
            source=out/(product+'.c');source.write_text('#define '+('XRT_IMPLEMENTATION' if product=='core' else 'XRUNTIME_IMPLEMENTATION')+'\n#include "lib/'+('xrt/xrt.h' if product=='core' else 'xruntime/xruntime.h')+'"\n',encoding='utf8')
            for label,cc,options in [('O0',gcc,['-O0']),('O2',gcc,['-O2']),('tcc',tcc,[])]:
                name=product+'.'+label;obj=out/(name+'.o');binary=out/(name+('.exe' if os.name=='nt' else ''));strict=['-Wall','-Wextra','-Werror'] if cc==gcc else []
                run(name+'.implementation',[cc,*flags,*options,*strict,'-c',source,'-o',obj])
                run(name+'.link',[cc,*flags,*options,*strict,*(['-DXL6_OBSERVER_CORE'] if product=='core' else []),case,obj,*libraries,'-o',binary]);run(name+'.run',[binary])
        r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items());r['Complete']=r['InputsUnchanged'];r['Stages']=len(r['Rows']);save();assert r['Complete']
    except Exception as e:r['Failures'].append(repr(e));save();raise
if __name__=='__main__':main()
