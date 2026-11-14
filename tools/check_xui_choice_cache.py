"""Existing choice methods against the native init/cache corrections.
Full O2 compiler and pinned SDK; CPU owned-resource and allocation-failure gates,
C/C++ headers, JIT/dynamic/static and bounded CJK window lessons. Native O0/O2
qualification is supplied separately; no network or shipping installation.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in('root','compiler','compiler-report','native-root','engine','static-build','dynamic-packages','static-packages','out','native-report'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--font',type=Path,required=True)
    a=p.parse_args();root,compiler,build,native,engine,static_build,dynamic,static,out=[v.resolve()for v in
        (a.root,a.compiler,a.compiler_report,a.native_root,a.engine,a.static_build,a.dynamic_packages,a.static_packages,a.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt'
    tools={n:Path(shutil.which(n)).resolve()for n in('gcc','g++','objdump')}
    case=root/'test/cases/foreign_xui_choice_cache.xl';examples=[root/('lib/bindings/xui/examples/'+n+'.xl')for n in('button','checkbox','toggle')]
    fixture=case.with_suffix('.c')
    h=json.loads((engine/'build-report.json').read_text(encoding='utf-8'))
    s=json.loads((static_build/'build-report.json').read_text(encoding='utf-8'))
    assert h['linked']and h['xrt_mode']=='host'and s['passed']and s['sourceUnchanged']and s['xrt_mode']=='direct-external'
    image=engine/h['runtime_image'];library=engine/h['import_library']if win else image;archive=static_build/s['archive']
    runtime=compiler.parent/('xlc.dll'if win else'libxlc.so')
    files=[Path(__file__).resolve(),compiler,build,*extra,*tools.values(),case,fixture,case.with_suffix('.h'),*examples,a.font.resolve(),root/'lib/bindings/xui/examples/_window.xl',native/'tests/xui/xui_test_proxy.c',native/'tests/xui/xui_test_proxy.h',
        Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py'),
        image,library,archive,engine/'build-report.json',static_build/'build-report.json',engine/'xge_build_config.h',
        static_build/'xge_build_config.h',static_build/'xge_static_xrt_contract.h',*sorted((engine/'host-xrt').glob('*'))]
    files.extend(native/v for v in('include/xui.h','src/xui/controls/xui_radio.c','src/xui/controls/xui_checkbox.c','src/xui/controls/xui_toggle.c','src/xui/controls/xui_button.c'))
    files.extend(root/v for v in('test/cases/foreign_xui_render_backend.c','test/cases/foreign_xui_render_backend.h','test/cases/foreign_xui_render_adapters.inc'))
    files.append(a.native_report.resolve());nd=json.loads(a.native_report.read_text(encoding='utf-8'));assert nd['complete']and nd['passed']and nd['inputsUnchanged']and len(nd['rows'])==52
    for artifact in(image,library,archive):assert nd['inputs'][str(artifact)]==hashlib.sha256(artifact.read_bytes()).hexdigest()
    files.extend(compiler.parent/n for n in(('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    for directory in(sdk,dynamic,static):files.extend(v for v in directory.rglob('*')if v.is_file())
    sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest();inputs={str(v):sha(v)for v in files}
    out.mkdir(parents=True,exist_ok=False);snapshots=out/'input-snapshots';snapshots.mkdir()
    for v in(Path(__file__).resolve(),case,fixture,case.with_suffix('.h'),*examples):shutil.copy2(v,snapshots/v.name)
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,
        nativeRebuilt=True,networkTests=False,cpuEquivalenceClaim=False,inputs=inputs,rows=[],programs=[],heap=[],readbacks=[])
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
        for source in(case,*examples):
            name=source.stem;lesson=source!=case;c=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,'--emit',c,'--emit-header',header,*includes],dynamic)
            generated=c.read_text(encoding='utf-8-sig')
            symbols=['xuiButtonCreate','xuiCheckBoxCreate','xuiToggleCreate','xuiRadioCreate','xuiRenderPrepare']
            if lesson:symbols=['xui'+{'button':'Button','checkbox':'CheckBox','toggle':'Toggle'}[source.stem]+'Create']
            for symbol in symbols:assert re.search(r'\b'+symbol+r'\(',generated),symbol
            if not lesson:
                report['generatedC']=dict(path=str(c),sha256=sha(c),directCalls={symbol:len(re.findall(r'(?<![\w&])'+symbol+r'\(',generated))for symbol in symbols},
                    existingDeclarationsUnchanged=True,nativeCacheCorrections=True,zeroBridgeOverheadClaim=False)
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
                    assert b'ChoiceCache: isolated generations, owned resources, 256 OOM prefixes and exact cleanup passed'in output
                    counts=re.findall(rb'Choice idle kind=(\d+) native=(\d+) queries=(\d+) alloc=(\d+) free=(\d+) realloc=(\d+)',output)
                    assert len(counts)==8
                    values={(int(v[0]),int(v[1])):list(map(int,v[2:]))for v in counts}
                    assert values=={(k,n):[1000,0,0,0]for k in range(4)for n in range(2)},values
                    report['heap'].append(dict(mode=kind,idleRounds=1000,allocationPrefixes=256,counts=[dict(kind=k,native=n,values=v)for(k,n),v in values.items()],cpuEquivalenceClaim=False))
                else:
                    dimensions=re.search(rb'RGBA (\d+) (\d+)',output);assert dimensions
                    w,h=map(int,dimensions.groups());image=out/(name+'-'+kind+('-execute'if kind!='jit'else'')+'.rgba');data=image.read_bytes()
                    assert w>0 and h>0 and len(data)==w*h*4 and len({data[i:i+4]for i in range(0,len(data),4)})>1
                    report['readbacks'].append(dict(lesson=name,mode=kind,path=str(image),width=w,height=h,sha256=sha(image)))
                save()
        for source in examples:assert len({v['sha256']for v in report['readbacks']if v['lesson']==source.stem})==1
        assert len(report['rows'])==44
        report.update(complete=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()));report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
