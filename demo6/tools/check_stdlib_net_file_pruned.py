#!/usr/bin/env python3
"""Native execution-family component pruning; owned ordinary files only."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--out',type=Path,required=True);p.add_argument('--compiler',action='append',required=True)
    p.add_argument('--sanitize',action='store_true');a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists()
    compilers=[Path(shutil.which(c) or c).resolve() for c in a.compiler];gcc=next(c for c in compilers if c.stem.lower()!='tcc')
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    inputs={str(f):sha(f) for folder in ['src','lib/xrt','lib/xruntime','release/include'] for f in (root/folder).rglob('*') if f.is_file()}
    inputs.update({str(f):sha(f) for f in [Path(__file__).resolve(),*compilers,*[root/f'test/support/{n}.c' for n in ['net_file_execution_pruned','net_file_execution_plan_manifest']]]})
    rows=[];products=[];out.mkdir(parents=True)
    report=dict(Complete=False,Inputs=inputs,Rows=rows,Products=products,Failures=[],ApplicationNetworkTraffic=False,NetworkSecurityTests=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def product(p):products.append(dict(Path=str(p),Sha256=sha(p)));save()
    def run(name,args):
        cwd=out/(name+'.cwd');cwd.mkdir();log=out/(name+'.log');start=time.monotonic()
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with log.open('wb') as stream:r=subprocess.run(list(map(str,args)),cwd=cwd,stdout=stream,stderr=subprocess.STDOUT,timeout=180,env=env)
        rows.append(dict(Name=name,Exit=r.returncode,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,r.returncode,flush=True)
        assert not r.returncode,str(log);return log.read_text(encoding='utf-8',errors='replace')
    def flags(cc):return [cc,'-std=c11','-Wall','-Werror',*(['-Wextra'] if cc.stem.lower()!='tcc' else []),'-I'+str(root),'-I'+str(root/'release'),'-I'+str(out),*(['-D_GNU_SOURCE'] if os.name!='nt' else [])]
    save()
    try:
        manifest=out/('manifest.exe' if os.name=='nt' else 'manifest.out')
        run('manifest.build',flags(gcc)+['-O0','-ffunction-sections','-fdata-sections','-Wno-missing-field-initializers','-fno-unwind-tables','-fno-asynchronous-unwind-tables',*(['-fno-jump-tables'] if os.name=='nt' else []),root/'test/support/net_file_execution_plan_manifest.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',manifest]);product(manifest)
        run('manifest.run',[manifest]);sources=run('manifest.sources',[manifest,'--sources']).splitlines()
        modules=run('manifest.modules',[manifest,'--modules']);assert 'XRT_MODULE_NET_FILE_FUTURE' in modules and '_MODULE_ALL' not in modules
        header=out/'net-file-execution-modules.h';header.write_text(modules,encoding='utf-8');product(header)
        report['Sources']=sources;report['Modules']=modules;save()
        links=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
        for i,cc in enumerate(compilers):
            modes=['O0','O2'] if cc.stem.lower()!='tcc' else ['O0']
            if a.sanitize and cc.stem.lower()!='tcc':modes.append('asan')
            for mode in modes:
                tag=str(i)+'.'+mode;binary=out/(tag+('.exe' if os.name=='nt' else '.out'))
                opt=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie'] if mode=='asan' else ['-'+mode]
                run(tag+'.build',flags(cc)+opt+[root/'test/support/net_file_execution_pruned.c',*[root/s for s in sources],*links,'-o',binary]);product(binary)
                run(tag+'.run',[binary])
                nm=Path(shutil.which('nm') or gcc.parent/('nm.exe' if os.name=='nt' else 'nm'))
                symbols=run(tag+'.symbols',[nm,'--defined-only',binary])
                assert not any(line.split() and line.split()[-1] in ['xrtTaskPoolCreate','xrtExecutorCreate','xl6TaskPoolCreateV1','xrtNetStreamConnect','xrtNetUdpOpen'] for line in symbols.splitlines())
        report['InputsUnchanged']=all(sha(f)==h for f,h in inputs.items());assert report['InputsUnchanged'];report['Complete']=True;save()
    except BaseException as e:report['Failures'].append(repr(e));save();raise
if __name__=='__main__':main()
