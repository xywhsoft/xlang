"""Complete-compiler CPU CString/typed-NULL acceptance.

Direct, bound, protocol and function-field calls, strict public headers,
JIT and standalone AOT with no compiler/runtime DLL imports.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in('root','compiler','compiler-report','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args()
    root,compiler,build,out=[p.resolve()for p in(a.root,a.compiler,a.compiler_report,a.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt'
    tools={n:Path(shutil.which(n)).resolve()for n in('gcc','g++','objdump')}
    case=root/'test/cases/foreign_null_cstr.xl';native=case.with_name(case.stem+'_native.c')
    files=[Path(__file__),compiler,build,case,native,case.with_suffix('.h'),
        *extra,*tools.values(),Path(__file__).with_name('foreign_compiler_cohort.py'),
        Path(__file__).with_name('full_compiler_qualification.py')]
    files.extend(compiler.parent/n for n in(('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    for folder in(sdk,):files.extend(p for p in folder.rglob('*')if p.is_file())
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    inputs={str(p):sha(p)for p in files};out.mkdir(parents=True,exist_ok=False)
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,
        networkTests=False,inputs=inputs,rows=[],programs=[],heap=[])
    def save():
        p=out/'results.json.tmp';p.write_text(json.dumps(report,indent=2)+'\n');os.replace(p,out/'results.json')
    def run(name,command,packages):
        env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
        key='PATH'if win else'LD_LIBRARY_PATH'
        env[key]=str(compiler.parent)+os.pathsep+env.get(key,'')
        command=list(map(str,command));start=time.monotonic()
        z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,
            seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert not z.returncode,log
        return z.stdout+z.stderr
    includes=['--inc-path',root,'--inc-path',sdk]
    c_includes=['-I'+str(p)for p in(root,sdk)]
    try:
        for source in(case,):
            name=source.stem;c=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,'--emit',c,'--emit-header',header,*includes],None)
            text=c.read_text(encoding='utf-8-sig')
            assert re.search(r'\bcstrFree\(',text)
            if source==case:
                for level in('O0','O2'):
                    run('provider-'+level,[tools['gcc'],'-std=c11','-'+level,'-Wall','-Wextra','-Werror',*c_includes,'-fsyntax-only',native],None)
            for standard,tool,language in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run(name+'-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*c_includes,'-x',language,'-fsyntax-only',header],None)
            for kind,packages in(('jit',None),('standalone',None)):
                if kind=='jit':output=run(name+'-'+kind,[compiler,'run',source,native,*includes],packages)
                else:
                    program=out/(name+'-'+kind+('.exe'if win else''))
                    run(name+'-'+kind+'-build',[compiler,'build',source,native,'-o',program,*includes],packages)
                    output=run(name+'-'+kind+'-execute',[program],packages)
                    table=run(name+'-'+kind+'-imports',[tools['objdump'],'-p',program],packages)
                    imports=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table)
                    lower={n.lower()for n in imports}
                    assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                    assert not any(b'xge'in n for n in lower)
                    report['programs'].append(dict(path=str(program),sha256=sha(program),bytes=program.stat().st_size,imports=[n.decode()for n in imports]))
                assert b'NULL CString: direct, bound, protocol and native function field calls passed'in output
        report.update(complete=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
