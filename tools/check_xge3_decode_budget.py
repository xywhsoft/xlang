"""CPU DecodeBudget direct declarations with real shared native quota owners.

Full-compiler JIT and independent dynamic/static case/example programs check
limits, live/rejected charges, independent share, active close and 64 managed
publication OOM budgets. Info and setLimit use 2000 calls per language/C path
under a zero-allocation budget. Backing allocator failure, model-loader
integration, GPU, language thread transfer and package DLL initialization are
not certified by this gate.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in('root','compiler','compiler-report','native-root','dynamic-packages','static-packages','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args()
    root,compiler,build,native,dynamic,static,out=[p.resolve()for p in
        (a.root,a.compiler,a.compiler_report,a.native_root,a.dynamic_packages,a.static_packages,a.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt'
    tools={n:Path(shutil.which(n)).resolve()for n in('gcc','g++','objdump')}
    case=root/'test/cases/foreign_xge3_decode_budget.xl';example=root/'lib/bindings/xge/three_d/examples/decode_budget.xl'
    files=[Path(__file__),compiler,build,case,case.with_suffix('.c'),case.with_suffix('.h'),example,
        *extra,*tools.values(),Path(__file__).with_name('foreign_compiler_cohort.py'),
        Path(__file__).with_name('full_compiler_qualification.py'),native/'include/xge.h',
        native/'src/xge/3d/xge3d_decode_budget.c',native/'config/modules.json']
    files.extend(compiler.parent/n for n in(('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    for folder in(sdk,dynamic,static):files.extend(p for p in folder.rglob('*')if p.is_file())
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    inputs={str(p):sha(p)for p in files};out.mkdir(parents=True,exist_ok=False)
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,
        networkTests=False,inputs=inputs,rows=[],programs=[],heap=[])
    def save():
        p=out/'results.json.tmp';p.write_text(json.dumps(report,indent=2)+'\n');os.replace(p,out/'results.json')
    def run(name,command,packages):
        env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
        env['XLANG_PACKAGE_PATH']=str(packages);key='PATH'if win else'LD_LIBRARY_PATH'
        env[key]=str(packages/'_xge_native/native')+os.pathsep+env.get(key,'')
        command=list(map(str,command));start=time.monotonic()
        z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,
            seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert not z.returncode,log
        return z.stdout+z.stderr
    includes=['--inc-path',root,'--inc-path',sdk,'--inc-path',native]
    c_includes=['-I'+str(p)for p in(root,sdk,native,native/'include',*sorted(dynamic.glob('*/include')))]
    try:
        for source in(case,example):
            name=source.stem;c=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,'--emit',c,'--emit-header',header,*includes],dynamic)
            text=c.read_text(encoding='utf-8-sig')
            for symbol in('xge3dDecodeBudgetCreate','xge3dDecodeBudgetShareOwned','xge3dDecodeBudgetInfo','xge3dDecodeBudgetSetLimit'):
                assert re.search(r'\b'+symbol+r'\(',text),symbol
            if source==case:
                for level in('O0','O2'):
                    run('provider-'+level,[tools['gcc'],'-std=c11','-'+level,'-Wall','-Wextra','-Werror',*c_includes,'-fsyntax-only',case.with_suffix('.c')],dynamic)
            for standard,tool,language in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run(name+'-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*c_includes,'-x',language,'-fsyntax-only',header],dynamic)
            for kind,packages in(('jit',dynamic),('dynamic',dynamic),('static',static)):
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
                if source==case:
                    assert b'independent ownership, live charges, active close, limits and 64 publication OOM budgets passed'in output
                    counts=re.findall(rb'DecodeBudget heap (language|native): alloc=(\d+) free=(\d+) realloc=(\d+)',output)
                    assert len(counts)==2 and {row[0]for row in counts}=={b'language',b'native'}
                    metrics={row[0].decode():dict(zip(('alloc','free','realloc'),map(int,row[1:])))for row in counts}
                    assert all(value==0 for row in metrics.values()for value in row.values())
                    report['heap'].append(dict(mode=kind,calls=2000,counts=metrics,zeroAllocationBudget=True));save()
                else:assert b'2097152'in output
        report.update(complete=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
