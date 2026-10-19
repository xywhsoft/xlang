"""Sealed offline pool control, exact source closure and legacy frame ownership.

Builds selected real providers and the real current native implementation; no
shipping DLL, source substitution or stub fallback. Not public language API,
whole-product JIT/AOT or all-five-group acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',action='append',required=True)
    p.add_argument('--sanitize',action='store_true')
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve()
    if out.exists():p.error('Preserve previous evidence; choose a fresh output directory')
    if a.sanitize and os.name=='nt':p.error('The instrumented lane requires Linux GCC')
    compilers=[Path(shutil.which(name) or name).resolve() for name in a.compiler]
    gcc=next((path for path in compilers if path.stem.lower()!='tcc'),None)
    if not gcc:p.error('A GCC lane is required for actual source-plan verification')
    def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
    paths=[path for folder in ('src','lib/xrt','lib/xruntime','release/include') for path in (root/folder).rglob('*')
        if path.is_file() and path.suffix in ('.c','.h','.inc')]
    paths += list((root/'tools').glob('*.ps1'))
    paths += [root/name for name in ('test/support/task_pool_control_native.c','test/support/task_pool_control_plan.c',
        'test/support/task_pool_control_legacy_outcome.c','test/cases/xlir_task_outcome_whitebox.c','test/support/task_pool_control_minimal.c',
        'test/support/executor_catalog_plan.c','test/cases/xlir_task_pool_graph_runtime.c','tools/xrt_single_header_impl.c',
        'tools/check_stdlib_fs_posix.sh','tools/check_xlir_future_cancel_core.sh',
        'tools/check_stdlib_task_pool_control.py','tools/compiler_dll.rsp.in','tools/runtime_dll.rsp.in','build_linux.sh')]
    paths += compilers
    if os.name=='nt':paths.append(Path('C:/Windows/System32/ucrtbase.dll'))
    inputs={str(path):sha(path) for path in sorted(set(paths))};rows=[];logs=[];products=[];failures=[]
    out.mkdir(parents=True)
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,
        Failures=failures,TaskSources=[],PoolSources=[],NativeControlAccepted=False,PlanAccepted=False,MinimalDeploymentAccepted=False,
        LegacyGraphAccepted=False,LegacyOutcomeAccepted=False,
        InstrumentedSelectedNativeControl=False,PublicLanguageApiAccepted=False,DllJitAotAccepted=False,
        NetworkTests=False,NetworkSecurityTests=False,Scope=__doc__)
    env=dict(os.environ)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,args,proof=None,sanitized=False):
        log=out/(name+'.log');start=time.monotonic();selected_env=env
        if sanitized:selected_env=dict(env,ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
            UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with log.open('wb') as stream:
            result=subprocess.run(list(map(str,args)),cwd=root,stdout=stream,stderr=subprocess.STDOUT,timeout=300,env=selected_env)
        digest=sha(log);logs.append((log,digest));value=log.read_text(encoding='utf8',errors='replace')
        rows.append(dict(Name=name,Command=list(map(str,args)),Exit=result.returncode,Sha256=digest,Seconds=time.monotonic()-start));save()
        print(name+': '+str(result.returncode),flush=True)
        if result.returncode:raise RuntimeError('Failed '+name+'; see '+str(log))
        if proof and proof not in value:raise RuntimeError('Missing proof '+name)
        return value
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def common(compiler):
        flags=[str(compiler),'-std=c11','-Wall','-Werror','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
        if compiler.stem.lower()!='tcc':flags+=['-Wextra']
        if os.name!='nt':flags+=['-D_GNU_SOURCE']
        return flags
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    suffix='.exe' if os.name=='nt' else ''
    native_sources=[root/('src/'+name) for name in ('runtime/xl6_task_pool.c','runtime/xl6_ownership_domain_task_pool.c',
        'runtime/xl6_ownership_catalog.c','api/xl_foreign_error.c')]
    native_fixture=root/'test/support/task_pool_control_native.c'
    native_proof='Task pool control: 80 balanced lifetimes, 32 run/drop barriers, 16 real creating-cancel races, 16 control-retirement handoffs, 32 allocation prefixes, no hidden pool on read'
    graph_proof='Task pool wrapper: 280 balanced lifetimes, 240 exact queued raw/language graphs, 520 real preparations, accepted execution, joined native owner and both Finish orders'
    save()
    try:
        source_names=None
        for opt in ('O0','O2'):
            name='plan.'+opt;binary=out/(name+suffix)
            # MinGW places optimized switch jump tables in the shared .rdata
            # section. Their back-references retain the unrelated IR planner
            # even with function/data GC. This standalone closure test needs
            # only table/closure functions, not their compiler/IR consumers.
            # Keep O2, but do not emit those shared jump-table roots on PE.
            plan_flags=['-fno-jump-tables'] if os.name=='nt' else []
            run(name+'.build',common(gcc)+['-'+opt,'-Wno-missing-field-initializers','-ffunction-sections','-fdata-sections',
                '-fno-asynchronous-unwind-tables','-fno-unwind-tables',*plan_flags,root/'test/support/task_pool_control_plan.c',
                root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',binary])
            run(name+'.run',[binary],'Task pool control plan: management excludes worker frames/captures/default async; legacy tasks include both exact domains')
            product(binary)
            if source_names is None:
                source_names=run('task-sources',[binary,'--task-sources']).splitlines()
                if len(source_names)!=len(set(source_names)) or 'src/runtime/xl6_task_pool.c' not in source_names:raise RuntimeError('Incomplete Task source closure')
                report['TaskSources']=source_names;save()
                pool_sources=run('pool-sources',[binary,'--pool-sources']).splitlines()
                modules=run('pool-modules',[binary,'--pool-modules'])
                if len(pool_sources)!=len(set(pool_sources)) or 'src/runtime/xl6_task.c' in pool_sources:raise RuntimeError('Wrong management closure')
                if '#define XRT_MODULE_ATOMIC\n' not in modules or '_MODULE_ALL' in modules:raise RuntimeError('Wrong management selectors')
                module_header=out/'pool-modules.h';module_header.write_text(modules,encoding='utf8');product(module_header)
                report['PoolSources']=pool_sources;save()
        report['PlanAccepted']=True
        legacy=[root/name for name in source_names if name!='src/api/xl_foreign_error.c']
        # The canonical native implementation already includes the SAME real
        # foreign-error provider. Do not compile a duplicate TLS/error boundary.
        for index,compiler in enumerate(compilers):
            tcc=compiler.stem.lower()=='tcc';flags=common(compiler)
            for header in ('xrt','xruntime'):
                for opt in ('O0','O2'):
                    name=str(index)+'.'+header+'.'+opt;binary=out/(name+suffix)
                    defines=['-DXL6_TASK_POOL_TEST_RUNTIME'] if header=='xruntime' else []
                    minimal=out/(name+'.minimal'+suffix)
                    run(name+'.minimal.build',flags+['-'+opt,'-I'+str(out),*defines,root/'test/support/task_pool_control_minimal.c',
                        *[root/path for path in pool_sources],*libs,'-o',minimal])
                    run(name+'.minimal.run',[minimal],'Task pool minimal deployment: exact planned sources/selectors, actual native implementation, no test feature additions');product(minimal)
                    run(name+'.build',flags+['-'+opt,*defines,native_fixture,*native_sources,*libs,'-o',binary])
                    run(name+'.run',[binary],native_proof);product(binary)
            for opt in ('O0','O2'):
                name=str(index)+'.legacy.'+opt;kernel=out/(name+'.native.o');binary=out/(name+suffix)
                strict=[] if tcc else ['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
                    '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
                    '-Wno-pointer-sign','-Wno-format-truncation']
                run(name+'.kernel',flags+['-'+opt,*strict,'-c',root/'tools/xrt_single_header_impl.c','-o',kernel]);product(kernel)
                crt=[Path('C:/Windows/System32/ucrtbase.dll')] if tcc and os.name=='nt' else []
                run(name+'.build',flags+['-'+opt,*strict,root/'test/cases/xlir_task_pool_graph_runtime.c',*legacy,kernel,*crt,*libs,'-o',binary])
                run(name+'.run',[binary],graph_proof);product(binary)
                outcome=out/(name+'.outcome'+suffix)
                # The old white-box provider includes task.c. Do not link the
                # normal task.c again; its actual split pool provider and all
                # other plan-selected dependencies are still required.
                outcome_sources=[path for path in legacy if path.name!='xl6_task.c']
                run(name+'.outcome.build',flags+['-'+opt,*strict,root/'test/support/task_pool_control_legacy_outcome.c',
                    root/'test/cases/xlir_task_outcome_whitebox.c',*outcome_sources,kernel,*crt,*libs,'-o',outcome])
                run(name+'.outcome.run',[outcome],'Legacy task outcome: 400 isolated ready/registration callbacks, exact memory balance');product(outcome)
        report['NativeControlAccepted']=True;report['MinimalDeploymentAccepted']=True
        report['LegacyGraphAccepted']=True;report['LegacyOutcomeAccepted']=True
        if a.sanitize:
            binary=out/'instrumented.control'
            run('instrumented.control.build',common(gcc)+['-O1','-g','-fno-omit-frame-pointer','-fsanitize=address,undefined',
                '-fno-pie','-no-pie',native_fixture,*native_sources,*libs,'-o',binary])
            run('instrumented.control.run',[binary],native_proof,True);product(binary)
            report['InstrumentedSelectedNativeControl']=True
        report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
        report['LogsUnchanged']=all(sha(path)==digest for path,digest in logs)
        if not report['InputsUnchanged'] or not report['LogsUnchanged']:raise RuntimeError('Source/log drift')
        report['Complete']=True;save()
    except BaseException as error:
        failures.append(repr(error));report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items());save();raise

if __name__=='__main__':main()
