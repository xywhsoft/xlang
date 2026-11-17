"""Qualify shared CPU 3D Scenes through matched SDKs and full compilers.

Hierarchy, transforms, retained names, status-preserving lookup, shared owners,
checked expiry, native active pins, OOM and zero-allocation warmed queries.
JIT, hosted/static EXE and acyclic shared-module checked retirement/unload.
No graphics/network tests, arbitrary callback cycles or shipping installation.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
from full_compiler_qualification import verify_full_compiler
from stage_xge3_replay_contracts import verify_replay_package


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','compiler-report','engine','dynamic-packages','static-packages','replay-report','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,cc,build,engine,dynamic,static,replay,out=[v.resolve() for v in
        (a.root,a.compiler,a.compiler_report,a.engine,a.dynamic_packages,a.static_packages,a.replay_report,a.out)]
    verify_full_compiler(build,cc);win=os.name=='nt';sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    sdk=cc.parent/'include';case=root/'test/cases/foreign_xge3d_scene.xl';provider=case.with_suffix('.c')
    lesson=root/'lib/bindings/xge/three_d/examples/scene_hierarchy.xl'
    files=[Path(__file__).resolve(),Path(__file__).with_name('full_compiler_qualification.py'),cc,build,case,
        provider,case.with_suffix('.h'),lesson,*[cc.parent/n for n in (('xl.dll','xlc.dll') if win else ('libxl.so','libxlc.so'))]]
    for packages in (dynamic,static):files.extend(verify_replay_package(packages,replay))
    tools={n:Path(shutil.which(n)).resolve() for n in ('gcc','g++','objdump')};files.extend(tools.values())
    inputs={str(f):sha(f) for f in files};out.mkdir(parents=True,exist_ok=False)
    report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[],programs=[],costs=[],
        fullCompilerBuild=True,shippingDelivery=False,networkTests=False,graphicsTests=False,cpuEquivalenceClaim=False)
    def save():
        tmp=out/'results.json.tmp';tmp.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');os.replace(tmp,out/'results.json')
    def run(name,command,packages=dynamic,marker=None):
        env={k:v for k,v in os.environ.items() if not k.startswith('XL6_') and k!='XLANG_PACKAGE_PATH'}
        env.update(XLANG_PACKAGE_PATH=str(packages),TMP=str(out),TEMP=str(out),TMPDIR=str(out))
        key='PATH' if win else 'LD_LIBRARY_PATH'
        env[key]=os.pathsep.join((str(packages/'_xge_native/native'),str(engine),str(cc.parent),env.get(key,'')))
        command=list(map(str,command));start=time.monotonic()
        z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
        data=z.stdout+z.stderr;log=out/(name+'.log');log.write_bytes(data)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert z.returncode==0,log
        if marker:assert marker in data,log
        return data
    def imports(name,binary,hosted):
        data=run(name+'-imports',[tools['objdump'],'-p',binary])
        names=re.findall(rb'DLL Name:\s*(\S+)',data) if win else re.findall(rb'\bNEEDED\s+(\S+)',data)
        lowered={n.lower() for n in names}
        assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lowered)
        assert any(b'xge3_host' in n for n in lowered)==hosted
        report['programs'].append(dict(path=str(binary),sha256=sha(binary),bytes=binary.stat().st_size,imports=[n.decode() for n in names]));save()
    def costs(name,data):
        assert data.count(b'scene-query-xrt-allocations=0')==2
        cold=re.findall(rb'scene language cold kind=(\d+) allocations=(\d+) bytes=(\d+)',data)
        assert len(cold)==2 and [v[0] for v in cold]==[b'0',b'1']
        report['costs'].append(dict(name=name,warmCallsPerPath=1000,warmAllocations=0,
            cold=[list(map(int,v)) for v in cold],cpuEquivalenceClaim=False));save()
    try:
        includes=['--inc-path',root,'--inc-path',sdk]
        ci=['-I'+str(v) for v in (root,sdk,*sorted(dynamic.glob('*/include')))]
        for source in (case,lesson):
            name=source.stem;generated=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[cc,'cgen',source,'--emit',generated,'--emit-header',header,*includes])
            if source==case:
                text=generated.read_text(encoding='utf-8-sig')
                for native in ('xge3dSceneCreateOwned','xge3dSceneShareOwned','xge3dNodeCreate','xge3dNodeSetTransform','xge3dNodeGetWorldMatrix','xge3dNodeName','xge3dNodeFind'):
                    assert re.search(r'(?<![\w&])'+native+r'\(',text),native
                assert re.search(r'xl6_xge3d_scene_scope_t\s+\w+\s*=\s*\{0\}',text)
            for standard,tool,language in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run(name+'-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*ci,'-x',language,'-fsyntax-only',header])
            marker=b'XGE3D_SCENE_LANGUAGE_OK' if source==case else b'camera mount'
            # The CLI includes the same-stem C companion for the original case.
            # The relocated shared source below needs its provider explicitly.
            extra=[]
            data=run(name+'-jit',[cc,'run',source,*extra,*includes],marker=marker)
            if source==case:costs(name+'-jit',data)
            for kind,packages in (('dynamic',dynamic),('static',static)):
                binary=out/(name+'-'+kind+('.exe' if win else ''))
                run(name+'-'+kind+'-build',[cc,'build',source,*extra,'-o',binary,*includes],packages)
                data=run(name+'-'+kind+'-run',[binary],packages,marker);imports(name+'-'+kind,binary,kind=='dynamic')
                if source==case:costs(name+'-'+kind,data)
        shared=out/'scene-shared.xl'
        shared.write_text(case.read_text(encoding='utf8').replace('func main(...) as int','export "scene_check" func check() as int'),encoding='utf8')
        host=out/'shared-host.py'
        host.write_text('import ctypes,sys,os,_ctypes\n'
            'lib=ctypes.CDLL(sys.argv[1],**({"winmode":0} if os.name=="nt" else {}))\n'
            'lib.xl_script_module_init.argtypes=[ctypes.c_void_p]\nlib.xl_script_module_init.restype=ctypes.c_int\n'
            'assert lib.xl_script_module_init(None)==1\nlib.scene_check.restype=ctypes.c_int\nassert lib.scene_check()==0\n'
            'lib.xl_script_module_unit_checked.restype=ctypes.c_int\nassert lib.xl_script_module_unit_checked()==1\n'
            'if os.name=="nt":\n lib.xl_script_runtime_retire_checked.restype=ctypes.c_int\n assert lib.xl_script_runtime_retire_checked()==1\n'
            '(_ctypes.FreeLibrary if os.name=="nt" else _ctypes.dlclose)(lib._handle)\nprint("SCENE_CHECKED_UNLOAD_OK")\n',encoding='utf8')
        for kind,packages in (('dynamic',dynamic),('static',static)):
            library=out/('scene-'+kind+('.dll' if win else '.so'))
            run(kind+'-shared-build',[cc,'build',shared,provider,'--shared','-o',library,*includes],packages)
            data=run(kind+'-shared-run',[sys.executable,'-B',host,library],packages,b'SCENE_CHECKED_UNLOAD_OK')
            costs(kind+'-shared',data);imports(kind+'-shared',library,kind=='dynamic')
        for packages in (dynamic,static):verify_replay_package(packages,replay)
        verify_full_compiler(build,cc)
        report.update(complete=True,inputsUnchanged=all(sha(f)==h for f,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except BaseException as error:report.update(complete=True,error=repr(error));raise
    finally:save()


if __name__=='__main__':main()
