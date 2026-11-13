"""Context-owned localization with full compiler and matched SDKs.

Real CPU registry/owner/copy/OOM cases in JIT and independent dynamic/static
programs. The short learning lesson is built in all modes; --window-tests
also executes its bounded real window loop and validates an RGBA readback.
No network or security tests.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in('root','compiler','compiler-report','native-root','dynamic-packages','static-packages','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--window-tests',action='store_true')
    parser.add_argument('--font',type=Path,help='Explicit font input for reproducible window readbacks')
    a=parser.parse_args()
    if a.window_tests and a.font is None:parser.error('--window-tests requires --font')
    font=a.font.resolve()if a.font is not None else None
    root,compiler,build,native,dynamic,static,out=[p.resolve()for p in
        (a.root,a.compiler,a.compiler_report,a.native_root,a.dynamic_packages,a.static_packages,a.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt'
    tools={n:Path(shutil.which(n)).resolve()for n in('gcc','g++','objdump')}
    case=root/'test/cases/foreign_xui_localization.xl';example=root/'lib/bindings/xui/examples/localization.xl'
    files=[Path(__file__),compiler,build,case,case.with_suffix('.c'),case.with_suffix('.h'),example,
        *extra,*tools.values(),Path(__file__).with_name('foreign_compiler_cohort.py'),
        Path(__file__).with_name('full_compiler_qualification.py'),native/'include/xui.h',
        native/'src/xui/core/xui_core.c',native/'src/xui/core/xui_internal.h',native/'tests/xui/xui_test_proxy.c',native/'tests/xui/xui_test_proxy.h',root/'test/cases/foreign_xui_icon_models.c',root/'test/cases/foreign_xui_icon_models.h',example.parent/'_window.xl']
    if font is not None:files.append(font)
    files.extend(compiler.parent/n for n in(('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    for folder in(sdk,dynamic,static):files.extend(p for p in folder.rglob('*')if p.is_file())
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    inputs={str(p):sha(p)for p in files};out.mkdir(parents=True,exist_ok=False)
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,
        networkTests=False,windowTests=a.window_tests,font=str(font)if font else None,inputs=inputs,rows=[],programs=[],heap=[])
    def save():
        p=out/'results.json.tmp';p.write_text(json.dumps(report,indent=2)+'\n');os.replace(p,out/'results.json')
    def run(name,command,packages):
        env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
        env['XLANG_PACKAGE_PATH']=str(packages);key='PATH'if win else'LD_LIBRARY_PATH'
        env[key]=str(packages/'_xge_native/native')+os.pathsep+env.get(key,'')
        env['XUI_EXAMPLE_FRAMES']='2'
        if font is not None:env['XUI_EXAMPLE_FONT']=str(font)
        if name.startswith('localization-'):
            env['XUI_EXAMPLE_DUMP']=str(out/(name+'.rgba'))
        command=list(map(str,command));start=time.monotonic()
        startup=None
        if win:
            startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300,startupinfo=startup)
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
            assert re.search(r'\bxuiCreateLanguageOwned\(',text)
            if source==case:
                for level in('O0','O2'):
                    run('provider-'+level,[tools['gcc'],'-std=c11','-'+level,'-Wall','-Wextra','-Werror',*c_includes,'-fsyntax-only',case.with_suffix('.c')],dynamic)
            for standard,tool,language in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run(name+'-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*c_includes,'-x',language,'-fsyntax-only',header],dynamic)
            for kind,packages in(('jit',dynamic),('dynamic',dynamic),('static',static)):
                if kind=='jit':
                    if source==example and not a.window_tests:continue
                    additional=[root/'test/cases/foreign_xui_icon_models.c']if source==case else[]
                    output=run(name+'-'+kind,[compiler,'run',source,*additional,*includes],packages)
                else:
                    program=out/(name+'-'+kind+('.exe'if win else''))
                    additional=[root/'test/cases/foreign_xui_icon_models.c']if source==case else[]
                    run(name+'-'+kind+'-build',[compiler,'build',source,*additional,'-o',program,*includes],packages)
                    output=run(name+'-'+kind+'-execute',[program],packages)if source==case or a.window_tests else b''
                    table=run(name+'-'+kind+'-imports',[tools['objdump'],'-p',program],packages)
                    imports=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table)
                    lower={n.lower()for n in imports}
                    assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                    assert any(b'xge3_host'in n for n in lower)==(kind=='dynamic')
                    report['programs'].append(dict(path=str(program),sha256=sha(program),bytes=program.stat().st_size,imports=[n.decode()for n in imports]))
                if source==case:
                    assert b'Localization: registry lifetime, fallback, snapshots, checked errors and 64 managed allocation budgets passed'in output
                    counts=re.findall(rb'Localization heap (language|native): alloc=(\d+) free=(\d+) realloc=(\d+)',output)
                    assert len(counts)==2 and {row[0]for row in counts}=={b'language',b'native'}
                    metrics={row[0].decode():dict(zip(('alloc','free','realloc'),map(int,row[1:])))for row in counts}
                    assert metrics['language']==metrics['native']==dict(alloc=0,free=0,realloc=0)
                    report['heap'].append(dict(mode=kind,rounds=1000,nativeCalls=2000,counts=metrics));save()
                elif a.window_tests:
                    dimensions=re.search(rb'RGBA (\d+) (\d+)',output);assert dimensions,output
                    width,height=map(int,dimensions.groups());pixels=out/(name+'-'+kind+('-execute'if kind!='jit'else'')+'.rgba')
                    data=pixels.read_bytes();assert width>0 and height>0 and len(data)==width*height*4
                    assert len(set(data[i:i+4]for i in range(0,len(data),4)))>1
                    report.setdefault('readbacks',[]).append(dict(mode=kind,width=width,height=height,path=str(pixels),sha256=sha(pixels)));save()
        report.update(complete=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
