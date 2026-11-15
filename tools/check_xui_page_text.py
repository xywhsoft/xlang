"""Qualify Page's four direct owning text snapshots without a product adapter.

Full O2 compiler, matched SDK, C O0/O2 host/static clients, JIT/dynamic/static,
C/C++ headers, OOM/expiry/thread tests and two-frame lesson readbacks. Heap
counts isolate query plus byte-length consumption; string assertions stay
outside that window. No CPU equivalence or whole-bridge zero-cost claim.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort
from stage_runtime_headers import stage as stage_runtime_headers
from xui_snapshot_contracts import PAGE_TEXT_GET,extend


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in('root','compiler','compiler-report','native-root','engine','static-build','dynamic-packages','static-packages','out','font'):
        p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();root,compiler,build,native,engine,static_build,dynamic,static,out,font=[v.resolve()for v in(a.root,a.compiler,a.compiler_report,a.native_root,a.engine,a.static_build,a.dynamic_packages,a.static_packages,a.out,a.font)]
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt';sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    assert not out.is_relative_to(root)
    tools={n:Path(shutil.which(n)).resolve()for n in('gcc','g++','objdump')}
    if not win:tools['xvfb-run']=Path(shutil.which('xvfb-run')).resolve()
    h=json.loads((engine/'build-report.json').read_text());s=json.loads((static_build/'build-report.json').read_text())
    assert h['linked']and h['xrt_mode']=='host'and s['passed']and s['sourceUnchanged']and s['xrt_mode']=='direct-external'
    image=engine/h['runtime_image'];library=engine/h['import_library']if win else image;archive=static_build/s['archive'];runtime=compiler.parent/('xlc.dll'if win else'libxlc.so')
    case=root/'test/cases/foreign_xui_page_text.xl';fixture=case.with_suffix('.c');driver=case.with_name(case.stem+'_native.c');example=root/'lib/bindings/xui/examples/page.xl'
    files=[Path(__file__).resolve(),compiler,build,runtime,*extra,*tools.values(),case,fixture,driver,case.with_suffix('.h'),example,font,root/'lib/bindings/xui/examples/_window.xl',root/'lib/bindings/xui/src/managed.xl',
        *[root/'test/cases'/f'foreign_xui_render_{n}'for n in('backend.c','backend.h','adapters.inc')],native/'tests/xui/xui_test_proxy.c',native/'tests/xui/xui_test_proxy.h',native/'src/xui/controls/xui_page.c',native/'src/xui/core/xui_text.c',
        *[Path(__file__).with_name(n+'.py')for n in('foreign_compiler_cohort','full_compiler_qualification','stage_runtime_headers','gen_tcc_vfs_resources','xui_snapshot_contracts')],image,library,archive,engine/'build-report.json',static_build/'build-report.json',engine/'xge_build_config.h',static_build/'xge_build_config.h']
    files.extend(compiler.parent/n for n in(('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    for folder in(sdk,dynamic,static):files.extend(f for f in folder.rglob('*')if f.is_file())
    inputs={str(f):sha(f)for f in files};out.mkdir(parents=True,exist_ok=False);scratch=out/'tmp';scratch.mkdir();snap=out/'input-snapshots';snap.mkdir()
    for f in(case,fixture,driver,case.with_suffix('.h'),example,Path(__file__).resolve()):shutil.copy2(f,snap/f.name)
    metadata=json.loads(build.read_text());frozen=Path(metadata['source']);fresh=out/'runtime-sdk';headers=stage_runtime_headers(frozen,fresh,frozen/'tools'/('tcc_vfs_manifest_win64_xlc.txt'if win else'tcc_vfs_manifest_linux_xlc.txt'))
    for f in fresh.rglob('*'):
        if f.is_file():inputs[str(f)]=sha(f)
    report=dict(complete=False,passed=False,shippingDelivery=False,scope=__doc__,fullCompilerBuild=True,nativeRebuilt=False,networkTests=False,cpuEquivalenceClaim=False,inputs=inputs,stagedPrivateHeaders=len(headers),rows=[],programs=[],heap=[],budgets=[],readbacks=[])
    def save():
        temp=out/'results.json.tmp';temp.write_text(json.dumps(report,indent=2)+'\n');os.replace(temp,out/'results.json')
    def run(name,command,packages=dynamic,lesson=False):
        env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k not in('XLANG_PACKAGE_PATH','XUI_EXAMPLE_DUMP','XUI_EXAMPLE_FRAMES','XUI_EXAMPLE_CYCLE','XUI_EXAMPLE_EXERCISE')}
        env.update(XLANG_PACKAGE_PATH=str(packages),TEMP=str(scratch),TMP=str(scratch),TMPDIR=str(scratch));key='PATH'if win else'LD_LIBRARY_PATH';env[key]=os.pathsep.join((str(packages/'_xge_native/native'),str(engine),str(compiler.parent),env.get(key,'')))
        if lesson:env.update(XUI_EXAMPLE_FRAMES='2',XUI_EXAMPLE_FONT=str(font),XUI_EXAMPLE_DUMP=str(out/(name+'.rgba')))
        command=list(map(str,command))
        if not win and lesson:command=[str(tools['xvfb-run']),'-a',*command]
        startup=None
        if win:startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        def no_core():
            import resource
            resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        start=time.monotonic();z=subprocess.run(command,cwd=root/('release/win_x64'if win else'release/linux_x64')if lesson else root,env=env,capture_output=True,timeout=300,startupinfo=startup,preexec_fn=None if win else no_core)
        data=z.stdout+z.stderr;log=out/(name+'.log');log.write_bytes(data);report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,passed=not z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert not z.returncode,log;return data
    includes=['--inc-path',root,'--inc-path',fresh/'include','--inc-path',sdk,'--inc-path',native]
    ci=['-I'+str(v)for v in(fresh/'include',root,sdk,native,native/'include',*sorted(dynamic.glob('*/include')))]
    try:
        managed=(dynamic/'xui/src/managed.xl').read_text(encoding='utf-8-sig');assert managed.count(PAGE_TEXT_GET.strip())==1
        classes={'Widget':[],'CodeEdit':[],'Page':[]};mapping={};extend([],classes,mapping);assert classes['Page']==[PAGE_TEXT_GET]and mapping['xuiPageGetText']=='xui.Page.getText'
        text=(native/'src/xui/controls/xui_page.c').read_text()
        for field in('sFirstText','sLastText','sPrevText','sNextText'):assert re.search(r'char\s+'+field+r'\[16\]',text)
        assert 'xuiInternalUtf8CopyGraphemes'in text
        for kind,folder,lib in(('host',engine,library),('static',static_build,archive)):
            flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-cast-function-type','-DXGE_XRT_HOST=1','-include',folder/'xge_build_config.h',*ci,'-I'+str(folder),'-I'+str(engine/'host-xrt')]
            flags+=['-DXGE_TEST_STATIC=1','-DXL6_XGE_STATIC=1']if kind=='static'else['-DXGE_DLL=1','-DXUI_DLL=1']if win else[]
            systems=['-l'+n for n in s['system_libraries']]if kind=='static'else[]
            if not win:systems+=['-Wl,-rpath,'+str(engine),'-Wl,-rpath,'+str(runtime.parent),'-ldl','-lm','-pthread']
            for opt in('O0','O2'):
                name='native-'+kind+'-'+opt;exe=out/(name+('.exe'if win else''));run(name+'-build',[tools['gcc'],*flags,'-'+opt,driver,fixture,lib,runtime,*systems,'-o',exe]);data=run(name,[exe]);assert b'Page text native: four live buffers'in data and b'Page text heap native: alloc=4000 free=4000 realloc=0'in data
        for source in(case,example):
            name=source.stem;lesson=source==example;c=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,'--emit',c,'--emit-header',header,*includes]);assert re.search(r'\bxuiPageGetText\(',c.read_text(encoding='utf-8-sig'))
            for standard,tool,language in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):run(name+'-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*ci,'-x',language,'-fsyntax-only',header])
            for kind,packages in(('jit',dynamic),('dynamic',dynamic),('static',static)):
                if kind=='jit':data=run(name+'-jit',[compiler,'run',source,*includes],packages,lesson)
                else:
                    exe=out/(name+'-'+kind+('.exe'if win else''));run(name+'-'+kind+'-build',[compiler,'build',source,'-o',exe,*includes],packages);data=run(name+'-'+kind+'-execute',[exe],packages,lesson)
                    table=run(name+'-'+kind+'-imports',[tools['objdump'],'-p',exe],packages);imports=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table);lower={v.lower()for v in imports}
                    assert not{b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                    assert any(b'xge3_host'in v for v in lower)==(kind=='dynamic');report['programs'].append(dict(path=str(exe),sha256=sha(exe),imports=[v.decode()for v in imports]))
                if lesson:
                    dims=re.search(rb'RGBA (\d+) (\d+)',data);assert dims
                    width,height=map(int,dims.groups());picture=out/(name+'-'+kind+('-execute'if kind!='jit'else'')+'.rgba');pixels=picture.read_bytes()
                    assert width>0 and height>0 and len(pixels)==width*height*4 and len({pixels[i:i+4]for i in range(0,len(pixels),4)})>1
                    report['readbacks'].append(dict(mode=kind,width=width,height=height,path=str(picture),sha256=sha(picture)))
                else:
                    assert b'Page text: direct, bound/protocol'in data
                    counts=re.findall(rb'Page text heap (language|native): alloc=(\d+) free=(\d+) realloc=(\d+)',data);assert len(counts)==2
                    values=[list(map(int,v[1:]))for v in counts];assert [v[0]for v in counts]==[b'language',b'native']and values==[[4000,4000,0],[4000,4000,0]]
                    oom=re.findall(rb'Page text OOM: prefixes=32 failures=\s*(\d+)',data);assert len(oom)==1 and 0<int(oom[0])<32
                    report['heap'].append(dict(mode=kind,rounds=1000,queryCalls=1000,language=values[0],native=values[1],consumer='sum21 byte lengths per round; content assertions outside window'));report['budgets'].append(dict(mode=kind,prefixes=32,failures=int(oom[0])))
                save()
        assert len(report['rows'])==30 and len(report['readbacks'])==3 and len({v['sha256']for v in report['readbacks']})==1
        report.update(complete=True,inputsUnchanged=all(sha(f)==v for f,v in inputs.items()));report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
