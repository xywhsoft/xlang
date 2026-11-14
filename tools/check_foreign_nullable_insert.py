"""Qualify contextual null insertion with a complete O2 compiler cohort.

Literal/conditional null for Array and Set; real owned CPU resources, returned
views, expiry and calibrated append failures. C/C++ headers, JIT/dynamic/static,
an existing nullable-container matrix, nested compilation and negative admission.
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
    names=('foreign_nullable_array_append','foreign_nullable_set_insert','foreign_nullable_array_owned','typed_nullable_container_matrix','foreign_nullable_insert_nested')
    cases=[root/'test/cases'/f'{n}.xl'for n in names]
    negatives=[root/'test/cases'/f'bad_nullable_append_{n}.xl'for n in ('nonnullable_number','nonnullable_view','wrong_view')]
    metadata=json.loads(build.read_text(encoding='utf-8'))
    snapshot=Path(metadata['source']);freeze=json.loads(Path(metadata['sourceFreeze']).read_text(encoding='utf-8'))
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    core=[root/'src/ir'/n for n in ('xl6_ir.c','xl6_ir_lower.c')]
    for f in core:
        relative=f.relative_to(root).as_posix()
        assert sha(f)==freeze['files'][relative]==sha(snapshot/relative),f
    files=[Path(__file__).resolve(),compiler,build,Path(metadata['sourceFreeze']),*extra,*core,*tools.values(),*cases,*negatives,
        root/'test/cases/foreign_nullable_array_owned.h',root/'test/cases/foreign_nullable_array_owned.c',
        root/'test/cases/foreign_xui_render_backend.c',root/'test/cases/foreign_xui_render_backend.h',root/'test/cases/foreign_xui_render_adapters.inc',
        native/'tests/xui/xui_test_proxy.c',native/'tests/xui/xui_test_proxy.h',engine/'build-report.json',
        Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py')]
    files.extend(compiler.parent/n for n in (('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    for directory in (sdk,dynamic,static):files.extend(f for f in directory.rglob('*')if f.is_file())
    inputs={str(f):sha(f)for f in files};out.mkdir(parents=True,exist_ok=False)
    snapshots=out/'input-snapshots';snapshots.mkdir()
    for f in (*cases,*negatives,root/'test/cases/foreign_nullable_array_owned.h',root/'test/cases/foreign_nullable_array_owned.c',Path(__file__).resolve()):
        shutil.copy2(f,snapshots/f.name)
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,
        compilerOptimization=metadata['optimization'],networkTests=False,nativeRebuilt=False,inputs=inputs,rows=[],programs=[],calibration=[],generatedC=[])
    def save():
        temp=out/'results.json.tmp';temp.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');os.replace(temp,out/'results.json')
    def run(name,command,packages,negative=False):
        env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
        env['XLANG_PACKAGE_PATH']=str(packages);key='PATH'if win else'LD_LIBRARY_PATH'
        env[key]=os.pathsep.join((str(packages/'_xge_native/native'),str(engine),str(compiler.parent),env.get(key,'')))
        if name.startswith('foreign_nullable_insert_nested'):
            work=out/(name+'-work');work.mkdir(exist_ok=True)
            env['XLANG_NULLABLE_NESTED_WORK']=str(work)
        startup=None
        if win:
            startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        def no_core():
            import resource
            resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        command=list(map(str,command));start=time.monotonic()
        z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300,startupinfo=startup,preexec_fn=None if win else no_core)
        data=z.stdout+z.stderr;log=out/(name+'.log');log.write_bytes(data)
        passed=(z.returncode!=0 and b'[sem]'in data and b'array push/append value type mismatch'in data)if negative else z.returncode==0
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,expectedFailure=negative,passed=passed,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert passed,log;return data
    includes=['--inc-path',root,'--inc-path',sdk,'--inc-path',native]
    ci=['-I'+str(v)for v in (root,sdk,native,native/'include',*sorted(dynamic.glob('*/include')))]
    expected={names[0]:b'NullableArrayAppend:',names[1]:b'NullableSetInsert:',names[2]:b'NullableArrayOwned:',names[3]:b'typed nullable container matrix ok',names[4]:b'NullableInsertNested:'}
    try:
        for source in cases:
            name=source.stem;c=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,'--emit',c,'--emit-header',header,*includes],dynamic)
            text=c.read_text(encoding='utf-8-sig');symbol='xl6SetValueAddTakeV1'if name==names[1]else'xl6ArrayValueAppendTakeV1'
            if name not in (names[3],names[4]):assert re.search(r'\b'+symbol+r'\(',text),symbol
            report['generatedC'].append(dict(path=str(c),sha256=sha(c),contextualInsertion=True))
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
                    elif name==names[2]:assert any(b'xge3_host'in v for v in lower)
                    report['programs'].append(dict(path=str(binary),sha256=sha(binary),bytes=binary.stat().st_size,imports=[v.decode()for v in imports]))
                assert expected[name]in output
                if name==names[2]:
                    counts=re.findall(rb'Nullable array prefixes=(\d+)',output);assert len(counts)==1
                    count=int(counts[0]);assert 8<count<8192
                    report['calibration'].append(dict(mode=kind,prefixes=count,individualAppendAtomic=True,completeRetirementHeapChecked=True))
                save()
        for source in negatives:
            run(source.stem,[compiler,'cgen',source,'--emit',out/(source.stem+'.c'),*includes],dynamic,negative=True)
        assert len(report['rows'])==58 and len(report['calibration'])==3
        report.update(complete=True,inputsUnchanged=all(sha(f)==h for f,h in inputs.items()));report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
