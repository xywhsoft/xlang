"""Owned TaskNet native-base acceptance in both unchanged clock ABIs.
Only Engine tasks/timers, typed ownership graphs and allocator rollback; no
application sockets, DNS, endpoints, protocol samples or security tests.
No shipping DLL/SO/VFS or complete language TaskNet integration is claimed.
"""
from pathlib import Path
import argparse,hashlib,importlib.util,json,os,shutil,subprocess,sys,time
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--xrt-root',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',action='append',required=True)
    p.add_argument('--sanitize',action='store_true')
    a=p.parse_args();root=a.root.resolve();native=a.xrt_root.resolve();out=a.out.resolve()
    assert not out.exists();assert not a.sanitize or os.name!='nt'
    compilers=[Path(shutil.which(c) or c).resolve() for c in a.compiler]
    sys.path.insert(0,str(native/'tools'))
    import build,amalgamate
    selected=build._load_modules('memory_debug,task_net',[],platform='windows' if os.name=='nt' else 'linux')
    sources,defines=selected[0],selected[4]
    links=['-l'+x for x in selected[5]]
    if os.name!='nt':links+=['-pthread','-ldl','-lm']
    else:links+=['-ladvapi32','-liphlpapi']
    files={Path(__file__).resolve(),*compilers,root/'test/support/net_task_owned_plan.c',
        root/'test/support/net_task_owned_lifetime.c',root/'test/support/net_task_owned_test_clock.h',
        root/'src/runtime/xl6_ownership_plan.c',root/'src/runtime/xl6_ownership_plan.h',
        root/'src/runtime/xl6_code_lease.h',root/'src/runtime/xl6_code_owner.h'}
    for folder in (root/'lib/xrt',root/'lib/xruntime',native/'src',native/'include',native/'tools',native/'config'):
        files.update(f for f in folder.rglob('*') if f.is_file() and f.suffix in ('.c','.h','.inc','.py','.json'))
    sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    inputs={str(f):sha(f) for f in sorted(files)};rows=[];products=[];failures=[];out.mkdir(parents=True)
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,ProductsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Failures=failures,
        Scope=__doc__,NativeSources=sources,ClockContractsPreserved=True,NetworkTraffic=False,NetworkSecurityTests=False,ShippingLibraryAcceptance=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,args,proof=None):
        log=out/(name+'.log');env=dict(os.environ)
        if 'asan' in name:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        start=time.monotonic()
        with log.open('wb') as f:r=subprocess.run(list(map(str,args)),cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=120,env=env)
        rows.append(dict(Name=name,Command=list(map(str,args)),Exit=r.returncode,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save()
        print(name,r.returncode,flush=True);assert r.returncode==0,log
        text=log.read_text(encoding='utf8',errors='replace')
        if proof:assert proof in text,log
    save()
    try:
        header=amalgamate._content([])[1]
        frozen=out/'native-headers';(frozen/'lib/xrt').mkdir(parents=True)
        for file in (frozen/'task-net-native.h',frozen/'lib/xrt/xrt.h'):
            file.write_text(header,encoding='utf8',newline='\n');product(file)
        proof='TaskNet real XLIR plans: cycles=80 runs=48 data-drops=80 payload-drops=32 finalizers=80 freed=80 refusals=32 resurrections=40 pending-graphs=80'
        for lane in ('pinned-core','pinned-runtime','native-single','native-modular'):
            folder=out/lane;folder.mkdir();impl=folder/'impl.c'
            if lane.startswith('pinned'):
                definition='XRUNTIME_IMPLEMENTATION' if lane.endswith('runtime') else 'XRT_IMPLEMENTATION'
                include='lib/xruntime/xruntime.h' if lane.endswith('runtime') else 'lib/xrt/xrt.h'
                prefix=('#include "lib/xruntime/xruntime_features.h"\n#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n' if lane.endswith('runtime') else '')
                impl.write_text('#define XRT_MODULE_MEMORY_DEBUG\n#define XRT_MODULE_TASK_NET\n#define '+definition+'\n'+prefix+'#include "'+include+'"\n',encoding='utf8');product(impl)
                native_flags=[];plan_include=['-I'+str(root)]
            else:
                native_flags=['-DXL6_TASK_NET_NATIVE_TIME']
                plan_include=['-I'+str(frozen),'-I'+str(root),'-I'+str(native/'include')]
                impl.write_text('#define XRT_MODULE_MEMORY_DEBUG\n#define XRT_MODULE_TASK_NET\n#define XRT_IMPLEMENTATION\n#include "task-net-native.h"\n',encoding='utf8');product(impl)
            production=[native/s for s in sources] if lane=='native-modular' else [impl]
            production_flags=['-D'+d for d in defines] if lane=='native-modular' else []
            for index,cc in enumerate(compilers):
                for opt in ('O0','O2'):
                    if cc.stem.lower()=='tcc' and opt=='O2':continue
                    tag=f'{lane}.{index}.{opt}';binary=folder/(str(index)+'.'+opt+('.exe' if os.name=='nt' else ''))
                    flags=[cc,'-std=c11','-Wall','-Werror','-'+opt,*plan_include,*native_flags,*production_flags]
                    if cc.stem.lower()!='tcc':flags+=['-Wextra']
                    if os.name!='nt':flags+=['-D_GNU_SOURCE']
                    run(tag+'.build',flags+[root/'test/support/net_task_owned_plan.c',root/'src/runtime/xl6_ownership_plan.c',*production,*links,'-o',binary]);product(binary)
                    run(tag+'.run',[binary],proof)
                    lifetime=binary.with_name(binary.stem+'.lifetime'+binary.suffix)
                    run(tag+'.lifetime.build',flags+[root/'test/support/net_task_owned_lifetime.c',*production,*links,'-o',lifetime]);product(lifetime)
                    run(tag+'.lifetime.run',[lifetime],'TaskNet owned lifetime: budgets=64 failures=12 precancel=16 closed=16')
            if a.sanitize:
                cc=next(c for c in compilers if c.stem.lower()!='tcc');tag=lane+'.asan';binary=folder/'asan'
                flags=[cc,'-std=c11','-Wall','-Wextra','-Werror','-O1','-g','-D_GNU_SOURCE',
                    '-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie',
                    *plan_include,*native_flags,*production_flags]
                run(tag+'.build',flags+[root/'test/support/net_task_owned_plan.c',root/'src/runtime/xl6_ownership_plan.c',*production,*links,'-o',binary]);product(binary)
                run(tag+'.run',[binary],proof)
                lifetime=folder/'asan.lifetime'
                run(tag+'.lifetime.build',flags+[root/'test/support/net_task_owned_lifetime.c',*production,*links,'-o',lifetime]);product(lifetime)
                run(tag+'.lifetime.run',[lifetime],'TaskNet owned lifetime: budgets=64 failures=12 precancel=16 closed=16')
        report['InputsUnchanged']=all(sha(f)==h for f,h in inputs.items())
        report['LogsUnchanged']=all(sha(r['Log'])==r['Sha256'] for r in rows)
        report['ProductsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in products)
        assert report['InputsUnchanged'] and report['LogsUnchanged'] and report['ProductsUnchanged']
        report['Complete']=True;save()
    except BaseException as error:failures.append(repr(error));save();raise
if __name__=='__main__':main()

