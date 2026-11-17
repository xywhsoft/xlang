"""Qualify fixed C-array default values through full compiler products.

Typed/lowered initializer contracts, scalar/record arrays, native record fields,
copy/bounds/loop behavior, suspension, strict public headers and standalone AOT.
Exercise the manual triangle lesson with matched hosted/static XGE packages.
No network, graphics, shipping install or CPU-equivalence claim.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
from full_compiler_qualification import verify_full_compiler
from stage_xge3_replay_contracts import verify_replay_package


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','compiler-report','dynamic-packages','static-packages','replay-report','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,cc,build,dynamic,static,replay,out=[v.resolve() for v in
        (a.root,a.compiler,a.compiler_report,a.dynamic_packages,a.static_packages,a.replay_report,a.out)]
    verify_full_compiler(build,cc);win=os.name=='nt';sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    sdk=cc.parent/'include';case=root/'test/cases/foreign_fixed_array_zero_init.xl'
    asynchronous=root/'test/cases/foreign_fixed_array_zero_async.xl'
    lesson=root/'lib/bindings/xge/three_d/examples/ray_queries.xl'
    negative=[root/'test/cases'/('bad_fixed_array_zero_'+n+'.xl') for n in
        ('owned_element','plain_class_field','self_reference','scalar_unassigned')]
    adjacent=[root/'test/cases'/n for n in ('xlir_value_initializer_defaults.xl',
        'xlir_value_initializer_class_default.xl','xlir_value_initializer_class_default_differential.xl',
        'xlir_value_initializer_class_null.xl',
        'xlir_value_initializer_containers.xl','foreign_decl_array.xl')]
    tools={n:Path(shutil.which(n)).resolve() for n in ('gcc','g++','objdump')}
    files=[Path(__file__).resolve(),Path(__file__).with_name('full_compiler_qualification.py'),cc,build,case,
        asynchronous,case.with_suffix('.c'),case.with_suffix('.h'),lesson,*negative,*adjacent,*tools.values()]
    array_provider=root/'test/cases/foreign_decl_array_native.c'
    files.append(array_provider)
    files.extend(p for source in adjacent for p in (source.with_suffix('.c'),source.with_suffix('.h')) if p.is_file())
    files.extend(cc.parent/n for n in (('xl.dll','xlc.dll') if win else ('libxl.so','libxlc.so')))
    for packages in (dynamic,static):files.extend(verify_replay_package(packages,replay))
    inputs={str(f):sha(f) for f in files};out.mkdir(parents=True,exist_ok=False)
    report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[],programs=[],initializerPlans={},
        fullCompilerBuild=True,shippingDelivery=False,networkTests=False,graphicsTests=False,cpuEquivalenceClaim=False)
    def save():
        tmp=out/'results.json.tmp';tmp.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');os.replace(tmp,out/'results.json')
    def run(name,command,packages=dynamic,marker=None,reject=False):
        env={k:v for k,v in os.environ.items() if not k.startswith('XL6_') and k!='XLANG_PACKAGE_PATH'}
        env.update(XLANG_PACKAGE_PATH=str(packages),TMP=str(out),TEMP=str(out),TMPDIR=str(out))
        key='PATH' if win else 'LD_LIBRARY_PATH'
        env[key]=os.pathsep.join((str(packages/'_xge_native/native'),str(cc.parent),env.get(key,'')))
        command=list(map(str,command));start=time.monotonic()
        z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
        data=z.stdout+z.stderr;log=out/(name+'.log');log.write_bytes(data)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,expectedRejection=reject,
            seconds=time.monotonic()-start,logSha256=sha(log)));save();print(name,z.returncode,flush=True)
        assert bool(z.returncode)==reject,log
        if marker:assert marker in data,log
        return data
    def imports(name,binary,hosted):
        data=run(name+'-imports',[tools['objdump'],'-p',binary])
        names=re.findall(rb'DLL Name:\s*(\S+)',data) if win else re.findall(rb'\bNEEDED\s+(\S+)',data)
        lowered={n.lower() for n in names}
        assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lowered)
        assert any(b'xge3_host' in n for n in lowered)==hosted
        report['programs'].append(dict(path=str(binary),sha256=sha(binary),bytes=binary.stat().st_size,imports=[n.decode() for n in names]));save()
    try:
        includes=['--inc-path',root,'--inc-path',sdk]
        ci=['-I'+str(v) for v in (root,sdk,*sorted(dynamic.glob('*/include')))]
        for source,expected,marker in ((case,13,b'FIXED_ARRAY_ZERO_INIT_OK'),
            (asynchronous,1,b'FIXED_ARRAY_ZERO_ASYNC_OK'),(lesson,1,b'triangle=3')):
            name=source.stem
            for stage in ('typed','lowered'):
                path=out/(name+'-'+stage+'.json')
                run(name+'-'+stage,[cc,'ir',source,*(['--lowered'] if stage=='lowered' else []),'--format','json','--emit',path,*includes])
                d=json.loads(path.read_text(encoding='utf8'));plans=[v for v in d['valueInitializers'] if v['abiName']=='c-array-zero']
                assert len(plans)==expected,(path,len(plans))
                assert all(v['expression']==0 and v['runtimeOpName']=='none' and v['errorAbiName']=='no-raise' for v in plans)
                if stage=='lowered':
                    for v in plans:
                        assert any(i['kindName']=='union-zero' and i['origin']==v['declaration'] and i['type']==v['type'] and
                            not i['operands'] and i['runtimeOpName']=='none' for i in d['instructions']),(path,v)
                report['initializerPlans'][name+'-'+stage]=dict(count=len(plans),path=str(path),sha256=sha(path));save()
            generated=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[cc,'cgen',source,'--emit',generated,'--emit-header',header,*includes])
            for standard,tool,language in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run(name+'-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*ci,'-x',language,'-fsyntax-only',header])
            data=run(name+'-jit',[cc,'run',source,*includes],marker=marker)
            if source==case:assert b'FIXED_ARRAY_ZERO_HOT_ALLOCATIONS=0' in data
            for kind,packages in (('dynamic',dynamic),('static',static)) if source==lesson else (('standalone',dynamic),):
                binary=out/(name+'-'+kind+('.exe' if win else ''))
                run(name+'-'+kind+'-build',[cc,'build',source,'-o',binary,*includes],packages)
                data=run(name+'-'+kind+'-run',[binary],packages,marker);imports(name+'-'+kind,binary,source==lesson and kind=='dynamic')
                if source==case:assert b'FIXED_ARRAY_ZERO_HOT_ALLOCATIONS=0' in data
        shared=out/'fixed-zero-shared.xl'
        shared.write_text(case.read_text(encoding='utf8').replace('func main(...) as int','export "fixed_zero_check" func check() as int'),encoding='utf8')
        library=out/('fixed-zero-shared'+('.dll' if win else '.so'))
        run('shared-build',[cc,'build',shared,case.with_suffix('.c'),'--shared','-o',library,*includes])
        host=out/'shared-host.py'
        host.write_text('import ctypes,sys,os,_ctypes\n'
            'lib=ctypes.CDLL(sys.argv[1],**({"winmode":0} if os.name=="nt" else {}))\n'
            'lib.xl_script_module_init.argtypes=[ctypes.c_void_p]\nlib.xl_script_module_init.restype=ctypes.c_int\n'
            'assert lib.xl_script_module_init(None)==1\nlib.fixed_zero_check.restype=ctypes.c_int\nassert lib.fixed_zero_check()==0\n'
            'lib.xl_script_module_unit_checked.restype=ctypes.c_int\nassert lib.xl_script_module_unit_checked()==1\n'
            'if os.name=="nt":\n lib.xl_script_runtime_retire_checked.restype=ctypes.c_int\n assert lib.xl_script_runtime_retire_checked()==1\n'
            '(_ctypes.FreeLibrary if os.name=="nt" else _ctypes.dlclose)(lib._handle)\nprint("FIXED_ARRAY_ZERO_UNLOAD_OK")\n',encoding='utf8')
        run('shared-run',[sys.executable,'-B',host,library],marker=b'FIXED_ARRAY_ZERO_UNLOAD_OK');imports('shared',library,False)
        for source in negative:
            path=out/(source.stem+'.c');run(source.stem,[cc,'cgen',source,'--emit',path,*includes],reject=True);assert not path.exists()
        for source in adjacent:
            provider=[array_provider] if source.name=='foreign_decl_array.xl' else []
            run('regression-'+source.stem,[cc,'run',source,*provider,*includes])
        for packages in (dynamic,static):verify_replay_package(packages,replay)
        verify_full_compiler(build,cc)
        report.update(complete=True,inputsUnchanged=all(sha(f)==h for f,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except BaseException as error:report.update(complete=True,error=repr(error));raise
    finally:save()


if __name__=='__main__':main()
