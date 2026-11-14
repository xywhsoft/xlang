"""Owned hotkey/error callbacks and CString error fields with a matched full SDK.

Checks native metadata/epoch expiry, self-removal/replacement, checked errors,
registration OOM and foreign threads in JIT/dynamic/static CPU clients. A bounded
real-window lesson verifies rendering, not physical keyboard input or OS IME.
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
    case=root/'test/cases/foreign_xui_input_owned.xl'
    example=root/'lib/bindings/xui/examples/input_callbacks.xl'
    files=[compiler,build,Path(__file__),case,example,*extra,*tools.values(),
        Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py')]
    files += [case.with_suffix('.c'),case.with_suffix('.h'),root/'test/cases/foreign_xui_input_extended.c',root/'test/cases/foreign_xui_input_extended.h',root/'test/cases/foreign_xui_text_layout.c',root/'test/cases/foreign_xui_text_layout.h',native/'tests/xui/xui_test_proxy.c',native/'tests/xui/xui_test_proxy.h',root/'lib/bindings/xui/examples/_window.xl']
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
        for source in (case,example):
            name=source.stem;generated=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,'--emit',generated,'--emit-header',header,*includes],dynamic)
            text=generated.read_text(encoding='utf-8-sig')
            for symbol in ('xuiHotKeyRegisterOwned','xuiSetErrorCallbackOwned'):
                assert re.search(r'\b'+symbol+r'\(',text),symbol
            if source==case:
                assert '->sOperation' in text and '->sMessage' in text and 'cstring_size' in text
            for kind,packages in (('jit',dynamic),('dynamic',dynamic),('static',static)):
                lesson=source==example
                if kind=='jit':
                    if lesson and not args.window_tests:continue
                    output=run(name+'-'+kind,[compiler,'run',source,*includes],packages,lesson)
                else:
                    program=out/(name+'-'+kind+('.exe' if win else ''))
                    run(name+'-'+kind+'-build',[compiler,'build',source,'-o',program,*includes],packages)
                    output=run(name+'-'+kind+'-execute',[program],packages,lesson)if not lesson or args.window_tests else b''
                    table=run(name+'-'+kind+'-imports',[tools['objdump'],'-p',program],packages)
                    imports=re.findall(rb'DLL Name:\s*(\S+)',table) if win else re.findall(rb'\bNEEDED\s+(\S+)',table)
                    lower={n.lower() for n in imports}
                    assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                    assert any(b'xge3_host' in n for n in lower)==(kind=='dynamic')
                    report['programs'].append(dict(path=str(program),sha256=sha(program),bytes=program.stat().st_size,
                        imports=[n.decode() for n in imports]))
                if source==case:
                    assert b'InputOwners language:'in output
                    counts=re.findall(rb'InputEx heap kind=(\d+) alloc=(\d+) free=(\d+) realloc=(\d+)',output)
                    assert {int(v[0]):tuple(map(int,v[1:]))for v in counts}=={0:(0,0,0),1:(0,0,0)}
                    report.setdefault('heap',[]).append(dict(mode=kind,rounds=1000,languageAndNativeAllocFreeRealloc=[0,0,0],cpuEquivalenceClaim=False))
                elif args.window_tests:
                    size=re.search(rb'RGBA (\d+) (\d+)',output);assert size
                    width,height=map(int,size.groups());path=out/(name+'-'+kind+('-execute'if kind!='jit'else'')+'.rgba');data=path.read_bytes()
                    assert width>0 and height>0 and len(data)==width*height*4
                    assert len({data[i:i+4]for i in range(0,len(data),4)})>1
                    report.setdefault('readbacks',[]).append(dict(mode=kind,width=width,height=height,path=str(path),sha256=sha(path)))
            if source==case:
                native_includes=['-I'+str(p) for p in
                    (root,sdk,sdk/'include',native,native/'include',*sorted(dynamic.glob('*/include')))]
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
