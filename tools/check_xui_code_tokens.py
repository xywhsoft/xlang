"""Independent CodeTokenBuffer: typed input and checked version/range metadata.

Full compiler, exact existing O2 SDK, native O0/O2 hosted/static clients and
JIT/dynamic/static language/lesson programs. Includes failure atomicity, OOM,
active close, thread/expiry, and prepared C/language heap comparisons.
Bulk token output and CodeEdit internal aliases are not certified. No new
compiler or native engine implementation; no graphics/network/final delivery.
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
    case=root/'test/cases/foreign_xui_code_tokens.xl';example=root/'lib/bindings/xui/examples/code_tokens.xl'
    fixture=case.with_suffix('.c');driver=case.with_name(case.stem+'_native.c')
    h=json.loads((engine/'build-report.json').read_text(encoding='utf-8'))
    s=json.loads((static_build/'build-report.json').read_text(encoding='utf-8'))
    assert h['linked']and h['xrt_mode']=='host'and s['passed']and s['sourceUnchanged']and s['xrt_mode']=='direct-external'
    image=engine/h['runtime_image'];library=engine/h['import_library']if win else image;archive=static_build/s['archive']
    runtime=compiler.parent/('xlc.dll'if win else'libxlc.so')
    files=[Path(__file__).resolve(),compiler,build,*extra,*tools.values(),case,fixture,case.with_suffix('.h'),driver,example,
        Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py'),
        image,library,archive,engine/'build-report.json',static_build/'build-report.json',engine/'xge_build_config.h',
        static_build/'xge_build_config.h',static_build/'xge_static_xrt_contract.h',*sorted((engine/'host-xrt').glob('*'))]
    files.extend(native/v for v in('include/xui.h','src/xui/code/xui_code_token_buffer.c'))
    files.extend(compiler.parent/n for n in(('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    for directory in(sdk,dynamic,static):files.extend(v for v in directory.rglob('*')if v.is_file())
    sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest();inputs={str(v):sha(v)for v in files}
    out.mkdir(parents=True,exist_ok=False);snapshots=out/'input-snapshots';snapshots.mkdir()
    for v in(Path(__file__).resolve(),case,fixture,case.with_suffix('.h'),driver,example):shutil.copy2(v,snapshots/v.name)
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,
        nativeRebuilt=False,networkTests=False,cpuEquivalenceClaim=False,inputs=inputs,rows=[],programs=[],heap=[],cold=[])
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
        # Independently compiled C does not use a generated-language entry point.
        for kind,folder,lib in(('host',engine,library),('static',static_build,archive)):
            flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-cast-function-type','-DXGE_XRT_HOST=1',
                '-include',folder/'xge_build_config.h',*c_includes,'-I'+str(folder),'-I'+str(engine/'host-xrt')]
            flags+=['-DXGE_TEST_STATIC=1','-DXL6_XGE_STATIC=1']if kind=='static'else['-DXGE_DLL=1','-DXUI_DLL=1']if win else[]
            systems=['-l'+n for n in s['system_libraries']]if kind=='static'else[]
            if not win:systems+=['-Wl,-rpath,'+str(engine),'-Wl,-rpath,'+str(runtime.parent),'-ldl','-lm','-pthread']
            for level in('O0','O2'):
                name='native-'+kind+'-'+level;binary=out/(name+('.exe'if win else''))
                run(name+'-build',[tools['gcc'],*flags,'-'+level,driver,fixture,lib,runtime,*systems,'-o',binary],dynamic)
                output=run(name,[binary],dynamic)
                assert b'CodeTokens native: actual SDK ranges, payload, failure atomicity, allocation prefixes and warmed queries passed'in output
        for source in(case,example):
            name=source.stem;lesson=source==example;c=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,'--emit',c,'--emit-header',header,*includes],dynamic)
            generated=c.read_text(encoding='utf-8-sig')
            symbols=['xuiCodeTokenBufferCreate','xuiCodeTokenBufferSetRange','xuiCodeTokenBufferGetCount','xuiCodeTokenBufferGetRange']
            if not lesson:symbols+=['xuiCodeTokenBufferSet','xuiCodeTokenBufferClear','xuiCodeTokenBufferGetVersion']
            for symbol in symbols:assert re.search(r'\b'+symbol+r'\(',generated),symbol
            if not lesson:
                assert 'const xui_code_token_t*' in generated
                report['generatedC']=dict(path=str(c),sha256=sha(c),directCalls={symbol:len(re.findall(r'(?<![\w&])'+symbol+r'\(',generated))for symbol in symbols},
                    typedInputProjection=True,zeroBridgeOverheadClaim=False)
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
                    assert b'CodeTokens: typed input, version, ranges, failure atomicity, OOM, active close, thread and expiry passed'in output
                    counts=re.findall(rb'CodeToken heap kind=(\d+) alloc=(\d+) free=(\d+) realloc=(\d+)',output)
                    assert len(counts)==6 and {int(row[0])for row in counts}==set(range(6))
                    values={int(row[0]):tuple(map(int,row[1:]))for row in counts}
                    assert values[0]==values[1]==(0,0,0)
                    assert values[2]==(3,3,0)and values[3]==(1,1,0)
                    assert values[4]==(1,1,0)and values[5]==(0,0,0)
                    report['heap'].append(dict(mode=kind,rounds=1000,callsPerRound=3,allocationPrefixes=96,
                        counts={str(k):dict(zip(('alloc','free','realloc'),v))for k,v in values.items()}))
                else:
                    assert b'Cached tokens: 1'in output and b'Version and byte range: 1 0 4'in output
                save()
        report.update(complete=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()));report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
