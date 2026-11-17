"""Qualify direct 3D ray/volume/triangle queries against matching native SDKs.

CPU geometry only; no window, graphics device, network or release install.
Native statuses, fixed three-vertex input and output records remain explicit.
XRT heap counts do not measure CPU equivalence or OS allocator calls.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from full_compiler_qualification import verify_full_compiler
from compiler_response import command_with_response
from stage_xge3_replay_contracts import verify_replay_package


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','compiler-report','engine','static-build',
                 'dynamic-packages','static-packages','dynamic-replay-report','static-replay-report','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    root,cc,build,engine,static_build,dynamic,static,dr,sr,out=[p.resolve() for p in
        (args.root,args.compiler,args.compiler_report,args.engine,args.static_build,
         args.dynamic_packages,args.static_packages,args.dynamic_replay_report,args.static_replay_report,args.out)]
    verify_full_compiler(build,cc)
    win=os.name=='nt';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    sdk=cc.parent/'include';runtime=cc.parent/('xlc.dll' if win else 'libxlc.so')
    host=json.loads((engine/'build-report.json').read_text(encoding='utf8'))
    stat=json.loads((static_build/'build-report.json').read_text(encoding='utf8'))
    assert host['linked'] and host['xrt_mode']=='host'
    assert stat['passed'] and stat['sourceUnchanged'] and stat['xrt_mode']=='direct-external'
    image=engine/host['runtime_image'];library=engine/host['import_library'] if win else image
    archive=static_build/stat['archive']
    case=root/'test/cases/foreign_xge3_ray_queries.xl'
    fixture=case.with_suffix('.c');header=case.with_suffix('.h');driver=case.with_name(case.stem+'_native.c')
    lesson=root/'lib/bindings/xge/three_d/examples/ray_queries.xl'
    tools={name:Path(shutil.which(name)).resolve() for name in ('gcc','g++','objdump')}
    files=[Path(__file__),Path(__file__).with_name('full_compiler_qualification.py'),
        Path(__file__).with_name('compiler_response.py'),cc,runtime,build,case,fixture,header,driver,lesson,
        image,library,archive,*tools.values(),engine/'build-report.json',static_build/'build-report.json',
        engine/'xge_build_config.h',static_build/'xge_build_config.h']
    for packages,receipt in ((dynamic,dr),(static,sr)):
        files.extend(verify_replay_package(packages,receipt))
    for folder in (sdk,dynamic,static):files.extend(p for p in folder.rglob('*') if p.is_file())
    inputs={str(p):sha(p) for p in files};out.mkdir(parents=True,exist_ok=False)
    report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[],programs=[],
        fullCompilerBuild=True,shippingDelivery=False,networkTests=False,graphicsTests=False,cpuEquivalenceClaim=False)
    def save():
        temporary=out/'results.json.tmp'
        temporary.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
        os.replace(temporary,out/'results.json')
    def run(name,command,packages=dynamic,marker=None):
        env={k:v for k,v in os.environ.items() if not k.startswith('XL6_') and k!='XLANG_PACKAGE_PATH'}
        env.update(XLANG_PACKAGE_PATH=str(packages),TMP=str(out),TEMP=str(out),TMPDIR=str(out))
        key='PATH' if win else 'LD_LIBRARY_PATH'
        env[key]=os.pathsep.join((str(packages/'_xge_native/native'),str(engine),str(cc.parent),env.get(key,'')))
        command=list(map(str,command));actual=command_with_response(command,tools['gcc'],out/(name+'.rsp'))
        started=time.monotonic();z=subprocess.run(actual,cwd=root,env=env,capture_output=True,timeout=300)
        data=z.stdout+z.stderr;log=out/(name+'.log');log.write_bytes(data)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,
            seconds=time.monotonic()-started,logSha256=sha(log)));save();print(name,z.returncode,flush=True)
        assert not z.returncode,log
        if marker:assert marker in data,log
        return data
    def imports(name,program,hosted):
        data=run(name+'-imports',[tools['objdump'],'-p',program])
        names=re.findall(rb'DLL Name:\s*(\S+)',data) if win else re.findall(rb'\bNEEDED\s+(\S+)',data)
        lowered={n.lower() for n in names}
        assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lowered)
        assert any(b'xge3_host' in n for n in lowered)==hosted
        report['programs'].append(dict(path=str(program),sha256=sha(program),bytes=program.stat().st_size,
            imports=[n.decode() for n in names]));save()
    try:
        includes=['--inc-path',root,'--inc-path',sdk]
        ci=['-I'+str(p) for p in (root,sdk,*sorted(dynamic.glob('*/include')))]
        for kind,folder,link in (('host',engine,library),('static',static_build,archive)):
            flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-cast-function-type','-DXGE_XRT_HOST=1',
                '-include',folder/'xge_build_config.h',*ci,'-I'+str(folder),'-I'+str(engine/'host-xrt')]
            flags+=['-DXGE_TEST_STATIC=1','-DXL6_XGE_STATIC=1'] if kind=='static' else (['-DXGE_DLL=1','-DXUI_DLL=1'] if win else [])
            systems=['-l'+n for n in stat['system_libraries']] if kind=='static' else []
            if not win:systems+=['-Wl,-rpath,'+str(engine),'-Wl,-rpath,'+str(runtime.parent),'-ldl','-lm','-pthread']
            for opt in ('O0','O2'):
                name='native-'+kind+'-'+opt;binary=out/(name+('.exe' if win else ''))
                run(name+'-build',[tools['gcc'],*flags,'-'+opt,driver,fixture,link,runtime,*systems,'-o',binary])
                run(name+'-run',[binary],marker=b'XGE3D_RAY_NATIVE_OK')
        for source in (case,lesson):
            name=source.stem;generated=out/(name+'.c');public=out/(name+'.h')
            run(name+'-emit',[cc,'cgen',source,'--emit',generated,'--emit-header',public,*includes])
            if source==case:
                text=generated.read_text(encoding='utf-8-sig')
                for native in ('xge3dRayAabb','xge3dRaySphere','xge3dRayTriangle'):
                    assert re.search(r'\.e0\s*=\s*'+native+r'\(',text),native
                assert re.search(r'xge3dRayTriangle\([^;]+\.Elements',text)
                for standard,tool,language in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                    run('header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*ci,
                        '-x',language,'-fsyntax-only',public])
            for kind,packages in (('jit',dynamic),('dynamic',dynamic),('static',static)):
                if kind=='jit':data=run(name+'-jit',[cc,'run',source,*includes],packages)
                else:
                    program=out/(name+'-'+kind+('.exe' if win else ''))
                    run(name+'-'+kind+'-build',[cc,'build',source,'-o',program,*includes],packages)
                    data=run(name+'-'+kind+'-run',[program],packages);imports(name+'-'+kind,program,kind=='dynamic')
                marker=b'XGE3D_RAY_QUERIES_OK' if source==case else b'3D ray queries passed:'
                assert marker in data
                if source==case:assert data.count(b'calls=3000 alloc=0 free=0 realloc=0')==2
        verify_full_compiler(build,cc)
        report.update(complete=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except BaseException as error:report.update(complete=True,passed=False,error=repr(error));raise
    finally:save()


if __name__=='__main__':main()
