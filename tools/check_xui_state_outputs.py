"""Qualify Document text record snapshots and Dock owned in-memory state.

Full O2 source compiler and matched XGE SDK. Native C O0/O2 host/static
oracles, CLI JIT/dynamic/static, C/C++ headers, OOM and owned-state lifetime.
No network, security tests, product adapters or shipping installation.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in('root','compiler','compiler-report','native-root','engine','static-build','dynamic-packages','static-packages','out'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();root,compiler,build,native,engine,static_build,dynamic,static,out=[v.resolve()for v in(a.root,a.compiler,a.compiler_report,a.native_root,a.engine,a.static_build,a.dynamic_packages,a.static_packages,a.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt';sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    tools={n:Path(shutil.which(n)).resolve()for n in('gcc','g++','objdump')}
    case=root/'test/cases/foreign_xui_state_outputs.xl';fixture=case.with_suffix('.c');driver=case.with_name(case.stem+'_native.c')
    example=root/'lib/bindings/xui/examples/document_metadata.xl'
    h=json.loads((engine/'build-report.json').read_text());s=json.loads((static_build/'build-report.json').read_text())
    assert h['linked']and h['xrt_mode']=='host'and s['passed']and s['sourceUnchanged']and s['xrt_mode']=='direct-external'
    image=engine/h['runtime_image'];library=engine/h['import_library']if win else image;archive=static_build/s['archive']
    runtime=compiler.parent/('xlc.dll'if win else'libxlc.so')
    files=[Path(__file__).resolve(),compiler,build,runtime,*extra,*tools.values(),case,fixture,driver,case.with_suffix('.h'),example,
        root/'test/cases/foreign_xui_render_backend.c',root/'test/cases/foreign_xui_render_backend.h',root/'test/cases/foreign_xui_render_adapters.inc',native/'tests/xui/xui_test_proxy.c',native/'tests/xui/xui_test_proxy.h',
        Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py'),
        image,library,archive,engine/'build-report.json',static_build/'build-report.json',engine/'xge_build_config.h',static_build/'xge_build_config.h']
    for d in(sdk,dynamic,static):files.extend(f for f in d.rglob('*')if f.is_file())
    inputs={str(f):sha(f)for f in files};out.mkdir(parents=True,exist_ok=False);scratch=out/'tmp';scratch.mkdir();snapshots=out/'input-snapshots';snapshots.mkdir()
    for f in(case,fixture,driver,case.with_suffix('.h'),example,Path(__file__).resolve()):shutil.copy2(f,snapshots/f.name)
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,nativeRebuilt=False,networkTests=False,inputs=inputs,rows=[],programs=[],heap=[],budgets=[],nativeBudgets=[])
    def save():
        tmp=out/'results.json.tmp';tmp.write_text(json.dumps(report,indent=2)+'\n');os.replace(tmp,out/'results.json')
    def run(name,command,packages=dynamic):
        env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
        env.update(XLANG_PACKAGE_PATH=str(packages),TEMP=str(scratch),TMP=str(scratch),TMPDIR=str(scratch))
        key='PATH'if win else'LD_LIBRARY_PATH';env[key]=os.pathsep.join((str(packages/'_xge_native/native'),str(engine),str(compiler.parent),env.get(key,'')))
        startup=None
        if win:startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        def no_core():
            import resource
            resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        command=list(map(str,command));start=time.monotonic();z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300,startupinfo=startup,preexec_fn=None if win else no_core)
        data=z.stdout+z.stderr;log=out/(name+'.log');log.write_bytes(data)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,passed=not z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert not z.returncode,log;return data
    includes=['--inc-path',root,'--inc-path',sdk,'--inc-path',native]
    ci=['-I'+str(v)for v in(root,sdk,native,native/'include',*sorted(dynamic.glob('*/include')))]
    try:
        for kind,folder,lib in(('host',engine,library),('static',static_build,archive)):
            flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-cast-function-type','-DXGE_XRT_HOST=1','-include',folder/'xge_build_config.h',*ci,'-I'+str(folder),'-I'+str(engine/'host-xrt')]
            flags+=['-DXGE_TEST_STATIC=1','-DXL6_XGE_STATIC=1']if kind=='static'else['-DXGE_DLL=1','-DXUI_DLL=1']if win else[]
            systems=['-l'+n for n in s['system_libraries']]if kind=='static'else[]
            if not win:systems+=['-Wl,-rpath,'+str(engine),'-Wl,-rpath,'+str(runtime.parent),'-ldl','-lm','-pthread']
            for opt in('O0','O2'):
                name='native-'+kind+'-'+opt;binary=out/(name+('.exe'if win else''))
                run(name+'-build',[tools['gcc'],*flags,'-'+opt,driver,fixture,lib,runtime,*systems,'-o',binary]);data=run(name,[binary])
                assert b'State outputs native:'in data
                counts=re.findall(rb'Native Dock state OOM: prefixes=(\d+) failures=(\d+)',data);assert len(counts)==1 and 0<int(counts[0][1])<int(counts[0][0])
                report['nativeBudgets'].append(dict(kind=kind,optimization=opt,prefixes=int(counts[0][0]),failures=int(counts[0][1])));save()
        for source in(case,example):
            name=source.stem;c=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,'--emit',c,'--emit-header',header,*includes]);code=c.read_text(encoding='utf-8-sig')
            for symbol in('xuiDocumentSnapshotGetNode','xuiDocumentSnapshotQueryLink'):assert re.search(r'\b'+symbol+r'\(',code)
            if source==case:
                assert re.search(r'\bxuiDockPanelSaveState\(',code)and re.search(r'\bxuiDockPanelLoadState\(',code)
            for standard,tool,language in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run(name+'-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*ci,'-x',language,'-fsyntax-only',header])
            for kind,packages in(('jit',dynamic),('dynamic',dynamic),('static',static)):
                if kind=='jit':data=run(name+'-'+kind,[compiler,'run',source,*includes],packages)
                else:
                    binary=out/(name+'-'+kind+('.exe'if win else''));run(name+'-'+kind+'-build',[compiler,'build',source,'-o',binary,*includes],packages);data=run(name+'-'+kind+'-execute',[binary],packages)
                    table=run(name+'-'+kind+'-imports',[tools['objdump'],'-p',binary],packages);imports=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table);lower={v.lower()for v in imports}
                    assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                    assert any(b'xge3_host'in v for v in lower)==(kind=='dynamic')
                    report['programs'].append(dict(path=str(binary),sha256=sha(binary),bytes=binary.stat().st_size,imports=[v.decode()for v in imports]))
                if source==case:
                    assert b'State outputs: Document snapshots'in data
                    heap=re.findall(rb'Dock state heap (language|native): alloc=(\d+) free=(\d+) realloc=(\d+)',data)
                    assert len(heap)==2 and [v[0]for v in heap]==[b'language',b'native']
                    values=[list(map(int,v[1:]))for v in heap];assert values[0]==values[1]and values[0][0]==values[0][1]>0 and values[0][2]==0
                    doc=re.findall(rb'Document metadata OOM: prefixes=64 failures=\s*(\d+)',data);dock=re.findall(rb'Dock state OOM: prefixes=\s*(\d+)\s+failures=\s*(\d+)',data)
                    assert len(doc)==len(dock)==1 and 0<int(doc[0])<64 and 0<int(dock[0][1])<int(dock[0][0])
                    report['heap'].append(dict(mode=kind,rounds=100,language=values[0],native=values[1],scope='Same SDK and serialized layout; allocation/free/reallocation only; no CPU equivalence claim'))
                    report['budgets'].append(dict(mode=kind,documentPrefixes=64,documentFailures=int(doc[0]),dockPrefixes=int(dock[0][0]),dockFailures=int(dock[0][1])));save()
                else:assert b'Document metadata: independent text snapshot passed'in data
        report.update(complete=True,inputsUnchanged=all(sha(f)==v for f,v in inputs.items()));report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
