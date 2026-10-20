"""Offline exact numeric/enum domain resident providers and pruned closures."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--compiler',action='append',required=True)
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--discovery',action='store_true')
    args=parser.parse_args();root=Path(__file__).resolve().parents[1];out=args.out.resolve()
    assert not out.exists() and (not args.sanitize or os.name!='nt')
    cc=[Path(shutil.which(c) or c).resolve() for c in args.compiler]
    plan=root/'test/support/numeric_value_plan.c';fixture=root/'test/support/numeric_value_runtime.c'
    files={Path(__file__).resolve(),plan,fixture,root/'test/support/executor_catalog_plan.c',*cc}
    for area in ('src','lib/xrt','lib/xruntime','release/include'):
        files.update(p for p in (root/area).rglob('*') if p.is_file() and p.suffix in ('.c','.h','.inc'))
    inputs={str(p):sha(p) for p in sorted(files)};out.mkdir(parents=True);rows=[];plans={}
    report=dict(Complete=False,Inputs=inputs,Rows=rows,Plans=plans,InputsUnchanged=False,LogsUnchanged=False,
                NetworkTests=False,ShippingLibraryAcceptance=False,DiscoveryOnly=args.discovery)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,sanitized=False):
        env=dict(os.environ)
        if sanitized:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log=out/(name+'.log')
        with log.open('wb') as stream:
            result=subprocess.run(list(map(str,command)),cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=result.returncode,Log=str(log),Sha256=sha(log)))
        save();print(name+': '+str(result.returncode),flush=True)
        assert not result.returncode,log.read_text(errors='replace')[-4000:]
        return log.read_text(encoding='utf-8',errors='replace')
    def flags(c):
        result=[c,'-Wall','-Werror','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(out)]
        if c.stem.lower()!='tcc':result+=['-std=c11','-Wextra']
        if os.name!='nt':result+=['-D_GNU_SOURCE']
        return result
    libraries=[] if os.name=='nt' else ['-pthread','-ldl','-lm'];extension='.exe' if os.name=='nt' else ''
    save()
    try:
        binary=out/('plan'+extension)
        run('plan.build',[*flags(cc[0]),'-O0','-Wno-missing-field-initializers','-ffunction-sections',
            '-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables',
            plan,root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',binary])
        run('plan.run',[binary])
        for index,name in enumerate(('numeric','bind','add')):
            api=run(name+'.api',[binary,'--api-sources',name]).splitlines()
            sources=run(name+'.sources',[binary,'--sources',name]).splitlines()
            modules=run(name+'.modules',[binary,'--modules',name]);assert '_MODULE_ALL' not in modules
            plans[name]=dict(ApiSources=api,TestSources=sources);save()
            impl=out/(name+'.impl.c')
            impl.write_text(modules+'\n#define XRT_MODULE_MEMORY_DEBUG\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
            for cindex,c in enumerate(cc):
                modes=[('O0',[])] if args.discovery else [('O0',[]),('O2',[])]
                if args.sanitize and cindex==0:
                    modes.append(('instrumented',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
                for opt,extra in modes:
                    label=f'{name}.{cindex}.{opt}';executable=out/(label+extension)
                    run(label+'.build',[*flags(c),*(extra or ['-'+opt]),'-DTEST_ROOT='+str(index),fixture,impl,
                        *[root/p for p in sources],*libraries,'-o',executable])
                    run(label+'.run',[executable],opt=='instrumented')
        report['InputsUnchanged']=all(sha(p)==h for p,h in inputs.items())
        report['LogsUnchanged']=all(sha(r['Log'])==r['Sha256'] for r in rows)
        report['Complete']=report['InputsUnchanged'] and report['LogsUnchanged'];save();assert report['Complete']
    except BaseException as error:
        report['Error']=repr(error);save();raise

if __name__=='__main__':main()
