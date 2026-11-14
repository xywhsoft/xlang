"""Qualify XGE error field snapshots and direct packed error reporting.

Existing generic CString fields and record packing; independent snapshots,
NULL/empty, epoch expiry, OOM assignment preservation and self-removal.
C/C++ headers, JIT/dynamic/static and an engine-free console example.
No network, graphics window, native rebuild or shipping installation.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','compiler','compiler-report','native-root','engine','dynamic-packages','static-packages','out'):
        p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();root,compiler,build,native,engine,dynamic,static,out=[v.resolve()for v in
        (a.root,a.compiler,a.compiler_report,a.native_root,a.engine,a.dynamic_packages,a.static_packages,a.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt'
    tools={n:Path(shutil.which(n)).resolve()for n in ('gcc','g++','objdump')}
    names=('foreign_xge_error_fields','error_callback','foreign_xge_error_fields_alias')
    cases=[root/'test/cases/foreign_xge_error_fields.xl',root/'lib/bindings/xge/examples/error_callback.xl',root/'test/cases/foreign_xge_error_fields_alias.xl']
    metadata=json.loads(build.read_text(encoding='utf-8'))
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    files=[Path(__file__).resolve(),compiler,build,Path(metadata['sourceFreeze']),*extra,*tools.values(),*cases,
        root/'test/cases/foreign_xge_error_fields.h',root/'test/cases/foreign_xge_error_fields.c',
        native/'src/xge/core/xge_impl.c',native/'src/xge/core/xge_callback_owned.h',
        Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py')]
    files.extend(compiler.parent/n for n in (('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    for directory in (sdk,dynamic,static):files.extend(f for f in directory.rglob('*')if f.is_file())
    inputs={str(f):sha(f)for f in files};out.mkdir(parents=True,exist_ok=False)
    snapshots=out/'input-snapshots';snapshots.mkdir()
    for f in (*cases,root/'test/cases/foreign_xge_error_fields.h',root/'test/cases/foreign_xge_error_fields.c',Path(__file__).resolve()):
        shutil.copy2(f,snapshots/f.name)
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,
        compilerOptimization=metadata['optimization'],networkTests=False,nativeRebuilt=False,inputs=inputs,rows=[],programs=[],heap=[],generatedC=[])
    def save():
        temp=out/'results.json.tmp';temp.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');os.replace(temp,out/'results.json')
    def run(name,command,packages):
        env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
        env['XLANG_PACKAGE_PATH']=str(packages);key='PATH'if win else'LD_LIBRARY_PATH'
        env[key]=os.pathsep.join((str(packages/'_xge_native/native'),str(engine),str(compiler.parent),env.get(key,'')))
        startup=None
        if win:
            startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        def no_core():
            import resource
            resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        command=list(map(str,command));start=time.monotonic()
        z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300,startupinfo=startup,preexec_fn=None if win else no_core)
        data=z.stdout+z.stderr;log=out/(name+'.log');log.write_bytes(data)
        passed=z.returncode==0
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,passed=passed,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert passed,log;return data
    includes=['--inc-path',root,'--inc-path',sdk,'--inc-path',native]
    ci=['-I'+str(v)for v in (root,sdk,native,native/'include',*sorted(dynamic.glob('*/include')))]
    expected={names[0]:b'XgeErrorFields:',names[1]:b'Resource unavailable',names[2]:b'XgeErrorFieldsAlias:'}
    try:
        for source in cases:
            name=source.stem;c=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,'--emit',c,'--emit-header',header,*includes],dynamic)
            text=c.read_text(encoding='utf-8-sig')
            assert re.search(r'(?<![\w&])xgeReportError\(',text)
            if source!=cases[1]:
                for field in ('sSubsystem','sOperation','sMessage'):assert '->'+field in text,field
                assert 'cstring_size' in text
            report['generatedC'].append(dict(path=str(c),sha256=sha(c),directReportCall=True,pointerFieldCopies=source==cases[0]))
            for standard,tool,language in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run(name+'-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*ci,'-x',language,'-fsyntax-only',header],dynamic)
            for kind,packages in (('jit',dynamic),('dynamic',dynamic),('static',static)):
                if kind=='jit':output=run(name+'-'+kind,[compiler,'run',source,*includes],packages)
                else:
                    binary=out/(name+'-'+kind+('.exe'if win else''))
                    run(name+'-'+kind+'-build',[compiler,'build',source,'-o',binary,*includes],packages)
                    output=run(name+'-'+kind+'-execute',[binary],packages)
                    table=run(name+'-'+kind+'-imports',[tools['objdump'],'-p',binary],packages)
                    imports=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table);lower={v.lower()for v in imports}
                    assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                    if kind=='static':assert not any(b'xge3_host'in v for v in lower)
                    else:assert any(b'xge3_host'in v for v in lower)
                    report['programs'].append(dict(path=str(binary),sha256=sha(binary),bytes=binary.stat().st_size,imports=[v.decode()for v in imports]))
                assert expected[name]in output
                if source==cases[0]:
                    counts=re.findall(rb'Error fields heap (language|native): alloc=(\d+) free=(\d+) realloc=(\d+)',output)
                    assert counts==[(b'language',b'3000',b'3000',b'0'),(b'native',b'3000',b'3000',b'0')],counts
                    report['heap'].append(dict(mode=kind,rounds=1000,counts=[(v[0].decode(),list(map(int,v[1:])))for v in counts],cpuEquivalenceClaim=False))
                save()
        assert len(report['rows'])==33 and len(report['heap'])==3
        report.update(complete=True,inputsUnchanged=all(sha(f)==h for f,h in inputs.items()));report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
