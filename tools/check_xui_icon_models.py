"""XUI registry-owned Icon/Category views and typed IconPicker model edges.

An explicit full compiler, matched native SDK and CPU test Proxy are required.
Checks expiry/aliases/Context retention/cross-Context rejection and 96 factory
OOM budgets in JIT and independent dynamic/static programs. The graphical
learning example is emitted and its public header checked, not executed here.
Retained custom callbacks and every same-object reentry path are separate work.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','compiler-report','native-root','dynamic-packages','static-packages','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    root,compiler,build,native,dynamic,static,out=[p.resolve()for p in
        (args.root,args.compiler,args.compiler_report,args.native_root,args.dynamic_packages,args.static_packages,args.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt'
    tools={name:Path(shutil.which(name)).resolve()for name in ('gcc','g++','objdump')}
    case=root/'test/cases/foreign_xui_icon_models.xl';example=root/'lib/bindings/xui/examples/icon_picker.xl'
    files=[compiler,build,Path(__file__),case,example,example.with_name('_window.xl'),*extra,*tools.values(),
        Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py'),
        case.with_suffix('.h'),case.with_suffix('.c'),native/'tests/xui/xui_test_proxy.c',native/'tests/xui/xui_test_proxy.h']
    files.extend(compiler.parent/n for n in (('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    for directory in (sdk,dynamic,static):files.extend(p for p in directory.rglob('*')if p.is_file())
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    inputs={str(p):sha(p)for p in files};out.mkdir(parents=True,exist_ok=False)
    report=dict(complete=False,passed=False,scope=__doc__,fullCompilerBuild=True,shippingDelivery=False,networkTests=False,inputs=inputs,rows=[],programs=[])
    def save():
        temporary=out/'results.json.tmp';temporary.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');os.replace(temporary,out/'results.json')
    def run(name,command,packages):
        env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
        env['XLANG_PACKAGE_PATH']=str(packages);key='PATH'if win else'LD_LIBRARY_PATH'
        env[key]=str(packages/'_xge_native/native')+os.pathsep+env.get(key,'')
        command=list(map(str,command));start=time.monotonic()
        result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=result.returncode,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,result.returncode,flush=True);assert not result.returncode,log;return result.stdout+result.stderr
    try:
        includes=['--inc-path',root,'--inc-path',sdk,'--inc-path',native]
        for source in (case,example):
            name=source.stem;c=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,'--emit',c,'--emit-header',header,*includes],dynamic)
            text=c.read_text(encoding='utf-8-sig')
            for symbol in ('xuiIconCategoryCreate','xuiIconAddSvgPath','xuiIconPickerCreate','xuiIconPickerSetCategory'):
                assert re.search(r'\b'+symbol+r'\(',text),symbol
            if source==case:
                for kind,packages in (('jit',dynamic),('dynamic',dynamic),('static',static)):
                    if kind=='jit':output=run(name+'-'+kind,[compiler,'run',source,*includes],packages)
                    else:
                        program=out/(name+'-'+kind+('.exe'if win else''))
                        run(name+'-'+kind+'-build',[compiler,'build',source,'-o',program,*includes],packages)
                        output=run(name+'-'+kind+'-execute',[program],packages)
                        table=run(name+'-'+kind+'-imports',[tools['objdump'],'-p',program],packages)
                        imports=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table)
                        lower={n.lower()for n in imports}
                        assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                        assert any(b'xge3_host'in n for n in lower)==(kind=='dynamic')
                        report['programs'].append(dict(path=str(program),sha256=sha(program),bytes=program.stat().st_size,imports=[n.decode()for n in imports]))
                    assert b'Icon models: registry views, picker ownership, expiry and 96 OOM budgets passed'in output
            native_includes=['-I'+str(p)for p in(root,sdk,sdk/'include',*sorted(dynamic.glob('*/include')))]
            for standard,tool,language in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run(name+'-public-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*native_includes,'-x',language,'-fsyntax-only',header],dynamic)
        report.update(complete=True,passed=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()));assert report['inputsUnchanged']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
