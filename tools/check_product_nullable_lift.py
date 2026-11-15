"""Qualify protected product-field nullable lifts and owned-output happy calls.

Uses complete old/new compiler cohorts and one matched new resident runtime.
Tests preserve existing left-to-right destination commits. Instrumentation is
test-only: it counts explicit calls in emitted C, excluding included runtime
header internals and independently compiled native producer code. No network.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort
from stage_runtime_headers import stage as stage_runtime_headers


def instrument(code,header):
    names={'xrtGetError':'admissionCountGet','xrtTakeError':'admissionCountTake',
           'xrtValueRelease':'admissionCountRelease','xrtSetErrorTake':'admissionCountSet'}
    lines=[]
    for line in code.splitlines(keepends=True):
        if not re.match(r'\s*(extern\b|#)',line):
            for native,measured in names.items():line=re.sub(r'\b'+native+r'(?=\s*\()',measured,line)
        lines.append(line)
    result=''.join(lines);needle='#include <__xl_runtime/xrt_decl.h>'
    assert result.count(needle)==1
    prototypes='\n'.join(line for line in header.splitlines() if line.endswith(';'))
    assert len(prototypes.splitlines())==6
    return result.replace(needle,needle+'\n'+prototypes,1)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','compiler','compiler-report','baseline-compiler','baseline-report','out'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();root,compiler,build,baseline,old_build,out=[v.resolve()for v in(a.root,a.compiler,a.compiler_report,a.baseline_compiler,a.baseline_report,a.out)]
    sdk,_=load_compiler_cohort(root,compiler,build);old_sdk,_=load_compiler_cohort(root,baseline,old_build)
    assert not out.is_relative_to(root);win=os.name=='nt';sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    tools={n:Path(shutil.which(n)).resolve()for n in('gcc','g++','objdump')}
    case=root/'test/cases/foreign_product_nullable_lift.xl';benchmark=root/'test/cases/foreign_owned_admission_calls.xl'
    priority=root/'test/cases/foreign_owned_error_priority.xl'
    runtime=compiler.parent/('xl.dll'if win else'libxl.so');host=root/'test/cases/foreign_decl_module_host.c'
    files=[Path(__file__).resolve(),compiler,build,baseline,old_build,runtime,host,*tools.values()]
    # Linux launchers resolve libxlc through the loader. Each frontend probe
    # must execute its own complete cohort, even though emitted benchmark C
    # deliberately links against the same new resident runtime below.
    for selected in(compiler,baseline):
        files.extend(selected.parent/n for n in(('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    for f in(case,benchmark,priority):files.extend((f,f.with_suffix('.h'),f.with_suffix('.c')))
    files.extend(root/'test/cases'/('foreign_output_snapshots'+suffix)for suffix in('.c','.h'))
    files.extend(Path(__file__).with_name(n+'.py')for n in('foreign_compiler_cohort','full_compiler_qualification','stage_runtime_headers','gen_tcc_vfs_resources'))
    for folder in(sdk,old_sdk):files.extend(f for f in folder.rglob('*')if f.is_file())
    inputs={str(f):sha(f)for f in files};out.mkdir(parents=True,exist_ok=False);scratch=out/'tmp';scratch.mkdir();snap=out/'input-snapshots';snap.mkdir()
    metadata=json.loads(build.read_text());frozen=Path(metadata['source']);fresh=out/'runtime-sdk'
    headers=stage_runtime_headers(frozen,fresh,frozen/'tools'/('tcc_vfs_manifest_win64_xlc.txt'if win else'tcc_vfs_manifest_linux_xlc.txt'))
    for f in fresh.rglob('*'):
        if f.is_file():inputs[str(f)]=sha(f)
    for f in files:
        if f.parent==case.parent or f==Path(__file__).resolve():shutil.copy2(f,snap/f.name)
    env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
    env.update(TEMP=str(scratch),TMP=str(scratch),TMPDIR=str(scratch));key='PATH'if win else'LD_LIBRARY_PATH';env[key]=str(compiler.parent)+os.pathsep+env.get(key,'')
    report=dict(complete=False,passed=False,shippingDelivery=False,fullCompilerBuild=True,networkTests=False,scope=__doc__,inputs=inputs,rows=[],budgets=[],calls=[],programs=[],stagedPrivateHeaders=len(headers))
    def save():
        tmp=out/'results.json.tmp';tmp.write_text(json.dumps(report,indent=2)+'\n');os.replace(tmp,out/'results.json')
    def run(name,command,success=True):
        startup=None
        if win:startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        def no_core():
            import resource
            resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        command=list(map(str,command));call_env=env.copy()
        if not win and Path(command[0])in(compiler,baseline):
            call_env[key]=str(Path(command[0]).parent)+os.pathsep+os.environ.get(key,'')
        start=time.monotonic();z=subprocess.run(command,cwd=root,env=call_env,capture_output=True,timeout=300,startupinfo=startup,preexec_fn=None if win else no_core)
        data=z.stdout+z.stderr;log=out/(name+'.log');log.write_bytes(data);ok=(not z.returncode)==success
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,expectedSuccess=success,passed=ok,seconds=time.monotonic()-start,logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert ok,log;return data
    includes=['--inc-path',root,'--inc-path',fresh/'include','--inc-path',sdk]
    ci=['-I'+str(v)for v in(fresh/'include',root,sdk,compiler.parent)]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']if win else['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    def check(mode,data):
        assert b'Product nullable: scalar, text, bytes, array and retained-root lifts' in data
        counts=re.findall(rb'Product nullable OOM: prefixes=64 failures=\s*(\d+)',data);assert len(counts)==1 and 0<int(counts[0])<64
        report['budgets'].append(dict(mode=mode,prefixes=64,failures=int(counts[0])));save()
    try:
        target=out/'old-rejected.c'
        data=run('baseline-reject',[baseline,'cgen',case,'--emit',target,'--inc-path',root,'--inc-path',old_sdk],False)
        assert b'no exact protected field transaction'in data and not target.exists()
        generated=out/'generated.c';public=out/'public.h'
        run('emit',[compiler,'cgen',case,'--emit',generated,'--emit-header',public,*includes])
        run('ir',[compiler,'ir',case,'--lowered','--format','json','--emit',out/'lowered.json'])
        for standard,tool,language in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
            run('header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*ci,'-x',language,'-fsyntax-only',public])
        check('jit',run('jit',[compiler,'run',case,*includes]))
        exe=out/('aot.exe'if win else'aot');run('aot-build',[compiler,'build',case,'-o',exe,*includes]);check('aot',run('aot-execute',[exe]))
        data=run('aot-imports',[tools['objdump'],'-p',exe]);imports=re.findall(rb'DLL Name:\s*(\S+)',data)if win else re.findall(rb'\bNEEDED\s+(\S+)',data)
        assert not{v.lower()for v in imports}.intersection({b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'})
        report['programs'].append(dict(path=str(exe),sha256=sha(exe),bytes=exe.stat().st_size,imports=[v.decode()for v in imports]))
        for opt in('O0','O2'):
            exe=out/('gcc-'+opt+('.exe'if win else''));run(opt+'-build',[tools['gcc'],'-std=c11','-'+opt,*ci,generated,case.with_suffix('.c'),host,runtime,*systems,'-o',exe]);check(opt,run(opt+'-execute',[exe]))
        # Success status does not permit a result finalizer to replace an
        # already reported producer error (code 73) with cleanup code 91.
        data=run('baseline-priority-reject',[baseline,'run',priority,*includes],False)
        assert b'AssertionError' in data
        pc=out/'priority.c';ph=out/'priority.h'
        run('priority-emit',[compiler,'cgen',priority,'--emit',pc,'--emit-header',ph,*includes])
        for standard,tool,language in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
            run('priority-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*ci,'-x',language,'-fsyntax-only',ph])
        assert b'Owned priority: producer error survives' in run('priority-jit',[compiler,'run',priority,*includes])
        exe=out/('priority-aot.exe'if win else'priority-aot')
        run('priority-aot-build',[compiler,'build',priority,'-o',exe,*includes])
        assert b'Owned priority: producer error survives' in run('priority-aot-execute',[exe])
        data=run('priority-aot-imports',[tools['objdump'],'-p',exe])
        imports=re.findall(rb'DLL Name:\s*(\S+)',data)if win else re.findall(rb'\bNEEDED\s+(\S+)',data)
        assert not{v.lower()for v in imports}.intersection({b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'})
        for opt in('O0','O2'):
            exe=out/('priority-'+opt+('.exe'if win else''))
            run('priority-'+opt+'-build',[tools['gcc'],'-std=c11','-'+opt,*ci,pc,priority.with_suffix('.c'),host,runtime,*systems,'-o',exe])
            assert b'Owned priority: producer error survives' in run('priority-'+opt,[exe])
        measurements={}
        for tag,selected in(('before',baseline),('after',compiler)):
            c=out/(tag+'.c');run(tag+'-emit',[selected,'cgen',benchmark,'--emit',c,*includes])
            code=c.read_text(encoding='utf-8-sig');instrumented=out/(tag+'-instrumented.c');instrumented.write_text(instrument(code,benchmark.with_suffix('.h').read_text()))
            if tag=='after':assert re.search(r'else \{ if\(!\w+\.failed\)xl6ErrorScopeCaptureFailure\(&\w+\);\s+if\(native_raw\) \{ xerror\* output_value_error=',code)
            for opt in('O0','O2'):
                exe=out/(tag+'-calls-'+opt+('.exe'if win else''));run(tag+'-'+opt+'-build',[tools['gcc'],'-std=c11','-'+opt,*ci,instrumented,benchmark.with_suffix('.c'),host,runtime,*systems,'-o',exe]);data=run(tag+'-'+opt,[exe])
                count=re.findall(rb'Admission generated calls: get=(\d+) take=(\d+) release=(\d+) set=(\d+) nullRelease=(\d+)',data);assert len(count)==1
                measurements[tag,opt]=list(map(int,count[0]));report['calls'].append(dict(version=tag,optimization=opt,rounds=1000,counts=measurements[tag,opt]));save()
        for opt in('O0','O2'):
            difference=[x-y for x,y in zip(measurements['before',opt],measurements['after',opt])]
            assert difference==[1000,1000,1000,1000,1000],difference
        report.update(complete=True,inputsUnchanged=all(sha(f)==v for f,v in inputs.items()),generatedHappyCallsRemovedPerSuccess=dict(getError=1,takeError=1,releaseNull=1,setErrorTake=1),cpuEquivalenceClaim=False)
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
