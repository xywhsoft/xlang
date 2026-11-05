"""Windows source-free JIT/module/AOT with a real full or explicit successor provider.

The copied SDK contains headers, including required XRT implementation products. The outer diagnostic compiler uses
an explicit frozen standard C distribution. Relocated standalone programs have
no SDK, provider DLL or distribution beside them, and run with XL6 overrides
removed. Includes true double-nested compiler and record+size output fixtures.
The latter supplies its declared test-only native C sidecar, never compiler C.
This does not qualify Native UI, devices, graphics, browser or shipping delivery.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_validation_build import load_build

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--overlay',type=Path,help='Explicit compiler/provider successor overlay')
    args=parser.parse_args();root,public,out=[p.resolve()for p in (args.root,args.public_directory,args.out)]
    assert os.name=='nt'
    selected=load_build(None,args.overlay) if args.overlay else load_build(public/'build.json',None)
    report_path=public/'provider.json';provider_report=json.loads(report_path.read_text())
    assert provider_report['complete'] and provider_report['passed']
    provider=public/'xlc.dll';assert provider.is_file()
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    assert sha(provider)==provider_report['providerSha256']
    out.mkdir(parents=True,exist_ok=False)
    pins=[Path(__file__).resolve(),report_path,provider,selected['driver'],selected['runtime']]
    for source,name in ((selected['driver'],'public_source.exe'),(selected['runtime'],selected['runtime'].name),(provider,'xlc.dll')):
        shutil.copy2(source,out/name)
    for path in (public/'include').rglob('*'):
        if path.is_file() and path.suffix in ('.h','.inc','.def'):
            target=out/'include'/path.relative_to(public/'include');target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(path,target);pins.append(path)
    for path in (root/'release/win_x64/include').glob('*.h'):
        shutil.copy2(path,out/'include'/path.name);pins.append(path)
    work=out/'empty-work';work.mkdir();(work/'build/test').mkdir(parents=True)
    dist=root/'res/tcc/win64/x86_64';shutil.copytree(dist,work/'res/tcc/win64/x86_64')
    pins.extend(path for path in dist.rglob('*')if path.is_file())
    relocated=out/'relocated';relocated.mkdir();(relocated/'build/test').mkdir(parents=True)
    cases=[]
    for name in ('stdlib_nested_sdk.xl','stdlib_nested_aot.xl','stdlib_string_bridge_boundaries.xl','stdlib_tcc_embed.xl','native_sequence.xl'):
        source=root/'test/cases'/name;local=out/name;shutil.copy2(source,local);pins.append(source);cases.append((local,[]))
    inner=('auto values=NativeSequence::<c_int>()\nassert values.length==0\n'
        'for value in values\n throw "unexpected"\nend\nvar erased=values\n'
        'assert (erased as NativeSequence<c_int>).count()==0\n')
    middle='import xlang\nauto nested=xlang.Context()\nassert nested.run('+json.dumps(inner)+'), nested.lastError\n'
    for name,text in (('nested-native-sequence',inner),('double-nested-compiler',middle)):
        source=out/(name+'.xl');source.write_text('import xlang\nauto context=xlang.Context()\nassert context.run('
            +json.dumps(text)+'), context.lastError\nprintln("'+name+': PASS")\n',encoding='utf-8')
        pins.append(source);cases.append((source,[]))
    fixture=root/'test/cases/foreign_output_record_size.xl'
    native=root/'test/cases/foreign_output_record_size_native.c'
    header=root/'test/cases/foreign_output_record_size.h'
    local=out/fixture.name;sidecar=out/native.name;target=out/'include/test/cases'/header.name
    target.parent.mkdir(parents=True);shutil.copy2(header,target);shutil.copy2(fixture,local);shutil.copy2(native,sidecar)
    cases.append((local,[sidecar]));pins.extend((fixture,native,header))
    assert not any(p.suffix in ('.c','.o')for p in (out/'include').rglob('*')if p.is_file())
    inputs=dict(selected['inputs']);inputs.update((str(p),sha(p))for p in pins)
    env={k:v for k,v in os.environ.items()if not k.upper().startswith('XL6_')and k not in ('CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    standalone_env=dict(env,PYTHONDONTWRITEBYTECODE='1',TEMP=str(out),TMP=str(out))
    env.update(XL6_SOURCE_NATIVE_ROOT=str(work),XL6_TCC_DLL=str(out/'xlc.dll'),XL6_NESTED_SDK_WORK=str(work),
        PYTHONDONTWRITEBYTECODE='1',TEMP=str(out),TMP=str(out))
    report=dict(complete=False,passed=False,scope=__doc__,compilerBuild=selected['buildManifest'],compilerOverlay=selected['overlay'],
        fullSourceRebuild=selected['fullSourceRebuild'],providerObjectsRebuiltByThisGate=False,sourceFreeSdk=True,
        externalStandardDistribution=str(dist),nativeTestSidecars=[str(sidecar)],shippingDelivery=False,inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,cwd=work,environment=env):
        started=time.monotonic();result=subprocess.run(list(map(str,command)),cwd=cwd,env=environment,capture_output=True,timeout=360)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name,command=list(map(str,command)),exitCode=result.returncode,
            seconds=round(time.monotonic()-started,3),logSha256=sha(log)));save()
        print(name,result.returncode,flush=True);assert result.returncode==0,log
        return result.stdout
    objdump=shutil.which('objdump');assert objdump
    for source,sidecars in cases:
        for mode in ('run','module','exe'):
            prefix=out/(source.stem+'-'+mode)
            run(source.stem+'-'+mode,[out/'public_source.exe',source,mode,prefix,*sidecars])
            if mode=='exe':
                program=prefix.with_suffix('.exe')
                dependencies=run(source.stem+'-dependencies',[objdump,'-p',program]).decode(errors='replace')
                names={name.lower()for name in re.findall(r'DLL Name:\s*(\S+)',dependencies)}
                assert not names.intersection(('xl.dll','xlc.dll','libtcc.dll')),names
                target=relocated/program.name;shutil.copy2(program,target)
                run(source.stem+'-relocated',[target],relocated,standalone_env)
    report.update(complete=True,inputsUnchanged=all(sha(p)==value for p,value in inputs.items()))
    report['passed']=report['inputsUnchanged'];save();assert report['passed']

if __name__=='__main__':main()
