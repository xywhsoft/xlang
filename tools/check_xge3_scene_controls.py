"""Qualify direct Scene queries, appearance controls and incremental retirement.

Complete compiler products, actual matched hosted/static SDKs and replayed
packages are required. A controlled CPU mesh fixture supplies geometry; it
does not promote raw mesh pointers to the public language API. No GPU/network
tests or final runtime installation are performed by this gate.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
from full_compiler_qualification import verify_full_compiler
from stage_xge3_replay_contracts import verify_replay_package
from check_xui_flow_busy_sdk import DRIVER


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','compiler-report','native-source','sdk-build','dynamic-packages','static-packages','replay-report','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,cc,build,native,sdk,dynamic,static,replay,out=[v.resolve() for v in
        (a.root,a.compiler,a.compiler_report,a.native_source,a.sdk_build,a.dynamic_packages,a.static_packages,a.replay_report,a.out)]
    win=os.name=='nt';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();verify_full_compiler(build,cc)
    b=json.loads((sdk/'results.json').read_text());assert b['complete'] and b['passed'] and Path(b['source']).resolve()==native
    case=root/'test/cases/foreign_xge3d_scene_controls.xl';lesson=root/'lib/bindings/xge/three_d/examples/scene_controls.xl'
    h=json.loads((sdk/'host/build-report.json').read_text());s=json.loads((sdk/'static/build-report.json').read_text())
    assert h['linked'] and s['passed'] and h['xrt_mode']=='host' and s['xrt_mode']=='direct-external'
    runtime=cc.parent/('xlc.dll' if win else 'libxlc.so');host=sdk/'host';archive=sdk/'static'/s['archive']
    library=host/(h['import_library'] if win else h['runtime_image'])
    tools={n:Path(shutil.which(n)).resolve() for n in ('gcc','g++','objdump')}
    files=[Path(__file__).resolve(),Path(__file__).with_name('full_compiler_qualification.py'),
        Path(__file__).with_name('stage_xge3_replay_contracts.py'),Path(__file__).with_name('check_xui_flow_busy_sdk.py'),
        cc,build,runtime,case,case.with_suffix('.c'),case.with_suffix('.h'),lesson,sdk/'results.json',
        host/'build-report.json',sdk/'static/build-report.json',library,archive,host/h['runtime_image'],*tools.values()]
    files.extend(cc.parent/n for n in (('xl.dll',) if win else ('libxl.so',)))
    for packages in (dynamic,static):files.extend(verify_replay_package(packages,replay))
    for folder in (native/'include',native/'src/xge/3d',native/'extlibs/xrt',host/'host-xrt'):
        files.extend(p for p in folder.rglob('*') if p.is_file())
    files.extend(sdk/k/'xge_build_config.h' for k in ('host','static'))
    files.append(sdk/'static/xge_static_xrt_contract.h')
    regressions=[root/'lib/bindings/xge/three_d/examples'/n for n in ('scene_hierarchy.xl','ray_queries.xl')]
    files.extend(regressions)
    inputs={str(f):sha(f) for f in files};out.mkdir(parents=True,exist_ok=False)
    report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[],programs=[],
        shippingDelivery=False,graphicsTests=False,networkTests=False,cpuEquivalenceClaim=False)
    def save():
        tmp=out/'results.json.tmp';tmp.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');os.replace(tmp,out/'results.json')
    def run(name,command,packages=dynamic,marker=None):
        env={k:v for k,v in os.environ.items() if not k.startswith('XL6_') and k!='XLANG_PACKAGE_PATH'}
        env.update(XLANG_PACKAGE_PATH=str(packages),TMP=str(out),TEMP=str(out),TMPDIR=str(out))
        key='PATH' if win else 'LD_LIBRARY_PATH';env[key]=os.pathsep.join((str(packages/'_xge_native/native'),str(host),str(cc.parent),env.get(key,'')))
        command=list(map(str,command));start=time.monotonic();z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
        data=z.stdout+z.stderr;log=out/(name+'.log');log.write_bytes(data)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert z.returncode==0,log
        if marker:assert marker in data,log
        return data
    def imports(name,binary,hosted,compiler_runtime=False):
        data=run(name+'-imports',[tools['objdump'],'-p',binary])
        names=re.findall(rb'DLL Name:\s*(\S+)',data) if win else re.findall(rb'\bNEEDED\s+(\S+)',data)
        lowered={n.lower() for n in names}
        if not compiler_runtime:assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lowered)
        assert any(b'xge3_host' in n for n in lowered)==hosted
        report['programs'].append(dict(path=str(binary),sha256=sha(binary),bytes=binary.stat().st_size,imports=[n.decode() for n in names]));save()
    try:
        includes=['--inc-path',root,'--inc-path',cc.parent/'include','--inc-path',native]
        ci=['-I'+str(p) for p in (root,native,cc.parent/'include',*sorted(dynamic.glob('*/include')))]
        for source,marker in ((case,b'XGE3D_SCENE_CONTROLS_OK'),(lesson,b'Scene controls passed')):
            name=source.stem;generated=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[cc,'cgen',source,'--emit',generated,'--emit-header',header,*includes])
            if source==case:
                text=generated.read_text(encoding='utf8')
                for fn in ('xge3dNodeGetBounds','xge3dSceneRaycast','xge3dSceneRaycastEx','xge3dScenePrepareSpatial',
                    'xge3dNodeSetLight','xge3dNodeSetColor','xge3dNodeSetShadowFlags','xge3dNodeGetShadowFlags','xge3dNodeDestroyStep'):
                    assert re.search(r'(?<![\w&])'+fn+r'\(',text),fn
                assert re.search(r'xl6_xge3d_scene_scope_t\s+\w+\s*=\s*\{0\}',text)
            for std,tool,language in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run(name+'-header-'+std,[tools[tool],'-std='+std,'-Wall','-Wextra','-Werror',*ci,'-x',language,'-fsyntax-only',header])
            data=run(name+'-jit',[cc,'run',source,*includes],marker=marker)
            if source==case:assert data.count(b'SCENE_CONTROLS_WARM_XRT_ALLOCATIONS=0')==2
            for kind,packages in (('dynamic',dynamic),('static',static)):
                binary=out/(name+'-'+kind+('.exe' if win else ''))
                run(name+'-'+kind+'-build',[cc,'build',source,'-o',binary,*includes],packages)
                data=run(name+'-'+kind+'-run',[binary],packages,marker);imports(name+'-'+kind,binary,kind=='dynamic')
                if source==case:assert data.count(b'SCENE_CONTROLS_WARM_XRT_ALLOCATIONS=0')==2
        shared=out/'controls-shared.xl';shared.write_text(case.read_text().replace('func main(...) as int','export "controls_check" func check() as int'),encoding='utf8')
        module=out/('controls-shared'+('.dll' if win else '.so'))
        run('shared-build',[cc,'build',shared,case.with_suffix('.c'),'--shared','-o',module,*includes])
        host_py=out/'shared-host.py';host_py.write_text('import ctypes,sys,os,_ctypes\n'
            'm=ctypes.CDLL(sys.argv[1],**({"winmode":0} if os.name=="nt" else {}))\n'
            'm.xl_script_module_init.argtypes=[ctypes.c_void_p]\nm.xl_script_module_init.restype=ctypes.c_int\nassert m.xl_script_module_init(None)==1\n'
            'm.controls_check.restype=ctypes.c_int\nassert m.controls_check()==0\n'
            'm.xl_script_module_unit_checked.restype=ctypes.c_int\nassert m.xl_script_module_unit_checked()==1\n'
            'if os.name=="nt":\n m.xl_script_runtime_retire_checked.restype=ctypes.c_int\n assert m.xl_script_runtime_retire_checked()==1\n'
            '(_ctypes.FreeLibrary if os.name=="nt" else _ctypes.dlclose)(m._handle)\nprint("SCENE_CONTROLS_SHARED_UNLOAD_OK")\n',encoding='utf8')
        run('shared-run',[sys.executable,'-B',host_py,module],marker=b'SCENE_CONTROLS_SHARED_UNLOAD_OK');imports('shared',module,True)
        driver=out/'native-driver.c';driver.write_text(DRIVER.replace('fixtureNativeMain','fixture3dControlsNativeMain').replace(
            '#define XRT_USE_SHARED\n','#ifndef XRT_USE_SHARED\n#define XRT_USE_SHARED\n#endif\n'),encoding='utf8')
        for kind,folder,lib in (('host',host,library),('static',sdk/'static',archive)):
            flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-cast-function-type',
                '-DXGE_XRT_HOST=1','-include',folder/'xge_build_config.h',*ci,'-I'+str(folder),'-I'+str(host/'host-xrt')]
            if kind=='static':flags+=['-DXGE_TEST_STATIC=1','-DXL6_XGE_STATIC=1']
            elif win:flags+=['-DXGE_DLL=1','-DXUI_DLL=1']
            if win:flags+=['-DXRT_USE_SHARED=1']
            systems=['-l'+n for n in s['system_libraries']] if kind=='static' else []
            if not win:systems+=['-Wl,-rpath,'+str(host),'-Wl,-rpath,'+str(cc.parent),'-pthread','-ldl','-lm']
            for opt in ('O0','O2'):
                name='native-'+kind+'-'+opt;exe=out/(name+('.exe' if win else ''))
                run(name+'-build',[tools['gcc'],*flags,'-'+opt,case.with_suffix('.c'),driver,lib,runtime,*systems,'-o',exe])
                run(name+'-run',[exe],marker=b'SCENE_CONTROLS_NATIVE_OK');imports(name,exe,kind=='host',True)
        for source in regressions:run('regression-'+source.stem,[cc,'run',source,*includes])
        for packages in (dynamic,static):verify_replay_package(packages,replay)
        verify_full_compiler(build,cc);report.update(complete=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except BaseException as error:report.update(complete=True,error=repr(error));raise
    finally:save()


if __name__=='__main__':main()
