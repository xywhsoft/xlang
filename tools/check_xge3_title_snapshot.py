"""Copied creator-thread title snapshots using a matched full compiler and SDK.

Checks NULL versus empty, UTF-8, retained snapshots after SetTitle/Unit, real
JIT, independent dynamic/static programs, examples and three public headers.
Does not certify live platform-window updates or package shared initialization.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','compiler-report','dynamic-packages','static-packages','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    root,compiler,build,dynamic,static,out=[v.resolve() for v in
        (args.root,args.compiler,args.compiler_report,args.dynamic_packages,args.static_packages,args.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build)
    out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    win=os.name=='nt'
    tools={name:Path(shutil.which(name)).resolve() for name in ('gcc','g++','objdump')}
    case=root/'test/cases/foreign_xge3_title_snapshot.xl'
    example=root/'lib/bindings/xge/examples/window_title.xl'
    files=[compiler,build,Path(__file__),case,example,*extra,*tools.values(),
        Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py')]
    files += [compiler.parent/name for name in (('xl.dll','xlc.dll') if win else ('libxl.so','libxlc.so'))]
    for folder in (sdk,dynamic,static):
        files.extend(p for p in folder.rglob('*') if p.is_file())
    inputs={str(p):sha(p) for p in files}
    report=dict(complete=False,passed=False,scope=__doc__,fullCompilerBuild=True,
        shippingDelivery=False,inputs=inputs,rows=[],programs=[])
    def save():
        temporary=out/'results.json.tmp'
        temporary.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        os.replace(temporary,out/'results.json')
    def run(name,command,packages):
        env={k:v for k,v in os.environ.items() if not k.startswith('XL6_') and k!='XLANG_PACKAGE_PATH'}
        env['XLANG_PACKAGE_PATH']=str(packages)
        key='PATH' if win else 'LD_LIBRARY_PATH'
        env[key]=str(packages/'_xge_native/native')+os.pathsep+env.get(key,'')
        command=list(map(str,command));started=time.monotonic()
        completed=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
        log=out/(name+'.log');log.write_bytes(completed.stdout+completed.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=completed.returncode,
            seconds=time.monotonic()-started,logSha256=sha(log)))
        save();print(name,completed.returncode,flush=True)
        assert not completed.returncode,log
        return completed.stdout+completed.stderr
    try:
        includes=['--inc-path',root,'--inc-path',sdk]
        for source in (case,example):
            name=source.stem;generated=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,'--emit',generated,'--emit-header',header,*includes],dynamic)
            text=generated.read_text(encoding='utf-8-sig')
            assert re.search(r'\bxgeGetTitle\(',text)
            for kind,packages in (('jit',dynamic),('dynamic',dynamic),('static',static)):
                if kind=='jit':
                    output=run(name+'-'+kind,[compiler,'run',source,*includes],packages)
                else:
                    program=out/(name+'-'+kind+('.exe' if win else ''))
                    run(name+'-'+kind+'-build',[compiler,'build',source,'-o',program,*includes],packages)
                    output=run(name+'-'+kind+'-execute',[program],packages)
                    table=run(name+'-'+kind+'-imports',[tools['objdump'],'-p',program],packages)
                    imports=re.findall(rb'DLL Name:\s*(\S+)',table) if win else re.findall(rb'\bNEEDED\s+(\S+)',table)
                    lower={n.lower() for n in imports}
                    assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                    assert any(b'xge3_host' in n for n in lower)==(kind=='dynamic')
                    report['programs'].append(dict(path=str(program),sha256=sha(program),bytes=program.stat().st_size,
                        imports=[n.decode() for n in imports]))
                assert (b'Title snapshots: null, UTF-8, empty and independent copies passed' if source==case
                    else b'My xlang application') in output
            if source==case:
                native_includes=['-I'+str(p) for p in
                    (root,sdk,sdk/'include',*sorted(dynamic.glob('*/include')))]
                for standard,tool,language in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                    run('public-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',
                        *native_includes,'-x',language,'-fsyntax-only',header],dynamic)
        report.update(complete=True,passed=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()))
        assert report['inputsUnchanged']
    except Exception as error:
        report.update(complete=True,passed=False,error=str(error))
        raise
    finally:
        save()


if __name__=='__main__':
    main()
