"""Offline native Executor catalog/closure tests, not public language API acceptance."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',action='append',required=True)
    p.add_argument('--plan',action='store_true',help='ELF section-GC source-plan check (requires GCC)')
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve()
    if out.exists():p.error('Preserve old evidence; choose a fresh directory')
    if a.plan and os.name=='nt':p.error('The focused plan link requires ELF section collection; use a complete public Windows build instead')
    def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
    paths=[path for folder in ('src','lib','release/include') for path in (root/folder).rglob('*')
        if path.is_file() and path.suffix in ('.c','.h','.inc')]
    paths += [root/name for name in ('test/cases/xrt_executor_ownership_native.c',
        'test/support/executor_catalog_ownership.c','test/support/executor_catalog_plan.c',
        'tools/check_stdlib_executor_catalog.py')]
    inputs={str(path):sha(path) for path in sorted(set(paths))};rows=[];logs=[];products=[];failures=[]
    out.mkdir(parents=True)
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,
        Products=products,Failures=failures,NetworkTests=False,NetworkSecurityTests=False,
        SourcePlanRequested=a.plan,SourcePlanAccepted=False,PublicLanguageApiAccepted=False,
        DllJitAotAccepted=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,args,proof=None):
        start=time.monotonic();log=out/(name+'.log')
        with log.open('wb') as stream:
            result=subprocess.run(args,cwd=root,stdout=stream,stderr=subprocess.STDOUT,timeout=120)
        digest=sha(log);logs.append((log,digest))
        rows.append(dict(Name=name,Command=list(map(str,args)),Exit=result.returncode,Sha256=digest,Seconds=time.monotonic()-start));save()
        print(name+': '+str(result.returncode),flush=True)
        if result.returncode:raise RuntimeError('Failed '+name+'; see '+str(log))
        if proof and proof not in log.read_text(encoding='utf8',errors='replace'):raise RuntimeError('Missing proof '+name)
    save()
    try:
        for index,compiler in enumerate(a.compiler):
            tcc=Path(compiler).stem.lower()=='tcc'
            common=[compiler,'-Wall','-Werror','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
            if not tcc:common+=['-std=c11','-Wextra']
            libraries=[] if os.name=='nt' else ['-pthread','-lm']
            for header in ('xrt','xruntime'):
                for opt in ('O0','O2'):
                    name=str(index)+'.'+header+'.'+opt;binary=out/(name+('.exe' if os.name=='nt' else ''))
                    flags=['-D_GNU_SOURCE'] if os.name!='nt' else []
                    if header=='xruntime':flags+=['-DXL6_EXECUTOR_TEST_RUNTIME']
                    sources=[root/('src/'+name) for name in ('runtime/xl6_ownership_catalog.c',
                        'runtime/xl6_ownership_domain_native_executor.c','api/xl_foreign_error.c')]
                    run(name+'.build',common+['-'+opt,*flags,str(root/'test/support/executor_catalog_ownership.c'),
                        *map(str,sources),*libraries,'-o',str(binary)])
                    run(name+'.run',[str(binary)],'Executor catalog: 64 queued graphs, 32 busy phases, typed identity refusal, sealed admission and 16 allocation prefixes')
                    products.append(dict(Path=str(binary),Sha256=sha(binary)));save()
            if a.plan and not tcc:
                for opt in ('O0','O2'):
                    name=str(index)+'.plan.'+opt;binary=out/name
                    run(name+'.build',common+['-'+opt,'-Wno-missing-field-initializers','-ffunction-sections','-fdata-sections',
                        str(root/'test/support/executor_catalog_plan.c'),str(root/'src/compile/xl6_compile_unit.c'),
                        '-Wl,--gc-sections','-o',str(binary)])
                    run(name+'.run',[str(binary)],'Executor catalog plan: independent native domain, default async pruning, exact symbol owners')
                    products.append(dict(Path=str(binary),Sha256=sha(binary)));save()
                report['SourcePlanAccepted']=True
        if a.plan and not report['SourcePlanAccepted']:raise RuntimeError('--plan requires a GCC compiler lane')
        report['InputsUnchanged']=all(sha(Path(path))==digest for path,digest in inputs.items())
        report['LogsUnchanged']=all(sha(path)==digest for path,digest in logs)
        if not report['InputsUnchanged'] or not report['LogsUnchanged']:raise RuntimeError('Input/log drift')
        report['Complete']=True;save()
    except BaseException as error:
        failures.append(repr(error));report['InputsUnchanged']=all(sha(Path(path))==digest for path,digest in inputs.items());save();raise

if __name__=='__main__':main()
