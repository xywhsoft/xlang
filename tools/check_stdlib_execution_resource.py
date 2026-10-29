"""Actual execution-resource ownership/last-user retirement and family pruning.

Native/runtime source scope only; not public-language class, module IR/Cgen
integration, shipping DLL/JIT/AOT or full five-group acceptance. Offline only.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--out',type=Path,required=True);p.add_argument('--compiler',action='append',required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root=a.root.resolve();out=a.out.resolve();assert not out.exists(),'Choose fresh evidence directory';assert not a.sanitize or os.name!='nt'
    compilers=[Path(shutil.which(c) or c).resolve() for c in a.compiler];gcc=next(c for c in compilers if c.stem.lower()!='tcc');nm=gcc.parent/('nm.exe' if os.name=='nt' else 'nm')
    def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
    files={Path(__file__).resolve(),root/'tools/check_stdlib_shipping_sources.py',root/'tools/compiler_dll.rsp.in',root/'tools/runtime_dll.rsp.in',
        root/'tools/build_xlir_public_source_driver.ps1',root/'build_linux.sh',root/'build_dll.bat',*compilers,nm,
        *[root/('test/support/'+name+'.c') for name in ('execution_resource_plan','execution_resource_minimal','execution_resource_native','executor_catalog_plan')]}
    for folder in ('src','lib/xrt','lib/xruntime','release/include'):
        files.update(f for f in (root/folder).rglob('*') if f.is_file() and f.suffix in ('.c','.h','.inc'))
    inputs={str(f):sha(f) for f in sorted(files)};out.mkdir(parents=True);rows=[];products=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Failures=failures,Plans={},
        PublicLanguageApiAccepted=False,ModuleIntegrationAccepted=False,DllJitAotAccepted=False,NetworkTests=False,NetworkSecurityTests=False,
        InstrumentedSelectedClosure=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,args,proof=None,sanitized=False,reject=False):
        started=time.monotonic();log=out/(name+'.log');env=dict(os.environ)
        if sanitized:env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with log.open('wb') as stream:r=subprocess.run(list(map(str,args)),cwd=root,stdout=stream,stderr=subprocess.STDOUT,timeout=180,env=env)
        rows.append(dict(Name=name,Command=list(map(str,args)),Exit=r.returncode,ExpectedRejection=reject,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started));save()
        print(name+': '+str(r.returncode),flush=True);assert (r.returncode!=0 if reject else r.returncode==0),(name,str(log))
        text=log.read_text(encoding='utf8',errors='replace')
        if reject:assert 'assert' in text.lower(),(name,'not an assertion rejection')
        if proof:assert proof in text,(name,'missing proof')
        return text
    def flags(cc,include):
        result=[cc,'-std=c11','-Wall','-Werror','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(include)]
        if cc.stem.lower()!='tcc':result+=['-Wextra']
        if os.name!='nt':result+=['-D_GNU_SOURCE']
        return result
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm'];suffix='.exe' if os.name=='nt' else ''
    save()
    try:
        proof='Execution resources: 32 worker last-owner tails (16 failed), 16 held entries, 16 owned close/waits, 16 real creation-close races, 8 explicit retries, 80 exact graphs (32 empty) with restore/arbitrary finish order, 120 allocation prefixes, identity refusal and balanced lifetimes'
        for opt in ('O0','O2'):
            binary=out/('plan.'+opt+suffix);extra=['-fno-jump-tables'] if os.name=='nt' else []
            run('plan.'+opt+'.build',flags(gcc,out)+['-'+opt,'-Wno-missing-field-initializers','-ffunction-sections','-fdata-sections',
                '-fno-asynchronous-unwind-tables','-fno-unwind-tables',*extra,root/'test/support/execution_resource_plan.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',binary]);product(binary)
            run('plan.'+opt+'.run',[binary],'Execution resources plan: source/Executor/TaskPool independent; exact owned family domains and symbol providers')
            if opt=='O0':
                for choice in ('source','executor','pool','both'):
                    sources=run(choice+'.sources',[binary,'--sources',choice]).splitlines();assert len(sources)==len(set(sources))
                    modules=run(choice+'.modules',[binary,'--modules',choice]);assert '_MODULE_ALL' not in modules
                    folder=out/choice;folder.mkdir();header=folder/'execution-resource-modules.h';header.write_text(modules,encoding='utf8');product(header)
                    report['Plans'][choice]=dict(Sources=sources,Modules=modules);save()
        for index,cc in enumerate(compilers):
            for opt in ('O0','O2'):
                for choice in ('source','executor','pool'):
                    name=str(index)+'.'+opt+'.'+choice;binary=out/(name+suffix)
                    define=[] if choice=='source' else ['-DXL6_TEST_'+choice.upper()]
                    sources=[root/s for s in report['Plans'][choice]['Sources']]
                    run(name+'.build',flags(cc,out/choice)+['-'+opt,*define,root/'test/support/execution_resource_minimal.c',*sources,*libs,'-o',binary]);product(binary)
                    run(name+'.run',[binary],'Execution resources minimal: selected actual source/native closure, creator consumed after physical retirement')
                    symbols=run(name+'.symbols',[nm,'--defined-only',binary])
                    absent=['xl6ContinuationExecutorCreate','xl6TaskRuntimeV1Get']
                    if choice in ('source','pool'):absent+=['xrtExecutorCreate','xl6ClosureRuntimeV1Get']
                    if choice in ('source','executor'):absent+=['xrtTaskPoolCreate','xl6TaskPoolControlV1Get']
                    for symbol in absent:assert not any(line.split() and line.split()[-1]==symbol for line in symbols.splitlines()),(choice,symbol)
                name=str(index)+'.'+opt+'.native';binary=out/(name+suffix);sources=[root/s for s in report['Plans']['both']['Sources']]
                run(name+'.build',flags(cc,out/'both')+['-'+opt,root/'test/support/execution_resource_native.c',*sources,*libs,'-o',binary]);product(binary)
                output=run(name+'.run',[binary],proof)
                assert 'Execution resource error retirement: 64 observed/unobserved' in output
                assert 'TaskPool worker exit: 48 real-thread normal/NULL/allocated failures, 144 persistent jobs, 16 lazy owned graphs, 32 lazy start prefixes' in output
                assert 'TaskPool resource-before-exit: 16 actual asynchronous files retained across join' in output
        run('api.syntax',flags(gcc,out)+['-DXL_BUILD_DLL','-Wno-missing-field-initializers','-fsyntax-only',
            root/'src/api/xl_api_symbols.c',root/'src/compile/xl6_compile_unit.c'])
        providers=[root/s for s in report['Plans']['both']['Sources']]
        controls=[('null-failure-lost','xl6_execution_resource.c','if(!outcome.ok)sourceFailureLocked(source,&error);',
            'if(error)sourceFailureLocked(source,&error);'),
            ('copied-family-admitted','xl6_execution_resource.c','families[i]==r->family','families[i]->kind==r->family->kind'),
            ('restore-stranded','xl6_execution_resource.c','s->claim=NULL;s->graphStop=false;','s->claim=NULL;s->graphStop=true;'),
            # Destroy now deliberately supports cleared graph owners as well;
            # replacing ReleaseClearedOwner with it is no longer a mutation.
            # Check the actual invariant: Finish must consume the real owner.
            ('pool-owner-not-consumed','xl6_execution_pool.c','xl6TaskPoolReleaseClearedOwnerV1(r->pool)','true'),
            ('exited-source-never-ready','xl6_execution_resource.c','s->joined||s->exited||(s->graphStop&&s->parked)',
             's->joined||(s->graphStop&&s->parked)'),
            ('graph-notification-lost','xl6_execution_failures.c','bool pending=s->unreported;','bool pending=false;'),
            ('observed-notification-repeated','xl6_execution_failures.c','if(pending&&!retirement)state->unreported=false;','(void)retirement;'),
            ('worker-exit-failure-lost','xl6_execution_worker_exit.c','xl6ExecutionFailuresRecordV1(raw,outcome.ok,outcome.error);xrtClearError();xrtSetErrorTake(previous);',
             '(void)raw;xrtErrorFree(outcome.error);xrtClearError();xrtSetErrorTake(previous);'),
            ('owned-wait-drains-only','xl6_task_pool.c','if(result==XWAIT_OK&&ownedExit)result=control_join(pool,native,deadline,cancel);',
             '(void)control_join;(void)ownedExit;'),
            ('pending-exit-ref-lost','xl6_task_pool.c','if(native)pool->exitData=NULL; /* successful native ownership transfer */',
             'pool->exitData=NULL; /* incorrect consumption on failed creation */'),
            ('unregistered-lazy-exit','xl6_task_pool.c','if(!known)return NULL;','(void)known;'),
            ('unobserved-pool-exit-lost','xl6_execution_pool.c','if(!xl6ExecutionFailuresUnreportedV1(r->failures))','if(false)'),
            ('resource-credit-stranded','xl6_task_pool.c',
             '    pool_close_resources(pool);\n    for(;;){\n        xwaitresult state=xrtTaskPoolTryJoinV1(native);',
             '    /* incorrect: leave accepted native resource credits alive */\n    for(;;){\n        xwaitresult state=xrtTaskPoolTryJoinV1(native);')]
        report['NegativeControls']=[]
        for name,filename,old,new in controls:
            original=root/'src/runtime'/filename;body=original.read_text(encoding='utf-8-sig');assert body.count(old)==1,(name,'inexact mutation')
            mutant=out/(name+'.c');mutant.write_text(body.replace(old,new),encoding='utf8');product(mutant)
            selected=[mutant if source==original else source for source in providers];binary=out/(name+suffix)
            run(name+'.build',flags(gcc,out/'both')+['-O0','-I'+str(root/'src/runtime'),root/'test/support/execution_resource_native.c',*selected,*libs,'-o',binary]);product(binary)
            run(name+'.refused',[binary],reject=True);report['NegativeControls'].append(name);save()
        if a.sanitize:
            binary=out/'instrumented';sources=[root/s for s in report['Plans']['both']['Sources']]
            run('instrumented.build',flags(gcc,out/'both')+['-O1','-g','-fno-omit-frame-pointer','-fsanitize=address,undefined','-fno-pie','-no-pie',
                root/'test/support/execution_resource_native.c',*sources,*libs,'-o',binary]);product(binary)
            output=run('instrumented.run',[binary],proof,True);assert 'Execution resource error retirement: 64 observed/unobserved' in output
            report['InstrumentedSelectedClosure']=True
        report['ShippingSources']=json.loads(run('shipping-sources',['python' if os.name=='nt' else 'python3',root/'tools/check_stdlib_shipping_sources.py','--root',root]));assert report['ShippingSources']['Complete']
        report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items());report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
        assert report['InputsUnchanged'] and report['LogsUnchanged'];report['Complete']=True;save()
    except BaseException as error:failures.append(repr(error));save();raise
if __name__=='__main__':main()
