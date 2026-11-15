"""Prove the existing Document stream continuation binding gap against native C.

The language witness intentionally reproduces a checked failure; a green run
does NOT mean the product is fixed. Native C exercises the valid NULL-descriptor
continuation, split UTF-8, worker Run, undo/redo and custom-allocator failures.
No new language rules, graphics, network or release installation.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','compiler','compiler-report','native-root','native-qualification',
              'engine','static-sdk','dynamic-packages','static-packages','out'):
        p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();root,compiler,build,native,qualification,engine,static_sdk,dynamic,static,out=[v.resolve()for v in
        (a.root,a.compiler,a.compiler_report,a.native_root,a.native_qualification,a.engine,a.static_sdk,a.dynamic_packages,a.static_packages,a.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt'
    sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest()
    q=json.loads(qualification.read_text(encoding='utf-8'));assert q['complete']and q['passed']
    proof=Path(q['acceptance']['path']);assert sha(proof)==q['acceptance']['sha256']
    original=json.loads(proof.read_text(encoding='utf-8'));assert original['complete']and original['passed']
    hr=engine/'build-report.json';sr=static_sdk/'build-report.json'
    h=json.loads(hr.read_text(encoding='utf-8'));s=json.loads(sr.read_text(encoding='utf-8'))
    assert h['linked']and h['xrt_mode']=='host'and s['passed']and s['xrt_mode']=='direct-external'
    image=engine/h['runtime_image'];library=engine/h['import_library']if win else image;archive=static_sdk/s['archive']
    runtime=compiler.parent/('xlc.dll'if win else'libxlc.so')
    for f in (image,library,archive,runtime,hr,sr):assert sha(f)==original['inputs'][str(f)]
    tools={n:Path(shutil.which(n)).resolve()for n in ('gcc','g++','objdump')}
    case=root/'test/cases/foreign_xui_document_stream_gap.xl';oracle=root/'test/cases/foreign_xui_document_stream_native.c'
    files=[Path(__file__),Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py'),
        compiler,build,qualification,proof,runtime,hr,sr,image,library,archive,case,oracle,native/'tests/xui/xui_code_margin_host.h',*tools.values(),*extra,
        native/'src/xui/document/xui_document.c',native/'src/xui/document/xui_document_prepare.c',native/'include/xui_document.h']
    for folder in (sdk,dynamic,static):files.extend(v for v in folder.rglob('*')if v.is_file())
    inputs={str(v):sha(v)for v in files};out.mkdir(parents=True,exist_ok=False)
    copies=out/'input-snapshots';copies.mkdir()
    for f in (case,oracle,Path(__file__)):shutil.copy2(f,copies/f.name)
    temp=out/'temp';temp.mkdir();report=dict(complete=False,checksPassed=False,productFixed=False,shippingDelivery=False,networkTests=False,
        scope=__doc__,inputs=inputs,rows=[],native=[],programs=[])
    env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
    env.update(TEMP=str(temp),TMP=str(temp),TMPDIR=str(temp));key='PATH'if win else'LD_LIBRARY_PATH'
    env[key]=os.pathsep.join((str(engine),str(runtime.parent),env.get(key,'')))
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def no_core():
        import resource
        resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def run(name,cmd,packages=None):
        e=dict(env)
        if packages:e['XLANG_PACKAGE_PATH']=str(packages)
        cmd=list(map(str,cmd));start=time.monotonic()
        z=subprocess.run(cmd,cwd=root,env=e,capture_output=True,timeout=240,preexec_fn=None if win else no_core)
        data=z.stdout+z.stderr;log=out/(name+'.log');log.write_bytes(data)
        report['rows'].append(dict(name=name,command=cmd,exitCode=z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert not z.returncode,log;return data
    try:
        for kind,folder,lib in (('host',engine,library),('static',static_sdk,archive)):
            flags=['-Wall','-Wextra','-Werror','-Wno-unused-function','-DXGE_XRT_HOST=1','-include',folder/'xge_build_config.h',
                *['-I'+str(v)for v in(native,native/'include',native/'src/xui/core',folder,engine/'host-xrt')]]
            flags+=['-DXGE_TEST_STATIC=1']if kind=='static'else(['-DXGE_DLL=1','-DXUI_DLL=1']if win else[])
            systems=['-l'+v for v in s['system_libraries']]if kind=='static'else[]
            if not win:systems+=['-ldl','-lm','-pthread','-Wl,-rpath,'+str(engine),'-Wl,-rpath,'+str(runtime.parent)]
            for opt in ('O0','O2'):
                label=kind+'-'+opt;binary=out/(label+('.exe'if win else''))
                run(label+'-build',[tools['gcc'],'-std=c11','-'+opt,*flags,oracle,lib,runtime,*systems,'-o',binary])
                data=run(label,[binary]);assert b'NULL continuation descriptor'in data and b'worker Run and one-step undo passed'in data
                counts=re.search(rb'allocation prefixes=(\d+), failures=(\d+), diagnostic allocator payloads balanced',data);assert counts and int(counts[2])>0
                report['native'].append(dict(kind=kind,optimization=opt,prefixes=int(counts[1]),failures=int(counts[2]),allocator='Custom diagnostic callbacks, raw provenance; owned default-path semantic cases separately executed'))
        includes=['--inc-path',root,'--inc-path',sdk]
        c=out/'document_stream_gap.c';header=out/'document_stream_gap.h'
        run('emit',[compiler,'cgen',case,'--emit',c,'--emit-header',header,*includes],dynamic)
        text=c.read_text(encoding='utf-8-sig');assert re.search(r'(?<![\w&])xuiDocumentPrepareStreamSource\(',text)
        report['generatedC']=dict(path=str(c),sha256=sha(c),note='Existing descriptor pack is non-NULL even when previous is supplied')
        ci=['-I'+str(v)for v in(root,sdk,native,native/'include',*sorted(dynamic.glob('*/include'))) ]
        for std,tool,lang in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
            run('header-'+std,[tools[tool],'-std='+std,'-Wall','-Wextra','-Werror',*ci,'-x',lang,'-fsyntax-only',header],dynamic)
        for kind,packages in (('jit',dynamic),('dynamic',dynamic),('static',static)):
            if kind=='jit':data=run(kind,[compiler,'run',case,*includes],packages)
            else:
                binary=out/(kind+('.exe'if win else''))
                run(kind+'-build',[compiler,'build',case,'-o',binary,*includes],packages);data=run(kind,[binary],packages)
                table=run(kind+'-imports',[tools['objdump'],'-p',binary],packages)
                names=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table);lower={v.lower()for v in names}
                assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                assert any(b'xge3_host'in v for v in lower)==(kind=='dynamic')
                report['programs'].append(dict(kind=kind,path=str(binary),sha256=sha(binary),imports=[v.decode()for v in names]))
            assert b'Document stream gap reproduced:'in data and b'original candidate remains publishable'in data
        assert len(report['rows'])==19 and len(report['native'])==4
        report.update(complete=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()),knownRegressionReproduced=True,nativeContinuationQualified=True)
        report['checksPassed']=report['inputsUnchanged'];assert report['checksPassed']
    except Exception as error:report.update(complete=True,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
