"""CPU-only 3D matrix/camera constructors against the same native SDK.

Real JIT, dynamic/static programs, C++ native value ABI and equal-workload XRT
heap counters. Does not certify GPU rendering, shared-library initialization,
whole-3D resource ownership, throughput or elimination of stack value copies.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','compiler','compiler-report','dynamic-packages','static-packages','out'):
        p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();r,cc,cohort,dynamic,static,out=[v.resolve()for v in(a.root,a.compiler,a.compiler_report,a.dynamic_packages,a.static_packages,a.out)]
    sdk,extra=load_compiler_cohort(r,cc,cohort);out.mkdir(parents=True,exist_ok=False)
    win=os.name=='nt';sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    tools={n:Path(shutil.which(n)).resolve()for n in('gcc','g++','ar','objdump')}
    case=r/'test/cases/foreign_xge3_camera_values.xl';example=r/'lib/bindings/xge/three_d/examples/camera_values.xl';cpp=case.with_name(case.stem+'_oracle.cpp')
    files=[cc,cohort,Path(__file__).resolve(),Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py'),*extra,*tools.values(),case,example,cpp,case.with_suffix('.c'),case.with_suffix('.h')]
    files += [cc.parent/n for n in(('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so'))]
    for folder in(sdk,dynamic,static):files.extend(v for v in folder.rglob('*')if v.is_file())
    inputs={str(v):sha(v)for v in files};report=dict(complete=False,passed=False,scope=__doc__,fullCompilerBuild=True,shippingDelivery=False,sharedLibraryPackageQualified=False,cpuPerformanceClaim=False,inputs=inputs,rows=[])
    save=lambda:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,cmd,packages=None):
        cmd=list(map(str,cmd));env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
        if packages:
            env['XLANG_PACKAGE_PATH']=str(packages)
            variable='PATH'if win else'LD_LIBRARY_PATH';env[variable]=str(packages/'_xge_native/native')+os.pathsep+env.get(variable,'')
        start=time.monotonic();z=subprocess.run(cmd,cwd=cc.parent,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=cmd,packages=str(packages)if packages else None,exitCode=z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)));save();print(name,z.returncode,flush=True)
        assert not z.returncode,log;return z.stdout+z.stderr
    includes=['--inc-path',r,'--inc-path',sdk];native_includes=['-I'+str(v)for v in dynamic.glob('*/include')if v.is_dir()]
    try:
        oracle=out/'oracle.o';archive=out/'oracle.a'
        run('cpp17-native',[tools['g++'],'-std=c++17','-O2','-DXL6_XGE_STATIC=1',*([]if win else['-fPIC']),'-ffunction-sections','-fdata-sections','-fno-exceptions','-fno-rtti','-I'+str(r),'-I'+str(sdk),*native_includes,'-c',cpp,'-o',oracle])
        run('cpp17-archive',[tools['ar'],'rcs',archive,oracle])
        for source in(case,example):
            name=source.stem;generated=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[cc,'cgen',source,'--emit',generated,'--emit-header',header,*includes],dynamic)
            for kind,packages in(('jit',dynamic),('dynamic',dynamic),('static',static)):
                links=['--link',archive]if source==case else[]
                if kind=='jit':cmd=[cc,'run',source,*includes,*links]
                else:
                    program=out/(name+'-'+kind+('.exe'if win else''))
                    run(name+'-'+kind+'-build',[cc,'build',source,'-o',program,*includes,*links],packages);cmd=[program]
                text=run(name+'-'+kind,cmd,packages)
                if source==case:
                    assert b'Native 3D values: zero XRT heap operations'in text and b'Language 3D values: zero XRT heap operations'in text
                    assert b'cameras and checked failures passed'in text
                else:assert b'3D matrix and camera values passed'in text
                if kind!='jit':
                    table=run(name+'-'+kind+'-imports',[tools['objdump'],'-p',program]);names=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table);lowered={v.lower()for v in names}
                    assert not{b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lowered)
                    assert any(b'xge3_host'in v for v in lowered)==(kind=='dynamic')
                    report.setdefault('programs',[]).append(dict(path=str(program),sha256=sha(program),bytes=program.stat().st_size,imports=[v.decode()for v in names]))
            if source==case:
                text=generated.read_text(encoding='utf-8');assert all(re.search(r'\b'+n+r'\(',text)for n in('xge3dTransformMatrix','xge3dCameraPerspective','xge3dCameraOrthographic'))
                for standard,tool,language in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                    run('public-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror','-I'+str(r),'-I'+str(sdk),*native_includes,'-x',language,'-fsyntax-only',header])
        report.update(complete=True,passed=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()));assert report['inputsUnchanged']
    except Exception as e:report.update(complete=True,passed=False,error=str(e));raise
    finally:save()

if __name__=='__main__':main()
