"""Cold Widget.measure allocation prefixes with a qualified full compiler/SDK.
No new ABI or native implementation; validates failure publication and recovery
in JIT/dynamic/static modes. Run beside check_xui_widget_queries.py.
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
    for name in ('root','compiler','compiler-report','native-root','dynamic-packages','static-packages','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--window-tests',action='store_true')
    parser.add_argument('--font',type=Path)
    args=parser.parse_args()
    if args.window_tests and args.font is None:parser.error('--window-tests requires --font')
    root,compiler,build,native,dynamic,static,out=[v.resolve() for v in
        (args.root,args.compiler,args.compiler_report,args.native_root,args.dynamic_packages,args.static_packages,args.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build)
    out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    win=os.name=='nt'
    tools={name:Path(shutil.which(name)).resolve() for name in ('gcc','g++','objdump')}
    case=root/'test/cases/foreign_xui_measure_oom.xl'
    files=[compiler,build,Path(__file__),case,*extra,*tools.values(),
        Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py')]
    files += [case.with_suffix('.c'),root/'test/cases/foreign_xui_widget_queries.c',root/'test/cases/foreign_xui_widget_queries.h',root/'test/cases/foreign_xui_text_layout.c',root/'test/cases/foreign_xui_text_layout.h',native/'tests/xui/xui_test_proxy.c',native/'tests/xui/xui_test_proxy.h',root/'lib/bindings/xui/examples/_window.xl']
    if args.font:files.append(args.font.resolve())
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
    def run(name,command,packages,window=False):
        env={k:v for k,v in os.environ.items() if not k.startswith('XL6_') and k!='XLANG_PACKAGE_PATH'}
        env['XLANG_PACKAGE_PATH']=str(packages)
        key='PATH' if win else 'LD_LIBRARY_PATH'
        env[key]=str(packages/'_xge_native/native')+os.pathsep+env.get(key,'')
        if window:
            env.update(XUI_EXAMPLE_FRAMES='2',XUI_EXAMPLE_FONT=str(args.font.resolve()),XUI_EXAMPLE_DUMP=str(out/(name+'.rgba')))
        command=list(map(str,command));started=time.monotonic()
        completed=subprocess.run(command,cwd=(root/'release/win_x64'if win and window else root),env=env,capture_output=True,timeout=300)
        log=out/(name+'.log');log.write_bytes(completed.stdout+completed.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=completed.returncode,
            seconds=time.monotonic()-started,logSha256=sha(log)))
        save();print(name,completed.returncode,flush=True)
        assert not completed.returncode,log
        return completed.stdout+completed.stderr
    try:
        includes=['--inc-path',root,'--inc-path',sdk,'--inc-path',native]
        for kind,packages in (('jit',dynamic),('dynamic',dynamic),('static',static)):
            if kind=='jit':output=run('measure-oom-jit',[compiler,'run',case,*includes],packages)
            else:
                program=out/('measure-oom-'+kind+('.exe'if win else''))
                run('measure-oom-'+kind+'-build',[compiler,'build',case,'-o',program,*includes],packages)
                output=run('measure-oom-'+kind+'-execute',[program],packages)
                report['programs'].append(dict(path=str(program),sha256=sha(program),bytes=program.stat().st_size))
            assert b'Widget measure OOM: 32 cold-layout prefixes preserve failed destinations and recover'in output
        report.update(complete=True,passed=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()))
        assert report['inputsUnchanged']
    except Exception as error:
        report.update(complete=True,passed=False,error=str(error))
        raise
    finally:
        save()


if __name__=='__main__':
    main()
