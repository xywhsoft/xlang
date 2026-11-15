"""Independent native source-patch semantics and real allocator-failure prefixes.

Owned default models and raw custom-allocator diagnostics are kept distinct.
No language custom-allocator support, graphics, network or shipping is implied.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','compiler-report','native-root','engine','static-sdk','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,compiler,build,native,engine,static,out=[v.resolve() for v in
        (a.root,a.compiler,a.compiler_report,a.native_root,a.engine,a.static_sdk,a.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    runtime=compiler.parent/('xlc.dll' if win else 'libxlc.so');cc=Path(shutil.which('gcc')).resolve()
    hr=engine/'build-report.json';sr=static/'build-report.json';h=json.loads(hr.read_text());s=json.loads(sr.read_text())
    assert h['linked'] and h['xrt_mode']=='host' and s['passed'] and s['xrt_mode']=='direct-external'
    image=engine/h['runtime_image'];library=engine/h['import_library'] if win else image;archive=static/s['archive']
    case=root/'test/cases/foreign_xui_document_source_native.c'
    files=[Path(__file__).resolve(),compiler,build,runtime,cc,hr,sr,image,library,archive,case,native/'tests/xui/xui_code_margin_host.h',
        native/'include/xui_document.h',native/'src/xui/document/xui_document_prepare.c',native/'src/xui/document/xui_document_markdown.c',*extra,
        Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py')]
    for folder in (sdk,engine,static):files.extend(f for f in folder.rglob('*') if f.is_file())
    inputs={str(f):sha(f) for f in files};out.mkdir(parents=True,exist_ok=False);scratch=out/'tmp';scratch.mkdir()
    shutil.copy2(case,out/case.name)
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,inputs=inputs,rows=[],native=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    env={k:v for k,v in os.environ.items() if not k.startswith('XL6_') and k!='XLANG_PACKAGE_PATH'}
    env.update(TEMP=str(scratch),TMP=str(scratch),TMPDIR=str(scratch));key='PATH' if win else 'LD_LIBRARY_PATH'
    env[key]=os.pathsep.join((str(engine),str(runtime.parent),env.get(key,'')))
    def run(name,command):
        command=list(map(str,command));start=time.monotonic()
        z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert z.returncode==0,log;return z.stdout+z.stderr
    try:
        for kind,folder,lib in (('host',engine,library),('static',static,archive)):
            flags=['-Wall','-Wextra','-Werror','-Wno-unused-function','-DXGE_XRT_HOST=1','-include',folder/'xge_build_config.h',
                *['-I'+str(v) for v in (native,native/'include',native/'src/xui/core',folder,engine/'host-xrt')]]
            flags+=['-DXGE_TEST_STATIC=1'] if kind=='static' else (['-DXGE_DLL=1','-DXUI_DLL=1'] if win else [])
            systems=['-l'+v for v in s['system_libraries']] if kind=='static' else []
            if not win:systems+=['-ldl','-lm','-pthread','-Wl,-rpath,'+str(engine),'-Wl,-rpath,'+str(runtime.parent)]
            for opt in ('O0','O2'):
                label=kind+'-'+opt;binary=out/(label+('.exe' if win else ''))
                run(label+'-build',[cc,'-std=c11','-'+opt,*flags,case,lib,runtime,*systems,'-o',binary])
                output=run(label+'-run',[binary]);assert b'source copy, candidate retention, retry, metadata and one-step undo passed' in output
                counts=re.search(rb'allocation prefixes=(\d+), failures=(\d+), diagnostic allocator payloads balanced',output)
                assert counts and int(counts[2])>0
                report['native'].append(dict(kind=kind,optimization=opt,prefixes=int(counts[1]),failures=int(counts[2]),
                    allocator='Raw custom allocator; not language Owned provenance',binary=str(binary),sha256=sha(binary)))
        report.update(complete=True,inputsUnchanged=all(sha(f)==digest for f,digest in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
