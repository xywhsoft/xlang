"""Qualify direct XUI diagnostic, virtual-text, accessibility and style records.

Complete O2 compiler and matched native SDK; C O0/O2 host/static oracle,
CLI JIT/dynamic/static AOT, C/C++ headers, transactional OOM and lifetimes.
CPU-only; no shipping installation, graphics, network or security tests.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort
from stage_runtime_headers import stage as stage_runtime_headers
from xui_snapshot_contracts import RECORD_OUTPUTS,STYLE_TOKEN_SET,VIRTUAL_TEXT_SET,declaration


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','compiler','compiler-report','native-root','engine','static-build','dynamic-packages','static-packages','out'):
        p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args()
    root,compiler,build,native,engine,static_build,dynamic,static,out=[v.resolve() for v in (a.root,a.compiler,a.compiler_report,a.native_root,a.engine,a.static_build,a.dynamic_packages,a.static_packages,a.out)]
    assert not out.is_relative_to(root)
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt'
    sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    tools={n:Path(shutil.which(n)).resolve() for n in ('gcc','g++','objdump')}
    case=root/'test/cases/foreign_xui_record_queries.xl';fixture=case.with_suffix('.c');driver=case.with_name(case.stem+'_native.c')
    example=root/'lib/bindings/xui/examples/code_annotations.xl'
    h=json.loads((engine/'build-report.json').read_text());s=json.loads((static_build/'build-report.json').read_text())
    assert h['linked'] and h['xrt_mode']=='host' and s['passed'] and s['sourceUnchanged'] and s['xrt_mode']=='direct-external'
    image=engine/h['runtime_image'];library=engine/h['import_library'] if win else image;archive=static_build/s['archive']
    runtime=compiler.parent/('xlc.dll' if win else 'libxlc.so')
    files=[Path(__file__).resolve(),compiler,build,runtime,*extra,*tools.values(),case,fixture,driver,case.with_suffix('.h'),example,
           root/'test/cases/foreign_xui_render_backend.c',root/'test/cases/foreign_xui_render_backend.h',root/'test/cases/foreign_xui_render_adapters.inc',
           native/'tests/xui/xui_test_proxy.c',native/'tests/xui/xui_test_proxy.h',
           *[Path(__file__).with_name(n+'.py') for n in ('foreign_compiler_cohort','full_compiler_qualification','stage_runtime_headers','gen_tcc_vfs_resources','xui_snapshot_contracts')],
           image,library,archive,engine/'build-report.json',static_build/'build-report.json',engine/'xge_build_config.h',static_build/'xge_build_config.h']
    for d in (sdk,dynamic,static):files.extend(f for f in d.rglob('*') if f.is_file())
    inputs={str(f):sha(f) for f in files}
    out.mkdir(parents=True,exist_ok=False);scratch=out/'tmp';scratch.mkdir();snapshots=out/'input-snapshots';snapshots.mkdir()
    metadata=json.loads(build.read_text());frozen=Path(metadata['source']);fresh=out/'runtime-sdk'
    staged=stage_runtime_headers(frozen,fresh,frozen/'tools'/('tcc_vfs_manifest_win64_xlc.txt' if win else 'tcc_vfs_manifest_linux_xlc.txt'))
    for f in fresh.rglob('*'):
        if f.is_file():inputs[str(f)]=sha(f)
    for f in (case,fixture,driver,case.with_suffix('.h'),example,Path(__file__).resolve()):shutil.copy2(f,snapshots/f.name)
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,nativeRebuilt=False,networkTests=False,inputs=inputs,stagedPrivateHeaders=len(staged),rows=[],programs=[],heap=[],budgets=[])
    def save():
        tmp=out/'results.json.tmp';tmp.write_text(json.dumps(report,indent=2)+'\n');os.replace(tmp,out/'results.json')
    def run(name,command,packages=dynamic):
        env={k:v for k,v in os.environ.items() if not k.startswith('XL6_') and k!='XLANG_PACKAGE_PATH'}
        env.update(XLANG_PACKAGE_PATH=str(packages),TEMP=str(scratch),TMP=str(scratch),TMPDIR=str(scratch))
        key='PATH' if win else 'LD_LIBRARY_PATH'
        env[key]=os.pathsep.join((str(packages/'_xge_native/native'),str(engine),str(compiler.parent),env.get(key,'')))
        startup=None
        if win:startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        def no_core():
            import resource
            resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        command=list(map(str,command));start=time.monotonic()
        z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300,startupinfo=startup,preexec_fn=None if win else no_core)
        data=z.stdout+z.stderr;log=out/(name+'.log');log.write_bytes(data)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,passed=not z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)));save()
        print(name,z.returncode,flush=True);assert not z.returncode,log
        return data
    includes=['--inc-path',root,'--inc-path',sdk,'--inc-path',native]
    ci=['-I'+str(v) for v in (fresh/'include',root,sdk,native,native/'include',*sorted(dynamic.glob('*/include')))]
    try:
        # Exact replay guards the separate AnnotationStore source as well as
        # classes emitted by the managed declaration generator.
        managed=(dynamic/'xui/src/managed.xl').read_text(encoding='utf-8-sig')
        annotations=(dynamic/'xui/src/code_annotations.xl').read_text(encoding='utf-8-sig')
        for symbol,row in RECORD_OUTPUTS.items():assert declaration(symbol).strip() in (annotations if row[0]=='CodeAnnotationStore' else managed)
        assert STYLE_TOKEN_SET.strip() in managed and VIRTUAL_TEXT_SET.strip() in managed
        for kind,folder,lib in (('host',engine,library),('static',static_build,archive)):
            flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-cast-function-type','-DXGE_XRT_HOST=1','-include',folder/'xge_build_config.h',*ci,'-I'+str(folder),'-I'+str(engine/'host-xrt')]
            flags+=['-DXGE_TEST_STATIC=1','-DXL6_XGE_STATIC=1'] if kind=='static' else ['-DXGE_DLL=1','-DXUI_DLL=1'] if win else []
            systems=['-l'+n for n in s['system_libraries']] if kind=='static' else []
            if not win:systems+=['-Wl,-rpath,'+str(engine),'-Wl,-rpath,'+str(runtime.parent),'-ldl','-lm','-pthread']
            for opt in ('O0','O2'):
                name='native-'+kind+'-'+opt;binary=out/(name+('.exe' if win else ''))
                run(name+'-build',[tools['gcc'],*flags,'-'+opt,driver,fixture,lib,runtime,*systems,'-o',binary]);data=run(name,[binary])
                assert b'Record queries native: authoritative fields' in data
        for source in (case,example):
            name=source.stem;c=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,'--emit',c,'--emit-header',header,*includes]);code=c.read_text(encoding='utf-8-sig')
            for symbol in (RECORD_OUTPUTS if source==case else ('xuiCodeAnnotationGetDiagnostic',)):
                assert re.search(r'\b'+symbol+r'\(',code)
            for standard,tool,language in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run(name+'-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*ci,'-x',language,'-fsyntax-only',header])
            for kind,packages in (('jit',dynamic),('dynamic',dynamic),('static',static)):
                if kind=='jit':data=run(name+'-'+kind,[compiler,'run',source,*includes],packages)
                else:
                    binary=out/(name+'-'+kind+('.exe' if win else ''))
                    run(name+'-'+kind+'-build',[compiler,'build',source,'-o',binary,*includes],packages);data=run(name+'-'+kind+'-execute',[binary],packages)
                    table=run(name+'-'+kind+'-imports',[tools['objdump'],'-p',binary],packages)
                    imports=re.findall(rb'DLL Name:\s*(\S+)',table) if win else re.findall(rb'\bNEEDED\s+(\S+)',table);lower={v.lower() for v in imports}
                    assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                    assert any(b'xge3_host' in v for v in lower)==(kind=='dynamic')
                    report['programs'].append(dict(path=str(binary),sha256=sha(binary),bytes=binary.stat().st_size,imports=[v.decode() for v in imports]))
                if source==case:
                    assert b'Record queries: exact fields' in data
                    heap=re.findall(rb'Record query heap kind=(\d) (language|native): alloc=(\d+) free=(\d+) realloc=(\d+)',data)
                    assert len(heap)==8
                    for i in range(4):
                        pair=heap[i*2:i*2+2];assert [v[1] for v in pair]==[b'language',b'native'] and all(int(v[0])==i for v in pair)
                        values=[list(map(int,v[2:])) for v in pair]
                        assert all(v[0]==v[1]>0 and v[2]==0 for v in values)
                        assert values[0][0]-values[1][0]==(1000 if i==3 else 0)
                        report['heap'].append(dict(mode=kind,query=i,rounds=1000,language=values[0],native=values[1],scope='Equivalent owned-string copies; style name adds one input C-string allocation; no CPU zero-overhead claim'))
                    oom=re.findall(rb'Record query OOM: prefixes=32 annotation=\s*(\d+)\s+token=\s*(\d+)\s+virtual=\s*(\d+)\s+accessible=\s*(\d+)',data)
                    assert len(oom)==1 and all(0<int(v)<32 for v in oom[0])
                    report['budgets'].append(dict(mode=kind,prefixes=32,annotation=int(oom[0][0]),token=int(oom[0][1]),virtual=int(oom[0][2]),accessible=int(oom[0][3])));save()
                else:assert b'W001' in data and b'Complete this task' in data
        report.update(complete=True,inputsUnchanged=all(sha(f)==v for f,v in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
