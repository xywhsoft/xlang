"""Qualify Breadcrumb/MenuBar/StatusBar direct record-column bulk inputs.
Actual complete O2 compiler and matched SDK; independent native O0/O2 callers,
JIT/dynamic/static CPU and real-window lesson, C/C++ headers, OOM/heap/weak menus.
No new native/compiler semantics, network or shipping installation.
"""
from pathlib import Path
import ast,argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in('root','compiler','compiler-report','native-root','engine','static-build','dynamic-packages','static-packages','out'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--font',type=Path,required=True)
    a=p.parse_args();root,compiler,build,native,engine,static_build,dynamic,static,out=[v.resolve()for v in
        (a.root,a.compiler,a.compiler_report,a.native_root,a.engine,a.static_build,a.dynamic_packages,a.static_packages,a.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt'
    tools={n:Path(shutil.which(n)).resolve()for n in('gcc','g++','objdump')}
    case=root/'test/cases/foreign_xui_control_items.xl';examples=[root/'lib/bindings/xui/examples/control_items.xl']
    fixture=case.with_suffix('.c');driver=case.with_name(case.stem+'_native.c')
    h=json.loads((engine/'build-report.json').read_text(encoding='utf-8'))
    s=json.loads((static_build/'build-report.json').read_text(encoding='utf-8'))
    assert h['linked']and h['xrt_mode']=='host'and s['passed']and s['sourceUnchanged']and s['xrt_mode']=='direct-external'
    image=engine/h['runtime_image'];library=engine/h['import_library']if win else image;archive=static_build/s['archive']
    runtime=compiler.parent/('xlc.dll'if win else'libxlc.so')
    files=[Path(__file__).resolve(),compiler,build,*extra,*tools.values(),case,fixture,case.with_suffix('.h'),driver,*examples,a.font.resolve(),root/'lib/bindings/xui/examples/_window.xl',root/'test/cases/foreign_xui_render_backend.c',root/'test/cases/foreign_xui_render_backend.h',root/'test/cases/foreign_xui_render_adapters.inc',native/'tests/xui/xui_test_proxy.c',native/'tests/xui/xui_test_proxy.h',
        Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py'),
        image,library,archive,engine/'build-report.json',static_build/'build-report.json',engine/'xge_build_config.h',
        static_build/'xge_build_config.h',static_build/'xge_static_xrt_contract.h',*sorted((engine/'host-xrt').glob('*'))]
    files.extend(native/v for v in('include/xui.h','src/xui/controls/xui_breadcrumb.c','src/xui/controls/xui_menubar.c','src/xui/controls/xui_statusbar.c'));files.extend((root/'tools/check_xge3_bindings.py',root/'lib/bindings/xui/src/managed.xl'))
    files.extend(compiler.parent/n for n in(('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    for directory in(sdk,dynamic,static):files.extend(v for v in directory.rglob('*')if v.is_file())
    sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest();inputs={str(v):sha(v)for v in files}
    out.mkdir(parents=True,exist_ok=False);snapshots=out/'input-snapshots';snapshots.mkdir()
    for v in(Path(__file__).resolve(),case,fixture,case.with_suffix('.h'),driver,*examples):shutil.copy2(v,snapshots/v.name)
    tree=ast.parse((root/'tools/check_xge3_bindings.py').read_text());assignments=[n for n in ast.walk(tree)if isinstance(n,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='array_methods'for t in n.targets)];assert len(assignments)==1;definitions=ast.literal_eval(assignments[0].value);managed=(root/'lib/bindings/xui/src/managed.xl').read_text();native_symbols=['xuiBreadcrumbSetItems','xuiMenuBarSetItems','xuiStatusBarSetItems']
    for symbol in native_symbols:
        owner,name,contract,parameters=definitions[symbol];declaration=' extern "'+symbol+'" @no_raise @c_result("status",0) '+contract+' func '+name+'('+parameters+')';start=managed.index(' class '+owner+' ');finish=managed.index('\nend',start);assert managed[start:finish].count(declaration)==1
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,
        nativeRebuilt=False,networkTests=False,cpuEquivalenceClaim=False,inputs=inputs,rows=[],programs=[],heap=[],nativeBudgets=[],readbacks=[])
    def save():
        target=out/'results.json.tmp';target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');os.replace(target,out/'results.json')
    def run(name,command,packages,lesson=False):
        env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k not in ('XLANG_PACKAGE_PATH','XUI_EXAMPLE_DUMP','XUI_EXAMPLE_CYCLE','XUI_EXAMPLE_EXERCISE')}
        env['XLANG_PACKAGE_PATH']=str(packages);key='PATH'if win else'LD_LIBRARY_PATH'
        env[key]=os.pathsep.join((str(packages/'_xge_native/native'),str(engine),str(runtime.parent),env.get(key,'')))
        if lesson:env.update(XUI_EXAMPLE_FRAMES='2',XUI_EXAMPLE_FONT=str(a.font.resolve()),XUI_EXAMPLE_DUMP=str(out/(name+'.rgba')))
        command=list(map(str,command));start=time.monotonic()
        def no_core():
            import resource
            resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        startup=None
        if win:
            startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        z=subprocess.run(command,cwd=root/('release/win_x64'if win else'release/linux_x64')if lesson else root,env=env,capture_output=True,timeout=300,preexec_fn=None if win else no_core,startupinfo=startup)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert not z.returncode,log;return z.stdout+z.stderr
    includes=['--inc-path',root,'--inc-path',sdk,'--inc-path',native]
    c_includes=['-I'+str(v)for v in(root,sdk,native,native/'include',*sorted(dynamic.glob('*/include')))]
    try:
        # Independently compiled C does not use a generated-language entry point.
        for kind,folder,lib in(('host',engine,library),('static',static_build,archive)):
            flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-cast-function-type','-DXGE_XRT_HOST=1',
                '-include',folder/'xge_build_config.h',*c_includes,'-I'+str(folder),'-I'+str(engine/'host-xrt')]
            flags+=['-DXGE_TEST_STATIC=1','-DXL6_XGE_STATIC=1']if kind=='static'else['-DXGE_DLL=1','-DXUI_DLL=1']if win else[]
            systems=['-l'+n for n in s['system_libraries']]if kind=='static'else[]
            if not win:systems+=['-Wl,-rpath,'+str(engine),'-Wl,-rpath,'+str(runtime.parent),'-ldl','-lm','-pthread']
            for level in('O0','O2'):
                name='native-'+kind+'-'+level;binary=out/(name+('.exe'if win else''))
                run(name+'-build',[tools['gcc'],*flags,'-'+level,driver,fixture,lib,runtime,*systems,'-o',binary],dynamic)
                output=run(name,[binary],dynamic)
                assert b'ControlItems native: exact fields, copied/aliased text, normalized values, OOM rollback and weak menus passed'in output
                counts=re.findall(rb'Native control item kind=(\d+) prefixes=(\d+) failures=(\d+)',output);assert [v[0]for v in counts]==[b'0',b'1',b'2']and all(int(v[1])>int(v[2])>0 for v in counts);report['nativeBudgets'].append(dict(kind=kind,optimization=level,counts=[list(map(int,v))for v in counts]))
        for source in(case,*examples):
            name=source.stem;lesson=source!=case;c=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,'--emit',c,'--emit-header',header,*includes],dynamic)
            generated=c.read_text(encoding='utf-8-sig')
            symbols=native_symbols
            for symbol in symbols:assert re.search(r'\b'+symbol+r'\(',generated),symbol
            if not lesson:
                report['generatedC']=dict(path=str(c),sha256=sha(c),directCalls={symbol:len(re.findall(r'(?<![\w&])'+symbol+r'\(',generated))for symbol in symbols},
                    recordArrayInput=True,weakMenuInput=True,zeroBridgeOverheadClaim=False)
            for standard,tool,language in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run(name+'-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*c_includes,'-x',language,'-fsyntax-only',header],dynamic)
            for kind,packages in(('jit',dynamic),('dynamic',dynamic),('static',static)):
                if kind=='jit':output=run(name+'-'+kind,[compiler,'run',source,*includes],packages,lesson)
                else:
                    binary=out/(name+'-'+kind+('.exe'if win else''))
                    run(name+'-'+kind+'-build',[compiler,'build',source,'-o',binary,*includes],packages)
                    output=run(name+'-'+kind+'-execute',[binary],packages,lesson)
                    table=run(name+'-'+kind+'-imports',[tools['objdump'],'-p',binary],packages)
                    imports=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table);lower={v.lower()for v in imports}
                    assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                    assert any(b'xge3_host'in v for v in lower)==(kind=='dynamic')
                    report['programs'].append(dict(path=str(binary),sha256=sha(binary),bytes=binary.stat().st_size,imports=[v.decode()for v in imports]))
                if not lesson:
                    assert b'ControlItems: exact columns, copied text, native normalization, failures, weak menus, thread and expiry passed'in output
                    counts=re.findall(rb'Control items heap kind=([012]) (language|native): alloc=(\d+) free=(\d+) realloc=(\d+) row=(\d+)',output)
                    values=[(int(row[0]),row[1].decode(),list(map(int,row[2:])))for row in counts]
                    assert len(values)==6
                    for k in range(3):
                        language,native_row=values[k*2:k*2+2];assert language[:2]==(k,'language')and native_row[:2]==(k,'native')
                        assert language[2][0]==language[2][1]==(2000 if k==1 else 1000)and language[2][2]==0
                        assert native_row[2][:3]==[0,0,0]and native_row[2][3]==language[2][3]
                    prefixes=re.findall(rb'Control item allocation kind=(\d+) prefixes=(\d+)',output);assert [v[0]for v in prefixes]==[b'0',b'1',b'2']and all(int(v[1])>0 for v in prefixes)
                    report['heap'].append(dict(mode=kind,rounds=1000,counts=values,allocationPrefixes=[list(map(int,v))for v in prefixes],counterScope='XRT-only warmed calls; input setup and native CRT excluded',cpuEquivalenceClaim=False))
                else:
                    dimensions=re.search(rb'RGBA (\d+) (\d+)',output);assert dimensions
                    w,h=map(int,dimensions.groups());image=out/(name+'-'+kind+('-execute'if kind!='jit'else'')+'.rgba');data=image.read_bytes()
                    assert w>0 and h>0 and len(data)==w*h*4 and len({data[i:i+4]for i in range(0,len(data),4)})>1
                    report['readbacks'].append(dict(lesson=name,mode=kind,path=str(image),width=w,height=h,sha256=sha(image)))
                save()
        for source in examples:assert len({v['sha256']for v in report['readbacks']if v['lesson']==source.stem})==1
        assert len(report['rows'])==30
        report.update(complete=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()));report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
