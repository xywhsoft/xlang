"""Qualify six direct value records, native array extents and C/C++ value ABI.

Uses a complete O2 compiler with unchanged matched host/static SDKs. Tests
independent copies, terminal array slots, checked bounds/failed output, two
C++17 O0/O2 archives, C/C++ headers and readable lessons in JIT/dynamic/static.
The heap comparison covers 1000 warm FindResult output calls only; no CPU
equivalence, UI, device, clipboard, network or shipping acceptance is claimed.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in('root','compiler','compiler-report','dynamic-packages','static-packages','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,compiler,build,dynamic,static,out=[v.resolve()for v in(a.root,a.compiler,a.compiler_report,a.dynamic_packages,a.static_packages,a.out)]
    assert not out.is_relative_to(root)
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt';sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    tools={n:Path(shutil.which(n)).resolve()for n in('gcc','g++','ar','objdump')}
    case=root/'test/cases/foreign_xge3_replay_values.xl';cpp=case.with_name(case.stem+'_oracle.cpp')
    examples=[root/'lib/bindings'/rel for rel in('xge/examples/touch_value.xl','xui/examples/search_value.xl')]
    files=[Path(__file__).resolve(),compiler,build,*extra,*tools.values(),case,case.with_suffix('.c'),case.with_suffix('.h'),cpp,*examples,*[Path(__file__).with_name(n+'.py')for n in('foreign_compiler_cohort','full_compiler_qualification')]]
    files.extend(compiler.parent/n for n in(('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    for folder in(sdk,dynamic,static):files.extend(f for f in folder.rglob('*')if f.is_file())
    inputs={str(f):sha(f)for f in files};out.mkdir(parents=True,exist_ok=False);scratch=out/'tmp';scratch.mkdir();snap=out/'input-snapshots';snap.mkdir()
    for f in(case,case.with_suffix('.c'),case.with_suffix('.h'),cpp,*examples,Path(__file__).resolve()):shutil.copy2(f,snap/f.name)
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,networkTests=False,nativeRebuilt=False,cpuEquivalenceClaim=False,inputs=inputs,rows=[],programs=[])
    def save():
        tmp=out/'results.json.tmp';tmp.write_text(json.dumps(report,indent=2)+'\n');os.replace(tmp,out/'results.json')
    def run(name,command,packages=dynamic):
        env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
        env.update(XLANG_PACKAGE_PATH=str(packages),TEMP=str(scratch),TMP=str(scratch),TMPDIR=str(scratch));key='PATH'if win else'LD_LIBRARY_PATH'
        env[key]=os.pathsep.join((str(packages/'_xge_native/native'),str(compiler.parent),env.get(key,'')))
        command=list(map(str,command));start=time.monotonic();z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
        data=z.stdout+z.stderr;log=out/(name+'.log');log.write_bytes(data);report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,passed=z.returncode==0,seconds=time.monotonic()-start,logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert not z.returncode,log;return data
    includes=['--inc-path',root,'--inc-path',sdk];ci=['-I'+str(v)for v in(root,sdk,*sorted(dynamic.glob('*/include')))]
    def program(source,label,links=(),case_run=False):
        for kind,packages in(('jit',dynamic),('dynamic',dynamic),('static',static)):
            if kind=='jit':raw=run(label+'-jit',[compiler,'run',source,*includes,*links],packages)
            else:
                binary=out/(label+'-'+kind+('.exe'if win else''));run(label+'-'+kind+'-build',[compiler,'build',source,'-o',binary,*includes,*links],packages);raw=run(label+'-'+kind+'-run',[binary],packages)
                table=run(label+'-'+kind+'-imports',[tools['objdump'],'-p',binary],packages)
                names=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table);lower={n.lower()for n in names}
                assert not{b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                assert any(b'xge3_host'in n for n in lower)==(kind=='dynamic')
                report['programs'].append(dict(path=str(binary),sha256=sha(binary),imports=[n.decode()for n in names]))
            if case_run:
                assert b'Six native value records:'in raw
                for implementation in(b'language',b'native'):assert b'Replay value heap '+implementation+b': alloc=0 free=0 realloc=0'in raw
            else:assert b'Original'in raw and b'Copied'in raw
    try:
        generated=out/'values.c';header=out/'values.h';run('emit',[compiler,'cgen',case,'--emit',generated,'--emit-header',header,*includes])
        for standard,tool,language in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
            run('header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*ci,'-x',language,'-fsyntax-only',header])
        for opt in('O0','O2'):
            obj=out/('oracle-'+opt+'.o');archive=out/('oracle-'+opt+'.a')
            run('cpp-'+opt,[tools['g++'],'-std=c++17','-'+opt,'-DXL6_XGE_STATIC=1',*([]if win else['-fPIC']),'-ffunction-sections','-fdata-sections','-fno-exceptions','-fno-rtti',*ci,'-c',cpp,'-o',obj]);run('archive-'+opt,[tools['ar'],'rcs',archive,obj]);program(case,'values-'+opt,('--link',archive),True)
        for example in examples:program(example,example.stem)
        report.update(complete=True,passed=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()));assert report['inputsUnchanged'];save()
    except BaseException as error:report.update(error=repr(error));save();raise


if __name__=='__main__':main()
