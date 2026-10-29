#!/usr/bin/env python3
"""Normal loopback mail, offline callback projection and dispatch ownership.
No external endpoints, real TLS handshake, security or protocol fuzz tests.
"""
from pathlib import Path
import argparse, hashlib, json, os, subprocess, time

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    ap.add_argument('--xl',type=Path,required=True)
    ap.add_argument('--compiler',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True)
    args=ap.parse_args();root=args.root.resolve();xl=args.xl.resolve();cc=args.compiler.resolve();out=args.out.resolve()
    out.mkdir(parents=True,exist_ok=False);package=root/'release/extlibs/xmail'
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    inputs={str(p):sha(p) for p in package.rglob('*') if p.is_file()}
    for p in [xl,cc,Path(__file__).resolve(),root/'test/cases/extlib_xmail.xl',root/'test/cases/xmail_abi_declarations.xl']:
        inputs[str(p)]=sha(p)
    for name in (['xl.dll','xlc.dll'] if os.name=='nt' else ['libxl.so','libxlc.so']):
        p=xl.parent/name
        if p.exists():inputs[str(p)]=sha(p)
    env=dict(os.environ,XLANG_PACKAGE_PATH=str(root/'release/extlibs'));rows=[]
    report=dict(complete=False,inputs=inputs,rows=rows,externalEndpoints=False,tlsHandshakes=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=list(map(str,command));start=time.monotonic()
        r=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        rows.append(dict(name=name,exit=r.returncode,seconds=time.monotonic()-start,command=command,log=str(log),sha256=sha(log)))
        save();print(name,r.returncode,flush=True)
        if r.returncode:raise RuntimeError(str(log))
    ext='.exe' if os.name=='nt' else ''
    shared='.dll' if os.name=='nt' else '.so'
    run('all-101-ABI-declarations.build',[xl,'build',root/'test/cases/xmail_abi_declarations.xl','-shared','-o',out/('abi'+shared)])
    libs=['-lws2_32','-liphlpapi','-lshell32','-ladvapi32','-lole32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    for opt in ['O0','O2']:
        product=out/('handoff-'+opt+ext)
        run('handoff-'+opt+'.build',[cc,'-std=c11','-'+opt,'-Wall','-Wextra','-Werror',
            '-ffunction-sections','-fdata-sections','-fno-unwind-tables','-fno-asynchronous-unwind-tables',
            *([] if os.name=='nt' else ['-D_GNU_SOURCE']),'-I'+str(root),'-I'+str(root/'release'),
            '-I'+str(root/'release/include'),'-I'+str(root/'lib/xrt'),'-I'+str(package/'include'),
            package/'test/native_handoff_lifetime.c','-Wl,--gc-sections',*libs,'-o',product])
        run('handoff-'+opt+'.run',[product])
    cases=[root/'test/cases/extlib_xmail.xl',package/'test/xmail_test.xl',
           *[package/('test/xmail_'+proto+'_loopback_test.xl') for proto in ['smtp','pop3','imap']],
           package/'test/context_projection.xl']
    for case in cases:
        name=case.stem;product=out/(name+ext)
        run(name+'.jit',[xl,'run',case]);run(name+'.build',[xl,'build',case,'-o',product]);run(name+'.aot',[product])
    report['inputsUnchanged']=all(Path(p).is_file() and sha(Path(p))==digest for p,digest in inputs.items())
    assert report['inputsUnchanged'];report['complete']=True;save()

if __name__=='__main__':main()
