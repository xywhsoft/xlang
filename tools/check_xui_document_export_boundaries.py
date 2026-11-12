"""Exercise Document ownership through the actual full compiler and native SDK.

Seven producers, reference-valued range arguments, invalid output transactions,
448 language allocation budgets, equal-workload C/text heap counts, JIT and
independent hosted/static executables. Does not certify renderer/GPU behavior,
the complete Document API, package shared-library initialization or CPU speed.
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
    win=os.name=='nt'
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    tools={name:Path(shutil.which(name)).resolve() for name in ('gcc','g++','objdump')}
    case=root/'test/cases/foreign_xui_document_export_boundaries.xl'
    files=[compiler,build,Path(__file__),Path(__file__).with_name('foreign_compiler_cohort.py'),
        Path(__file__).with_name('full_compiler_qualification.py'),*extra,*tools.values(),
        case,case.with_suffix('.c'),case.with_suffix('.h')]
    files += [compiler.parent/name for name in (('xl.dll','xlc.dll') if win else ('libxl.so','libxlc.so'))]
    for folder in (sdk,dynamic,static):
        files.extend(p for p in folder.rglob('*') if p.is_file())
    inputs={str(p):sha(p) for p in files}
    report=dict(complete=False,passed=False,scope=__doc__,fullCompilerBuild=True,
        shippingDelivery=False,sharedLibraryPackageQualified=False,cpuPerformanceClaim=False,
        inputs=inputs,rows=[],programs=[])
    def save():
        temporary=out/'results.json.tmp'
        temporary.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        os.replace(temporary,out/'results.json')
    def run(name,command,packages=dynamic):
        env={k:v for k,v in os.environ.items() if not k.startswith('XL6_') and k!='XLANG_PACKAGE_PATH'}
        env['XLANG_PACKAGE_PATH']=str(packages)
        key='PATH' if win else 'LD_LIBRARY_PATH'
        env[key]=str(packages/'_xge_native/native')+os.pathsep+env.get(key,'')
        command=list(map(str,command))
        started=time.monotonic()
        completed=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
        log=out/(name+'.log')
        log.write_bytes(completed.stdout+completed.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=completed.returncode,
            seconds=time.monotonic()-started,logSha256=sha(log)))
        save()
        print(name,completed.returncode,flush=True)
        assert not completed.returncode,log
        return completed.stdout+completed.stderr
    try:
        includes=['--inc-path',root,'--inc-path',sdk]
        generated=out/'document.c'
        header=out/'document.h'
        run('emit',[compiler,'cgen',case,'--emit',generated,'--emit-header',header,*includes])
        text=generated.read_text(encoding='utf-8-sig')
        names=('xuiDocumentSerialize','xuiDocumentExportHtml','xuiDocumentSnapshotCopyPlainText',
            'xuiDocumentSnapshotCopyRange','xuiDocumentSnapshotCopyTableMatrix',
            'xuiDocumentFragmentSerialize','xuiDocumentFragmentExportHtml')
        assert all(re.search(r'\b'+name+r'\(',text) for name in names)
        assert 'xl6COutputTextTakeV1(' in text and 'xl6COutputBytesTakeV1(' in text
        for kind,packages in (('jit',dynamic),('dynamic',dynamic),('static',static)):
            if kind=='jit':
                output=run(kind,[compiler,'run',case,*includes],packages)
            else:
                program=out/(kind+('.exe' if win else ''))
                run(kind+'-build',[compiler,'build',case,'-o',program,*includes],packages)
                output=run(kind+'-execute',[program],packages)
                table=run(kind+'-imports',[tools['objdump'],'-p',program],packages)
                imports=re.findall(rb'DLL Name:\s*(\S+)',table) if win else re.findall(rb'\bNEEDED\s+(\S+)',table)
                lower={name.lower() for name in imports}
                assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                assert any(b'xge3_host' in name for name in lower)==(kind=='dynamic')
                report['programs'].append(dict(path=str(program),sha256=sha(program),bytes=program.stat().st_size,
                    imports=[name.decode() for name in imports]))
            assert b'seven producers, 448 language OOM budgets and range/matrix reference ABI passed' in output
            counts=re.search(rb'Document language text: (\d+) allocations, (\d+) frees, (\d+) reallocations; equal native workload',output)
            assert counts
            report.setdefault('heapCounts',{})[kind]=[int(v) for v in counts.groups()]
        native_includes=['-I'+str(v) for v in (root,sdk,sdk/'include',dynamic/'_xge_native/include')]
        for standard,tool,language in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
            run('public-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',
                *native_includes,'-x',language,'-fsyntax-only',header])
        report.update(complete=True,passed=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()))
        assert report['inputsUnchanged']
    except Exception as error:
        report.update(complete=True,passed=False,error=str(error))
        raise
    finally:
        save()


if __name__=='__main__':
    main()
