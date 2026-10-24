"""Isolated private-source NULL/no-diagnostic boundary, no network traffic.

The private source is a correctly typed unit stub, NOT an object override of
the full-source build. Optionally prove the previous wrapper loses the bit.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
p=argparse.ArgumentParser(description=__doc__)
for name in ['root','support','out']:p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--old-root',type=Path);p.add_argument('--compiler',action='append',required=True);a=p.parse_args()
root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
sha=lambda q:hashlib.sha256(Path(q).read_bytes()).hexdigest()
compilers=[Path(shutil.which(q) or q).resolve() for q in a.compiler]
wrappers=[('current',root/'src/runtime/xl6_tls_worker_async.c',0)]
if a.old_root:wrappers.append(('previous',a.old_root/'src/runtime/xl6_tls_worker_async.c',1))
files={Path(__file__).resolve(),a.support.resolve(),root/'src/api/xl_foreign_error.c',*compilers,*[q for q in (root/'src').rglob('*') if q.is_file() and q.suffix in ['.h','.inc']],root/'lib/xrt/xrt.h',root/'release/include/xrt_decl.h',root/'release/include/xruntime_decl.h',*[q[1] for q in wrappers]}
r=dict(Complete=False,Inputs={str(q):sha(q) for q in sorted(files)},Rows=[],Products=[],Failures=[],NetworkTraffic=False,SecurityTests=False,MalformedCryptoTests=False,ShippingAccepted=False,FullTlsAccepted=False,PrivateSourceStubForBoundaryOnly=True,FullSourceBuildOverridden=False)
def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
def run(name,args,expected=0):
    args=list(map(str,args));command=args
    if Path(args[0]) in compilers:
        rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+q.replace('\\','/').replace('"','\\"')+'"' for q in args[1:])+'\n');command=[args[0],'@'+str(rsp)]
    log=out/(name+'.log');start=time.monotonic()
    with log.open('wb') as f:code=subprocess.run(command,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=120).returncode
    r['Rows'].append(dict(Name=name,Command=args,Exit=code,ExpectedReject=bool(expected),ExpectedExit=expected,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True);assert code==expected,str(log)
try:
    implementation=out/'native.c';implementation.write_text('#define XRT_MODULE_MEMORY_DEBUG\n#define XRT_MODULE_ONCE\n#define XRT_MODULE_ERROR\n#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n')
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    for index,cc in enumerate(compilers):
        modes=[('tcc',[])] if cc.stem=='tcc' else [('O0',['-O0']),('O2',['-O2'])]
        for mode,flags in modes:
            for label,wrapper,expected in wrappers:
                name=str(index)+'.'+mode+'.'+label;binary=out/(name+('.exe' if os.name=='nt' else ''))
                run(name+'.build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',*flags,'-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),*(['-D_GNU_SOURCE'] if os.name!='nt' else []),a.support,wrapper,root/'src/api/xl_foreign_error.c',implementation,*libs,'-o',binary])
                run(name+'.run',[binary],expected)
    r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items());assert r['InputsUnchanged']
    r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json'];r['Complete']=True;save()
except Exception as e:r['Failures'].append(repr(e));save();raise
