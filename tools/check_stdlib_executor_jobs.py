"""Offline resident detached callback/error/ownership acceptance.

Actual deployment-selected XRT/xruntime implementation and resident providers;
no shipping library fallback. Not public-language Executor/TaskPool lifecycle
or full DLL/JIT/AOT acceptance. No network/security tests.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',action='append',required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root=a.root.resolve();out=a.out.resolve();assert not out.exists(),'Preserve evidence; choose a fresh directory'
    compilers=[Path(shutil.which(name) or name).resolve() for name in a.compiler];gcc=next(c for c in compilers if c.stem.lower()!='tcc')
    assert not a.sanitize or os.name!='nt'
    def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
    files={Path(__file__).resolve(),root/'tools/check_stdlib_shipping_sources.py',root/'build_linux.sh',root/'build_dll.bat',
        root/'tools/compiler_dll.rsp.in',root/'tools/runtime_dll.rsp.in',root/'tools/build_xlir_public_source_driver.ps1',
        *compilers,*[root/('test/support/'+n+'.c') for n in ('executor_jobs_native','executor_jobs_minimal','executor_jobs_plan','executor_catalog_plan')]}
    for folder in ('src','lib/xrt','lib/xruntime','release/include'):
        files.update(f for f in (root/folder).rglob('*') if f.is_file() and f.suffix in ('.c','.h','.inc'))
    inputs={str(f):sha(f) for f in sorted(files)};out.mkdir(parents=True);rows=[];products=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Failures=failures,
        Sources=[],PublicLanguageApiAccepted=False,ManagedFinalOwnerAccepted=False,DllJitAotAccepted=False,NetworkTests=False,
        NetworkSecurityTests=False,InstrumentedSelectedClosure=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(f):products.append(dict(Path=str(f),Sha256=sha(f)));save()
    def run(name,args,proof=None,sanitized=False,reject=False):
        log=out/(name+'.log');started=time.monotonic();env=dict(os.environ)
        if sanitized:env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with log.open('wb') as stream:r=subprocess.run(list(map(str,args)),cwd=root,stdout=stream,stderr=subprocess.STDOUT,timeout=180,env=env)
        rows.append(dict(Name=name,Command=list(map(str,args)),Exit=r.returncode,ExpectedRejection=reject,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started));save()
        print(name+': '+str(r.returncode),flush=True);assert (r.returncode!=0 if reject else r.returncode==0),(name,str(log))
        text=log.read_text(encoding='utf8',errors='replace')
        if reject:assert 'assert' in text.lower(),(name,'unexpected rejection')
        if proof:assert proof in text,(name,'missing proof')
        return text
    def common(cc):
        flags=[cc,'-std=c11','-Wall','-Werror','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(out)]
        if cc.stem.lower()!='tcc':flags+=['-Wextra']
        if os.name!='nt':flags+=['-D_GNU_SOURCE']
        return flags
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm'];ext='.exe' if os.name=='nt' else ''
    save()
    try:
        for opt in ('O0','O2'):
            binary=out/('plan.'+opt+ext)
            flags=common(gcc)+['-'+opt,'-Wno-missing-field-initializers','-ffunction-sections','-fdata-sections',
                '-fno-asynchronous-unwind-tables','-fno-unwind-tables']+(['-fno-jump-tables'] if os.name=='nt' else [])
            run('plan.'+opt+'.build',flags+[root/'test/support/executor_jobs_plan.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',binary]);product(binary)
            run('plan.'+opt+'.run',[binary],'Executor jobs plan: exact producer/native domains, no TaskPool/default async dependency or implicit producer')
            if opt=='O0':
                sources=run('sources',[binary,'--sources']).splitlines();assert len(sources)==len(set(sources))
                assert 'src/runtime/xl6_executor_jobs.c' in sources and 'src/runtime/xl6_task.c' not in sources
                report['Sources']=sources;save();modules=out/'executor-jobs-modules.h'
                text=run('modules',[binary,'--modules']);assert '_MODULE_ALL' not in text;modules.write_text(text,encoding='utf8');product(modules)
        providers=[root/path for path in sources]
        proof='Executor jobs: 64 checked body/drop and code-lease outcomes, 8 pinned graphs, 264 parallel jobs, 24 cancelled captures, 8 rejection/abandoned-reader rounds, 8 caller fault prefixes; exact memory balance'
        for index,cc in enumerate(compilers):
            for opt in ('O0','O2'):
                for fixture,expected in (('minimal','Executor jobs minimal: actual planned sources/selectors and native implementation only'),('native',proof)):
                    name=str(index)+'.'+opt+'.'+fixture;binary=out/(name+ext)
                    run(name+'.build',common(cc)+['-'+opt,root/('test/support/executor_jobs_'+fixture+'.c'),*providers,*libs,'-o',binary]);product(binary)
                    run(name+'.run',[binary],expected)
        controls=[('cleanup-bool-lost','xl6_closure.c',
            '            xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});',
            '            if(!xrtGetError())xrtSetErrorKind(XERR_STATE);'),
            ('null-first-replaced','xl6_execution_failures.c','if(!state->count){state->error=result.error;result.error=NULL;}',
            'if(!state->error){state->error=result.error;result.error=NULL;}'),
            ('cancel-counted-as-failure','xl6_detached_work.c','job->references=1;job->body.ok=true;job->callback=',
            'job->references=1;job->body.ok=false;job->callback='),
            ('boxed-null-captured-late','xl6_closure.c','if(!ok)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});',
             '(void)ok;'),
            ('boxed-cancel-counted-as-failure','xl6_detached_work.c','job->references=1;job->body.ok=true;job->boxed=',
             'job->references=1;job->body.ok=false;job->boxed='),
            ('callback-null-observed-late','src/api/xl_foreign_error.c',
             '    xl6ErrorScopeObserveFailure(original);\n    if (original == NULL)\n        original = xrtErrorCreate(XERR_STATE, "xlang", 0,\n            "language callback failed without an error");',
             '    if (original == NULL)\n        original = xrtErrorCreate(XERR_STATE, "xlang", 0,\n            "language callback failed without an error");\n    xl6ErrorScopeObserveFailure(original);')]
        report['NegativeControls']=[]
        for name,filename,old,new in controls:
            source=root/filename if '/' in filename else root/'src/runtime'/filename
            body=source.read_text(encoding='utf-8-sig');assert body.count(old)==1,(name,'mutation not exact')
            mutant=out/(name+'.c');mutant.write_text(body.replace(old,new),encoding='utf8');product(mutant)
            selected=[mutant if path==source else path for path in providers];binary=out/(name+ext)
            run(name+'.build',common(gcc)+['-O0','-I'+str(source.parent),root/'test/support/executor_jobs_native.c',*selected,*libs,'-o',binary]);product(binary)
            run(name+'.refused',[binary],reject=True);report['NegativeControls'].append(name);save()
        if a.sanitize:
            binary=out/'instrumented'
            run('instrumented.build',common(gcc)+['-O1','-g','-fno-omit-frame-pointer','-fsanitize=address,undefined','-fno-pie','-no-pie',
                root/'test/support/executor_jobs_native.c',*providers,*libs,'-o',binary]);product(binary)
            run('instrumented.run',[binary],proof,True);report['InstrumentedSelectedClosure']=True
        shipping=json.loads(run('shipping-sources',['python' if os.name=='nt' else 'python3',root/'tools/check_stdlib_shipping_sources.py','--root',root]));assert shipping['Complete'];report['ShippingSources']=shipping
        report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
        report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
        assert report['InputsUnchanged'] and report['LogsUnchanged'];report['Complete']=True;save()
    except BaseException as error:failures.append(repr(error));save();raise
if __name__=='__main__':main()
