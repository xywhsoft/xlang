"""Full-compiler checked Run allocation failure and real-window recovery.

Public Run's zero-allocation budget may fail code-lease admission before Sokol.
Native C identifies the exact pre-window buffer stages separately. No remote
services, network or security testing. Linux graphics may use local Xvfb.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in('root','compiler','compiler-report','native-root','dynamic-packages','static-packages','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,compiler,build,native,dynamic,static,out=[v.resolve()for v in
        (a.root,a.compiler,a.compiler_report,a.native_root,a.dynamic_packages,a.static_packages,a.out)]
    win=os.name=='nt';sdk,extra=load_compiler_cohort(root,compiler,build)
    tools={n:Path(shutil.which(n)).resolve()for n in('gcc','g++','objdump')}
    case=root/'test/cases/foreign_xge3_state_oom.xl';sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest()
    files=[Path(__file__).resolve(),compiler,build,case,case.with_suffix('.c'),case.with_suffix('.h'),*extra,*tools.values(),
        Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py'),
        native/'extlibs/sokol/sokol_app.h',native/'src/xge/core/xge_impl.c']
    files.extend(compiler.parent/n for n in(('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    for folder in(sdk,dynamic,static):files.extend(v for v in folder.rglob('*')if v.is_file())
    inputs={str(v):sha(v)for v in files};out.mkdir(parents=True,exist_ok=False)
    for source in[Path(__file__).resolve(),case,case.with_suffix('.c'),case.with_suffix('.h')]:shutil.copy2(source,out/source.name)
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,
        networkTests=False,windowTests=True,inputs=inputs,rows=[],programs=[])
    def save():
        target=out/'results.json.tmp';target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');os.replace(target,out/'results.json')
    def run(name,command,packages):
        env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
        env['XLANG_PACKAGE_PATH']=str(packages);key='PATH'if win else'LD_LIBRARY_PATH'
        env[key]=str(packages/'_xge_native/native')+os.pathsep+env.get(key,'')
        command=list(map(str,command));start=time.monotonic()
        z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300,
            **({'creationflags':subprocess.CREATE_NO_WINDOW}if win else{}))
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,
            seconds=time.monotonic()-start,logSha256=sha(log)));save();print(name,z.returncode,flush=True)
        assert not z.returncode,log
        return z.stdout+z.stderr
    includes=['--inc-path',root,'--inc-path',sdk,'--inc-path',native]
    c_includes=['-I'+str(v)for v in(root,sdk,native,native/'include',*sorted(dynamic.glob('*/include')))]
    try:
        c=out/'generated.c';header=out/'public.h'
        run('emit',[compiler,'cgen',case,'--emit',c,'--emit-header',header,*includes],dynamic)
        assert re.search(r'\bxl6Xge3RunOwned\(',c.read_text(encoding='utf-8-sig'))
        for level in('O0','O2'):
            run('provider-'+level,[tools['gcc'],'-std=c11','-'+level,'-Wall','-Wextra','-Werror',*c_includes,'-fsyntax-only',case.with_suffix('.c')],dynamic)
        for standard,tool,language in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
            run('header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*c_includes,'-x',language,'-fsyntax-only',header],dynamic)
        for kind,packages in(('jit',dynamic),('dynamic',dynamic),('static',static)):
            if kind=='jit':output=run(kind,[compiler,'run',case,*includes],packages)
            else:
                binary=out/(kind+('.exe'if win else''))
                run(kind+'-build',[compiler,'build',case,'-o',binary,*includes],packages)
                output=run(kind+'-execute',[binary],packages)
                table=run(kind+'-imports',[tools['objdump'],'-p',binary],packages)
                imports=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table)
                lower={v.lower()for v in imports}
                assert not{b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                assert any(b'xge3_host'in v for v in lower)==(kind=='dynamic')
                report['programs'].append(dict(path=str(binary),sha256=sha(binary),bytes=binary.stat().st_size,imports=[v.decode()for v in imports]))
            assert b'Startup OOM: 12 checked failures, 3 real recoveries, reinitialize and manual mode passed'in output
        report.update(complete=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as e:report.update(complete=True,passed=False,error=str(e));raise
    finally:save()


if __name__=='__main__':main()
