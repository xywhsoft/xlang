"""Qualify strict scoped data snapshot inputs with a complete O2 compiler.

CPU-only: direct, stored/erased, bound/field and packed inputs, public schemas,
source isolation, owned results, native errors, rollback and allocation faults.
No final installation, device, graphics, network or security tests.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
from foreign_compiler_cohort import load_compiler_cohort
from stage_runtime_headers import stage as stage_runtime_headers


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','compiler','compiler-report','out'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();root,compiler,build,out=[v.resolve() for v in (a.root,a.compiler,a.compiler_report,a.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);assert not out.is_relative_to(root)
    win=os.name=='nt';sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    meta=json.loads(build.read_text('utf-8'));case=root/'test/cases/foreign_data_snapshot.xl'
    helper=case.with_suffix('.c');header=case.with_suffix('.h');host=root/'test/cases/foreign_decl_module_host.c'
    runtime=compiler.parent/('xl.dll' if win else 'libxl.so')
    tools={n:Path(shutil.which(n)).resolve() for n in ('gcc','g++','objdump')}
    files=[Path(__file__).resolve(),Path(__file__).with_name('foreign_compiler_cohort.py'),
        Path(__file__).with_name('full_compiler_qualification.py'),compiler,build,Path(meta['sourceFreeze']),
        runtime,case,helper,header,host,root/'test/cases/stdlib_template_privacy.xl',*extra,*tools.values()]
    files.extend(f for f in sdk.rglob('*') if f.is_file());inputs={str(f):sha(f) for f in files}
    out.mkdir(parents=True,exist_ok=False);scratch=out/'tmp';scratch.mkdir()
    snapshots=out/'input-snapshots';snapshots.mkdir()
    for path in (case,helper,header,Path(__file__).resolve()):shutil.copy2(path,snapshots/path.name)
    frozen=Path(meta['source']);fresh=out/'runtime-sdk'
    staged=stage_runtime_headers(frozen,fresh,frozen/'tools'/('tcc_vfs_manifest_win64_xlc.txt' if win else 'tcc_vfs_manifest_linux_xlc.txt'))
    for f in [Path(__file__).with_name('stage_runtime_headers.py'),*(p for p in fresh.rglob('*') if p.is_file())]:inputs[str(f)]=sha(f)
    env={k:v for k,v in os.environ.items() if not k.startswith('XL6_') and k!='XLANG_PACKAGE_PATH'}
    env.update(TEMP=str(scratch),TMP=str(scratch),TMPDIR=str(scratch))
    key='PATH' if win else 'LD_LIBRARY_PATH';env[key]=str(compiler.parent)+os.pathsep+env.get(key,'')
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,
        fullCompilerBuild=True,optimization=meta['optimization'],inputs=inputs,rows=[],programs=[],
        wholeBridgeZeroOverheadClaim=False,privateHeaders=len(staged))
    def save():
        tmp=out/'results.json.tmp';tmp.write_text(json.dumps(report,indent=2)+'\n','utf-8');os.replace(tmp,out/'results.json')
    def run(name,command,accepted=True):
        command=list(map(str,command));start=time.monotonic()
        startup=None
        if win:
            startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300,startupinfo=startup)
        data=z.stdout+z.stderr;log=out/(name+'.log');log.write_bytes(data)
        passed=(z.returncode==0)==accepted
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,expectedSuccess=accepted,
            passed=passed,seconds=time.monotonic()-start,logSha256=sha(log)));save();print(name,z.returncode,flush=True)
        assert passed,log
        return data
    def check(data):assert b'FOREIGN_DATA_SNAPSHOT_OK' in data,data[-1500:]
    includes=['--inc-path',root,'--inc-path',fresh/'include','--inc-path',sdk]
    ci=['-I'+str(v) for v in (fresh/'include',root,sdk,compiler.parent)]
    try:
        generated=out/'generated.c';public=out/'public.h'
        run('emit',[compiler,'cgen',case,'--emit',generated,'--emit-header',public,*includes])
        code=generated.read_text('utf-8-sig')
        assert 'xl6PersistenceSnapshotNativeV1(' in code and 'native_snapshots_ok' in code
        assert re.search(r'native_pack_\d+\.value=native_snapshot_\d+',code)
        assert re.search(r'snapshot_store\(native_snapshot_\d+\)',code)
        report['generatedC']=dict(path=str(generated),sha256=sha(generated),directNativeCall=True,scopedSnapshots=True)
        run('ir',[compiler,'ir',case,'--lowered','--format','json','--emit',out/'lowered.json',*includes])
        ir=json.loads((out/'lowered.json').read_text('utf-8-sig'))
        assert len(ir['foreignDataSnapshots'])>=9
        report['snapshotRows']=len(ir['foreignDataSnapshots'])
        for std,tool,language in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
            run('header-'+std,[tools[tool],'-std='+std,'-Wall','-Wextra','-Werror',*ci,'-x',language,'-fsyntax-only',public])
        check(run('jit',[compiler,'run',case,*includes]))
        exe=out/('aot.exe' if win else 'aot');run('aot-build',[compiler,'build',case,'-o',exe,*includes]);check(run('aot-run',[exe]))
        data=run('aot-imports',[tools['objdump'],'-p',exe])
        imports=re.findall(rb'DLL Name:\s*(\S+)',data) if win else re.findall(rb'\bNEEDED\s+(\S+)',data)
        assert not {n.lower() for n in imports}.intersection({b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'})
        report['programs'].append(dict(path=str(exe),sha256=sha(exe),imports=[n.decode() for n in imports]))
        systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if win else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
        for opt in ('O0','O2'):
            binary=out/('gcc-'+opt+('.exe' if win else ''))
            run(opt+'-build',[tools['gcc'],'-std=c11','-'+opt,*ci,generated,helper,host,runtime,*systems,'-o',binary])
            check(run(opt+'-run',[binary]))
        single=case.read_text('utf-8').splitlines()[0]
        unused=out/'unused.xl';unused.write_text(single+'\nfunc main(...) as int\n return 0\nend\n','utf-8')
        run('unused-emit',[compiler,'cgen',unused,'--emit',out/'unused.c',*includes])
        assert 'xl6PersistenceSnapshotNativeV1(' not in (out/'unused.c').read_text('utf-8-sig')
        assert 'xlir_persist_plan_' not in (out/'unused.c').read_text('utf-8-sig')
        shared=out/'snapshot-shared.xl'
        original=case.read_text('utf-8');assert original.count('func main(...) as int')==1
        shared.write_text(original.replace('func main(...) as int','export "snapshot_check" func check() as int'),'utf-8')
        module=out/('snapshot-shared'+('.dll' if win else '.so'))
        run('shared-build',[compiler,'build',shared,helper,'--shared','-o',module,*includes])
        shared_host=out/'shared-host.py'
        shared_host.write_text('import ctypes,sys,os,_ctypes\n'
            'm=ctypes.CDLL(sys.argv[1],**({"winmode":0} if os.name=="nt" else {}))\n'
            'm.xl_script_module_init.argtypes=[ctypes.c_void_p]\nm.xl_script_module_init.restype=ctypes.c_int\n'
            'assert m.xl_script_module_init(None)==1\n'
            'm.snapshot_check.restype=ctypes.c_int64\nassert m.snapshot_check()==0\n'
            'm.xl_script_module_unit_checked.restype=ctypes.c_int\nassert m.xl_script_module_unit_checked()==1\n'
            'if os.name=="nt":\n m.xl_script_runtime_retire_checked.restype=ctypes.c_int\n assert m.xl_script_runtime_retire_checked()==1\n _ctypes.FreeLibrary(m._handle)\n'
            'else: _ctypes.dlclose(m._handle)\n'
            'print("SNAPSHOT_SHARED_RETIRED")\n','utf-8')
        data=run('shared-run',[sys.executable,'-B',shared_host,module]);check(data);assert b'SNAPSHOT_SHARED_RETIRED' in data
        data=run('shared-imports',[tools['objdump'],'-p',module])
        names=re.findall(rb'DLL Name:\s*(\S+)',data) if win else re.findall(rb'\bNEEDED\s+(\S+)',data)
        assert not {n.lower() for n in names}.intersection({b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'})
        report['programs'].append(dict(path=str(module),sha256=sha(module),imports=[n.decode() for n in names]))
        run('public-projection-regression',[compiler,'run',root/'test/cases/stdlib_template_privacy.xl',*includes])
        negatives={
            'duplicate':single.replace('@c_data_snapshot(0)','@c_data_snapshot(0) @c_data_snapshot(0)'),
            'negative':single.replace('@c_data_snapshot(0)','@c_data_snapshot(-1)'),
            'large':single.replace('@c_data_snapshot(0)','@c_data_snapshot(64)'),
            'out-of-range':single.replace('@c_data_snapshot(0)','@c_data_snapshot(1)'),
            'string':single.replace('var value','string value'),
            'integer':single.replace('var value','c_int value'),
            'pure':single.replace('@no_raise','@no_raise @pure'),
            'no-header':single.replace(' @header("test/cases/foreign_data_snapshot.h")',''),
            'overlap':single.replace('@c_data_snapshot(0)','@c_data_snapshot(0) @c_cstr(0)'),
        }
        for name,text in negatives.items():
            source=out/('bad-'+name+'.xl');source.write_text(text+'\nfunc main(...) as int\n return 0\nend\n','utf-8')
            target=source.with_suffix('.c');data=run('reject-'+name,[compiler,'cgen',source,'--emit',target,*includes],False)
            assert not target.exists() and (b'[sem]' in data or b'[ir]' in data)
        report.update(complete=True,inputsUnchanged=all(sha(path)==h for path,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:
        report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
