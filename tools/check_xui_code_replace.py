"""Counted document replacement through two direct native declarations.

Full compiler, matching O2 hosted/static SDK, real JIT/dynamic/static runs,
C/C++ public headers, failure/pin/thread/expiry and native heap comparisons.
No window/network tests, final delivery, or CPU-equivalence claim.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in('root','compiler','compiler-report','native-root','engine','static-build','dynamic-packages','static-packages','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,compiler,build,native,engine,static_build,dynamic,static,out=[v.resolve()for v in
        (a.root,a.compiler,a.compiler_report,a.native_root,a.engine,a.static_build,a.dynamic_packages,a.static_packages,a.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt'
    tools={n:Path(shutil.which(n)).resolve()for n in('gcc','g++','objdump')}
    case=root/'test/cases/foreign_xui_code_replace.xl';example=root/'lib/bindings/xui/examples/code_replace.xl'
    fixture=case.with_suffix('.c')
    h=json.loads((engine/'build-report.json').read_text(encoding='utf-8'))
    s=json.loads((static_build/'build-report.json').read_text(encoding='utf-8'))
    assert h['linked']and h['xrt_mode']=='host'and s['passed']and s['sourceUnchanged']and s['xrt_mode']=='direct-external'
    image=engine/h['runtime_image'];library=engine/h['import_library']if win else image;archive=static_build/s['archive']
    runtime=compiler.parent/('xlc.dll'if win else'libxlc.so')
    files=[Path(__file__).resolve(),compiler,build,*extra,*tools.values(),case,fixture,case.with_suffix('.h'),example,
        Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py'),
        image,library,archive,engine/'build-report.json',static_build/'build-report.json',engine/'xge_build_config.h',
        static_build/'xge_build_config.h',static_build/'xge_static_xrt_contract.h',*sorted((engine/'host-xrt').glob('*'))]
    files.extend(native/v for v in('include/xui.h','src/xui/code/xui_code_document.c','src/xui/code/xui_code_document_internal.h','src/xui/code/xui_code_search.c','tests/xui/xui_code_search_outcome_test.c'))
    files.extend(compiler.parent/n for n in(('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    for directory in(sdk,dynamic,static):files.extend(v for v in directory.rglob('*')if v.is_file())
    sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest();inputs={str(v):sha(v)for v in files}
    out.mkdir(parents=True,exist_ok=False);snapshots=out/'input-snapshots';snapshots.mkdir()
    for v in(Path(__file__).resolve(),case,fixture,case.with_suffix('.h'),example):shutil.copy2(v,snapshots/v.name)
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,
        nativeRebuilt=True,networkTests=False,cpuEquivalenceClaim=False,inputs=inputs,rows=[],programs=[],heap=[],cold=[])
    def save():
        target=out/'results.json.tmp';target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');os.replace(target,out/'results.json')
    def run(name,command,packages):
        env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
        env['XLANG_PACKAGE_PATH']=str(packages);key='PATH'if win else'LD_LIBRARY_PATH'
        env[key]=os.pathsep.join((str(packages/'_xge_native/native'),str(engine),str(runtime.parent),env.get(key,'')))
        command=list(map(str,command));start=time.monotonic()
        def no_core():
            import resource
            resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300,preexec_fn=None if win else no_core)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert not z.returncode,log;return z.stdout+z.stderr
    includes=['--inc-path',root,'--inc-path',sdk,'--inc-path',native]
    c_includes=['-I'+str(v)for v in(root,sdk,native,native/'include',*sorted(dynamic.glob('*/include')))]
    try:
        for level in ('O0','O2'):
            run('fixture-syntax-'+level,[tools['gcc'],'-'+level,'-std=c11','-Wall','-Wextra','-Werror',*c_includes,'-fsyntax-only',fixture],dynamic)
        for source in(case,example):
            name=source.stem;lesson=source==example;c=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,'--emit',c,'--emit-header',header,*includes],dynamic)
            generated=c.read_text(encoding='utf-8-sig')
            symbols=['xuiCodeSearchReplaceAllPlain','xuiCodeSearchReplaceAllRegex']
            for symbol in symbols:assert re.search(r'\b'+symbol+r'\(',generated),symbol
            if not lesson:
                assert 'xl6BufferSpanBeginV1' in generated and 'xlir_cstr_arg_' in generated
                report['generatedC']=dict(path=str(c),sha256=sha(c),directCalls={symbol:len(re.findall(r'(?<![\w&])'+symbol+r'\(',generated))for symbol in symbols},
                    checkedCStringInput=True,writableBufferPin=True,zeroBridgeOverheadClaim=False)
            for standard,tool,language in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run(name+'-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*c_includes,'-x',language,'-fsyntax-only',header],dynamic)
            for kind,packages in(('jit',dynamic),('dynamic',dynamic),('static',static)):
                if kind=='jit':output=run(name+'-'+kind,[compiler,'run',source,*includes],packages)
                else:
                    binary=out/(name+'-'+kind+('.exe'if win else''))
                    run(name+'-'+kind+'-build',[compiler,'build',source,'-o',binary,*includes],packages)
                    output=run(name+'-'+kind+'-execute',[binary],packages)
                    table=run(name+'-'+kind+'-imports',[tools['objdump'],'-p',binary],packages)
                    imports=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table);lower={v.lower()for v in imports}
                    assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                    assert any(b'xge3_host'in v for v in lower)==(kind=='dynamic')
                    report['programs'].append(dict(path=str(binary),sha256=sha(binary),bytes=binary.stat().st_size,imports=[v.decode()for v in imports]))
                if not lesson:
                    assert b'CodeReplace: NUL, CRLF, undo, captures, callbacks, pinned diagnostics, OOM, thread and expiry passed'in output
                    counts=re.findall(rb'CodeReplace heap kind=(\d+) alloc=(\d+) free=(\d+) realloc=(\d+)',output)
                    assert len(counts)==6 and {int(row[0])for row in counts}==set(range(6))
                    values={int(row[0]):tuple(map(int,row[1:]))for row in counts}
                    assert values[0]==(2000,2000,0) and values[1]==values[4]==(0,0,0)
                    assert values[3]==values[5] and values[2]==tuple(a+b for a,b in zip(values[3],(200,200,0)))
                    assert values[3][0]>0 and values[3][2]>0
                    report['heap'].append(dict(mode=kind,plainCalls=1000,regexCalls=100,allocationPrefixes=96,
                        literalExtraAllocationsPerCall=2,preparedLanguageEqualsNativeHeap=True,
                        counts={str(k):dict(zip(('alloc','free','realloc'),v))for k,v in values.items()}))
                else:
                    assert b'Literal replacements: 2'in output and b'#12, #34'in output
                save()
        report.update(complete=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()));report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
