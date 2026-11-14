"""Unique TextLayout using complete compilers and matched O2 engine SDKs.

CPU lifetime/OOM/length/line tests plus a readable real-window lesson in JIT,
dynamic and static AOT. Public headers compile as C11/C++11/C++17. No remote
services or network/security tests. This does not install shipping artifacts.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in('root','compiler','compiler-report','native-root','dynamic-packages','static-packages','out'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--window-tests',action='store_true')
    p.add_argument('--example-only',action='store_true',help='Validate only a revised lesson; does not certify the CPU fixture')
    p.add_argument('--font',type=Path)
    a=p.parse_args()
    if a.window_tests and a.font is None:p.error('--window-tests requires --font')
    root,compiler,build,native,dynamic,static,out=[v.resolve()for v in
        (a.root,a.compiler,a.compiler_report,a.native_root,a.dynamic_packages,a.static_packages,a.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt'
    tools={n:Path(shutil.which(n)).resolve()for n in('gcc','g++','objdump')}
    case=root/'test/cases/foreign_xui_text_layout.xl'
    example=root/'lib/bindings/xui/examples/text_layout.xl'
    font=a.font.resolve()if a.font else None
    files=[Path(__file__).resolve(),compiler,build,*extra,*tools.values(),case,case.with_suffix('.c'),case.with_suffix('.h'),example,
        example.parent/'_window.xl',native/'include/xui.h',native/'src/xui/core/xui_text.c',
        native/'src/xui/core/xui_text_queries.inl',native/'tests/xui/xui_test_proxy.c',native/'tests/xui/xui_test_proxy.h',
        Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py')]
    if font:files.append(font)
    files.extend(compiler.parent/n for n in(('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    for directory in(sdk,dynamic,static):files.extend(v for v in directory.rglob('*')if v.is_file())
    sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest();inputs={str(v):sha(v)for v in files}
    out.mkdir(parents=True,exist_ok=False);snapshots=out/'input-snapshots';snapshots.mkdir()
    for v in[Path(__file__).resolve(),case,case.with_suffix('.c'),case.with_suffix('.h'),example]:shutil.copy2(v,snapshots/v.name)
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,networkTests=False,
        windowTests=a.window_tests,caseTests=not a.example_only,inputs=inputs,rows=[],programs=[],heap=[],readbacks=[])
    def save():
        target=out/'results.json.tmp';target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');os.replace(target,out/'results.json')
    def run(name,command,packages,lesson=False):
        env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k not in
            ('XLANG_PACKAGE_PATH','XUI_EXAMPLE_DUMP','XUI_EXAMPLE_CYCLE','XUI_EXAMPLE_EXERCISE')}
        env['XLANG_PACKAGE_PATH']=str(packages);key='PATH'if win else'LD_LIBRARY_PATH'
        env[key]=str(packages/'_xge_native/native')+os.pathsep+env.get(key,'')
        env['XUI_EXAMPLE_FRAMES']='2'
        if font:env['XUI_EXAMPLE_FONT']=str(font)
        if lesson:env['XUI_EXAMPLE_DUMP']=str(out/(name+'.rgba'))
        command=list(map(str,command));start=time.monotonic();startup=None
        if win:
            startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300,startupinfo=startup)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert not z.returncode,log;return z.stdout+z.stderr
    includes=['--inc-path',root,'--inc-path',sdk,'--inc-path',native]
    c_includes=['-I'+str(v)for v in(root,sdk,native,native/'include',*sorted(dynamic.glob('*/include')))]
    try:
        for source in((example,)if a.example_only else(case,example)):
            name=source.stem;lesson=source==example;c=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,'--emit',c,'--emit-header',header,*includes],dynamic)
            generated=c.read_text(encoding='utf-8-sig')
            symbols=['xuiTextLayoutCreate','xuiTextLayoutGetState','xuiTextLayoutGetLine','xuiTextLayoutDraw']
            if not lesson:symbols+=['xuiTextLayoutReset','xuiTextLayoutGetTextView','xuiTextMeasureLayout']
            for symbol in symbols:assert re.search(r'\b'+symbol+r'\(',generated),symbol
            assert not re.search(r'\bxuiTextLayoutGet(?:Size|LineCount|Truncated|Text)\(',generated)
            if not lesson:
                for level in('O0','O2'):
                    run('provider-'+level,[tools['gcc'],'-std=c11','-'+level,'-Wall','-Wextra','-Werror',*c_includes,'-fsyntax-only',case.with_suffix('.c')],dynamic)
            for standard,tool,language in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run(name+'-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*c_includes,'-x',language,'-fsyntax-only',header],dynamic)
            for kind,packages in(('jit',dynamic),('dynamic',dynamic),('static',static)):
                if kind=='jit':
                    if lesson and not a.window_tests:continue
                    output=run(name+'-'+kind,[compiler,'run',source,*includes],packages,lesson)
                else:
                    binary=out/(name+'-'+kind+('.exe'if win else''))
                    run(name+'-'+kind+'-build',[compiler,'build',source,'-o',binary,*includes],packages)
                    output=run(name+'-'+kind+'-execute',[binary],packages,lesson)if not lesson or a.window_tests else b''
                    table=run(name+'-'+kind+'-imports',[tools['objdump'],'-p',binary],packages)
                    imports=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table)
                    lower={v.lower()for v in imports}
                    assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                    assert any(b'xge3_host'in v for v in lower)==(kind=='dynamic')
                    report['programs'].append(dict(path=str(binary),sha256=sha(binary),bytes=binary.stat().st_size,imports=[v.decode()for v in imports]))
                if not lesson:
                    assert b'TextLayout: packed factory/reset, sized text, checked queries, OOM, thread and unique lifetime passed'in output
                    counts=re.findall(rb'Layout heap (language|native): alloc=(\d+) free=(\d+) realloc=(\d+)',output)
                    assert len(counts)==2 and all(tuple(map(int,row[1:]))==(0,0,0)for row in counts)
                    report['heap'].append(dict(mode=kind,rounds=1000,nativeCallsPerRound=1,
                        counts={row[0].decode():dict(zip(('alloc','free','realloc'),map(int,row[1:])))for row in counts},cpuEquivalenceClaim=False))
                elif a.window_tests:
                    match=re.search(rb'RGBA (\d+) (\d+)',output);assert match,output[-1000:]
                    w,h=map(int,match.groups());image=out/(name+'-'+kind+('-execute'if kind!='jit'else'')+'.rgba')
                    assert image.stat().st_size==w*h*4
                    report['readbacks'].append(dict(mode=kind,path=str(image),width=w,height=h,sha256=sha(image)))
                save()
        if a.window_tests:assert len({v['sha256']for v in report['readbacks']})==1,'Same-platform mode readbacks differ'
        report.update(complete=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()));report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
