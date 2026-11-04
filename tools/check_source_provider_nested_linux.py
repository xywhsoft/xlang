"""Source-free nested compiler JIT/module/AOT against a verified source/provider cohort.

The executable is the exact staged full-source driver. Adjacent SDK copies
contain declarations only, no compiler .c/.o files. Standalone outputs are
relocated and checked for accidental dynamic xlang/TinyCC dependencies.
Native Linux system CRT, libc headers and linker inputs remain required.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from xlir_public_build_inputs import verify_public_build
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,public,out=[v.resolve() for v in (a.root,a.public_directory,a.out)]
    assert os.name!='nt';build,pinned,_=verify_public_build(root,public)
    provider_report=public/'provider.json';provider=json.loads(provider_report.read_text())
    assert provider['complete'] and provider['passed'] and provider['mode']=='full'
    cases=[root/'test/cases'/name for name in ('stdlib_nested_sdk.xl','stdlib_nested_aot.xl','stdlib_string_bridge_boundaries.xl','stdlib_tcc_embed.xl')]
    pinned.update([Path(__file__).resolve(),provider_report,Path(build['TccCore']),*cases]);sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    inputs={str(path):sha(path) for path in pinned};out.mkdir(parents=True,exist_ok=False)
    driver=out/'public_source';shutil.copy2(build['Driver'],driver)
    headers=out/'include'
    for path in (public/'include').rglob('*'):
        if path.is_file() and path.suffix in ('.h','.inc','.def'):
            target=headers/path.relative_to(public/'include');target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target)
    assert not any(p.suffix in ('.c','.o') for p in headers.rglob('*') if p.is_file())
    work=out/'empty-work';work.mkdir();(work/'build/test').mkdir(parents=True)
    relocated=out/'relocated';relocated.mkdir();(relocated/'build/test').mkdir(parents=True)
    env={key:value for key,value in os.environ.items() if not key.startswith('XL6_') and key not in
         ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(XL6_SOURCE_NATIVE_ROOT=build['NativeRoot'],XL6_TCC_DLL=build['TccCore'],XL6_NESTED_SDK_WORK=str(work),
        LD_LIBRARY_PATH=str(Path(build['TccCore']).parent),TMPDIR=str(out),TEMP=str(out),TMP=str(out))
    report=dict(scope=__doc__,complete=False,passed=False,inputs=inputs,rows=[],sourceFreeSdk=True,shippingAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,cwd=work):
        start=time.monotonic();r=subprocess.run(list(map(str,command)),cwd=cwd,env=env,capture_output=True,timeout=360)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=list(map(str,command)),exitCode=r.returncode,
            seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save()
        print(name,r.returncode,flush=True);assert not r.returncode,log;return r.stdout
    for case in cases:
        local=out/case.name;shutil.copy2(case,local)
        for mode in ('run','module','exe'):
            output=out/(case.stem+'-'+mode)
            run(case.stem+'-'+mode,[driver,local,mode,output])
            if mode=='exe':
                dependencies=run(case.stem+'-dependencies',['readelf','-d',output]).decode()
                assert not any(name in dependencies for name in ('libxlc.so','libxl.so','libtcc.so'))
                copy=relocated/output.name;shutil.copy2(output,copy);run(case.stem+'-relocated',[copy],relocated)
    report.update(complete=True,passed=all(sha(path)==value for path,value in inputs.items()));save();assert report['passed']
if __name__=='__main__':main()
