"""Source-free nested compiler JIT/module/AOT against a verified source/provider cohort.

The executable is the exact staged full-source driver or an explicit overlay.
The provider remains a verified full source/provider cohort. Adjacent SDK copies
contain headers only, including required XRT implementation products; no loose compiler .c/.o files. Standalone outputs are
relocated and checked for accidental dynamic xlang/TinyCC dependencies.
Native Linux system CRT, libc headers and linker inputs remain required.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from foreign_validation_build import load_build
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--overlay',type=Path,help='Explicit partial compiler overlay; provider objects remain separately qualified')
    a=p.parse_args();root,public,out=[v.resolve() for v in (a.root,a.public_directory,a.out)]
    assert os.name!='nt'
    selected=load_build(None,a.overlay) if a.overlay else load_build(public/'build.json',None)
    provider_report=public/'provider.json';provider=json.loads(provider_report.read_text())
    assert provider['complete'] and provider['passed'] and provider['mode']=='full'
    cases=[root/'test/cases'/name for name in ('stdlib_nested_sdk.xl','stdlib_nested_aot.xl','stdlib_string_bridge_boundaries.xl','stdlib_tcc_embed.xl','native_sequence.xl')]
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    inputs=dict(selected['inputs']);inputs.update((str(path),sha(path)) for path in
        [Path(__file__).resolve(),provider_report,public/'build.json',selected['runtime'],selected['driver'],*cases])
    out.mkdir(parents=True,exist_ok=False)
    driver=out/'public_source';shutil.copy2(selected['driver'],driver)
    headers=out/'include'
    for path in (public/'include').rglob('*'):
        if path.is_file() and path.suffix in ('.h','.inc','.def'):
            target=headers/path.relative_to(public/'include');target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target)
            inputs[str(path)]=sha(path)
    assert not any(p.suffix in ('.c','.o') for p in headers.rglob('*') if p.is_file())
    inner=('auto values=NativeSequence::<c_int>()\nassert values.length==0\n'
        'for value in values\n throw "unexpected"\nend\nvar erased=values\n'
        'assert (erased as NativeSequence<c_int>).count()==0\n')
    nested=out/'nested-native-sequence.xl'
    nested.write_text('import xlang\nauto context=xlang.Context()\nassert context.run('
        +json.dumps(inner)+'), context.lastError\nprintln("nested NativeSequence: PASS")\n',encoding='utf-8')
    cases.append(nested);inputs[str(nested)]=sha(nested)
    second=out/'double-nested-compiler.xl'
    middle=('import xlang\nauto innerContext=xlang.Context()\nassert innerContext.run('
        +json.dumps(inner)+'), innerContext.lastError\n')
    second.write_text('import xlang\nauto context=xlang.Context()\nassert context.run('
        +json.dumps(middle)+'), context.lastError\nprintln("double nested compiler: PASS")\n',encoding='utf-8')
    cases.append(second);inputs[str(second)]=sha(second)
    sidecars={}
    for name in ('foreign_output_record_size.xl',):
        fixture=root/'test/cases'/name
        native=root/'test/cases'/(fixture.stem+'_native.c')
        header=root/'test/cases'/(fixture.stem+'.h')
        local=out/fixture.name;sidecar=out/native.name;target=headers/'test/cases'/header.name
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(fixture,local);shutil.copy2(native,sidecar);shutil.copy2(header,target)
        cases.append(local);sidecars[local.name]=[sidecar]
        inputs.update((str(path),sha(path)) for path in (fixture,native,header))
    work=out/'empty-work';work.mkdir();(work/'build/test').mkdir(parents=True)
    relocated=out/'relocated';relocated.mkdir();(relocated/'build/test').mkdir(parents=True)
    env={key:value for key,value in os.environ.items() if not key.startswith('XL6_') and key not in
         ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(PYTHONDONTWRITEBYTECODE='1',XL6_SOURCE_NATIVE_ROOT=str(public),XL6_TCC_DLL=str(selected['runtime']),XL6_NESTED_SDK_WORK=str(work),
        LD_LIBRARY_PATH=str(selected['runtime'].parent),TMPDIR=str(out),TEMP=str(out),TMP=str(out))
    report=dict(scope=__doc__,complete=False,passed=False,inputs=inputs,rows=[],sourceFreeSdk=True,shippingAccepted=False,
        compilerBuild=selected['buildManifest'],compilerOverlay=selected['overlay'],fullSourceRebuild=selected['fullSourceRebuild'],
        providerDirectory=str(public),providerObjectsRebuiltByThisGate=False,
        nativeTestSidecars=[str(path) for values in sidecars.values() for path in values])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,cwd=work):
        start=time.monotonic();r=subprocess.run(list(map(str,command)),cwd=cwd,env=env,capture_output=True,timeout=360)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=list(map(str,command)),exitCode=r.returncode,
            seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save()
        print(name,r.returncode,flush=True);return r.returncode==0,r.stdout
    for case in cases:
        local=out/case.name
        if local!=case:shutil.copy2(case,local)
        for mode in ('run','module','exe'):
            output=out/(case.stem+'-'+mode)
            ok,_=run(case.stem+'-'+mode,[driver,local,mode,output,*sidecars.get(local.name,[])])
            if ok and mode=='exe':
                _,dependencies=run(case.stem+'-dependencies',['readelf','-d',output]);dependencies=dependencies.decode()
                assert not any(name in dependencies for name in ('libxlc.so','libxl.so','libtcc.so'))
                copy=relocated/output.name;shutil.copy2(output,copy);run(case.stem+'-relocated',[copy],relocated)
    report['inputsUnchanged']=all(sha(path)==value for path,value in inputs.items())
    report.update(complete=True,passed=report['inputsUnchanged'] and all(row['exitCode']==0 for row in report['rows']))
    save();assert report['passed']
if __name__=='__main__':main()
