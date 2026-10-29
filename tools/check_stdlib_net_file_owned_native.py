#!/usr/bin/env python3
"""Compile/run the actual XRT owned-file fixture; regular files, no sockets."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time

def main():
    p=argparse.ArgumentParser();p.add_argument('--native',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True);p.add_argument('--compiler',action='append',required=True)
    p.add_argument('--sanitize',action='store_true')
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    a=p.parse_args();root=a.native.resolve();out=a.out.resolve()
    assert not out.exists();out.mkdir(parents=True)
    sys.path.insert(0,str(root/'tools'));import build,amalgamate
    selected=build._load_modules('net_file_future,memory_debug',[],platform='windows' if os.name=='nt' else 'linux')
    digest=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    compilers=[Path(shutil.which(c) or c).resolve() for c in a.compiler]
    inputs={str(f):digest(f) for folder in ('src','include','config','tools','tests')
        for f in sorted((root/folder).rglob('*')) if f.is_file() and f.suffix in ('.c','.h','.json','.py')}
    inputs.update({str(f):digest(f) for f in [Path(__file__).resolve(),*compilers]})
    pinned=a.root.resolve()
    for n in ['lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/include/xrt_decl.h','release/include/xruntime_decl.h']:
        inputs[str(pinned/n)]=digest(pinned/n)
    rows=[];products=[];report=dict(Complete=False,Inputs=inputs,Rows=rows,Products=products,Failures=[],
        ApplicationNetworkTraffic=False,NetworkSecurityTests=False)
    def save(): (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        cwd=out/(name+'.cwd');cwd.mkdir();log=out/(name+'.log');started=time.monotonic()
        with log.open('wb') as stream:
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            r=subprocess.run(list(map(str,command)),cwd=cwd,stdout=stream,stderr=subprocess.STDOUT,timeout=180,env=env)
        rows.append(dict(Name=name,Exit=r.returncode,Command=list(map(str,command)),Log=str(log),
            Sha256=digest(log),Seconds=time.monotonic()-started));save();print(name,r.returncode,flush=True)
        assert r.returncode==0,str(log)
    save()
    try:
        header=out/'owned-file-single.h';header.write_text(amalgamate._content([])[1],encoding='utf-8')
        impl=out/'single.c';impl.write_text('#define XRT_MODULE_NET_FILE_FUTURE\n#define XRT_MODULE_MEMORY_DEBUG\n#define XRT_IMPLEMENTATION\n#include "owned-file-single.h"\n',encoding='utf-8')
        for f in [header,impl]:products.append(dict(Path=str(f),Sha256=digest(f)))
        for lane in ['modular','single','pinned-core','pinned-runtime']:
          for i,cc in enumerate(compilers):
            modes=['O0','O2'] if cc.stem.lower()!='tcc' else ['O0']
            if a.sanitize and cc.stem.lower()!='tcc':modes.append('asan')
            for mode in modes:
                stem=f'{lane}-{i}-{mode}';binary=out/(stem+('.exe' if os.name=='nt' else '.out'))
                opt=['-O1','-g','-fno-omit-frame-pointer','-fsanitize=address,undefined','-no-pie'] if mode=='asan' else ['-'+mode]
                flags=[cc,'-std=c11','-Wall','-Werror',*(['-Wextra'] if cc.stem.lower()!='tcc' else []),
                    *opt,*(['-D_GNU_SOURCE'] if os.name!='nt' else []),'-I'+str(root/'include'),
                    *['-D'+d for d in selected[4]]]
                links=['-l'+n for n in selected[5]]+(['-ladvapi32','-liphlpapi'] if os.name=='nt' else ['-pthread','-ldl','-lm'])
                sources=[root/s for s in selected[0]] if lane=='modular' else [impl]
                fixture=root/'tests/network/test_net_file_future.c'
                if lane.startswith('pinned'):
                    product='xruntime' if lane=='pinned-runtime' else 'xrt'
                    localImpl=out/(lane+'-impl.c');localFixture=out/(lane+'-fixture.c')
                    headerName=f'lib/{product}/{product}.h'
                    localImpl.write_text(f'#define XRT_MODULE_NET_FILE_FUTURE\n#define XRT_MODULE_MEMORY_DEBUG\n#define {product.upper()}_IMPLEMENTATION\n#include "{headerName}"\n',encoding='utf-8')
                    localFixture.write_text(f'#define XRT_MODULE_NET_FILE_FUTURE\n#define XRT_MODULE_MEMORY_DEBUG\n#include "{headerName}"\n#include "{fixture.as_posix()}"\n',encoding='utf-8')
                    flags+=['-I'+str(pinned)];sources=[localImpl];fixture=localFixture
                run(stem+'.build',flags+[fixture,*sources,*links,'-o',binary])
                products.append(dict(Path=str(binary),Sha256=digest(binary)));save()
                run(stem+'.run',[binary])
        report['InputsUnchanged']=all(digest(f)==h for f,h in inputs.items())
        report['LogsUnchanged']=all(digest(r['Log'])==r['Sha256'] for r in rows)
        report['ProductsUnchanged']=all(digest(r['Path'])==r['Sha256'] for r in products)
        assert report['InputsUnchanged'] and report['LogsUnchanged'] and report['ProductsUnchanged']
        report['Complete']=True;save()
    except BaseException as e: report['Failures'].append(repr(e));save();raise
if __name__=='__main__':main()
