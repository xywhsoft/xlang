"""Reject actual generated-C regressions in bound vectors and class priority.
Requires a completed language product gate; no full53/shipping certification.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys
from xlir_public_build_inputs import verify_public_build

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','language-directory','out'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); root=args.root.resolve(); public=args.public_directory.resolve()
    language=args.language_directory.resolve(); out=args.out.resolve(); assert not out.exists()
    build,pinned,compared=verify_public_build(root,public)
    proof=language/'results.json'; prior=json.loads(proof.read_text(encoding='utf-8-sig'))
    assert prior['Complete'] and prior['InputsUnchanged'] and prior['DynamicMethodProductAcceptance']
    source=language/'stdlib_dynamic_method_products.c'
    assert any(row['Path']==str(source) and row['Sha256']==sha(source) for row in prior['Products'])
    cc=shutil.which('gcc'); assert cc
    host=root/'test/cases/xlir_module_product_oom_host.c'; memory=root/'test/support/module_product_memory.c'
    native=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    pinned.update((Path(__file__).resolve(),Path(cc),proof,source,host,memory,native))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir(); rows=[]; mutations=[]; env=dict(os.environ); suffix='.exe' if os.name=='nt' else ''
    if os.name=='nt':shutil.copy2(native,out/'xl.dll')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-O0','-DXL_STATIC_EMBED',
        '-I'+str(root),'-I'+str(root/'release/include'),'-I'+str(public/'include'),
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    links=[row['Path'] for row in build['Objects'] if Path(row['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    links+=[str(native),*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(native.parent)])]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Mutations=mutations,
        Full53Acceptance=False,ShippingLibraryAcceptance=False,Scope=__doc__,ComparedSources=compared)
    original=source.read_text(encoding='utf-8')
    bound='        frame->KeywordNames, owned != NULL ? owned + frame->ArgumentCount : NULL, callResult);'
    # Retain first, destroy all original slots, then expose only first. This
    # is the precise lossy legacy implementation, not a failed compile stub.
    truncate=bound+'\n    if (result && xrtCallResultCount(callResult)>1) { xvalue* first=xrtValueRetain(xrtCallResultGet(callResult,0)); xrtCallResultClear(callResult); result=xrtCallResultSetTake(callResult,0,&first); xrtValueRelease(first); }'
    changes=[('bound-truncates-vector',bound,truncate),
        ('native-before-class','if(receiver == NULL || xrtValueType(receiver) != 12)', 'if(receiver != NULL)')]
    try:
        memoryObject=out/'memory.o'
        result=subprocess.run([cc,*flags,'-c',str(memory),'-o',str(memoryObject)],cwd=root,env=env,capture_output=True,timeout=240)
        (out/'memory.build.log').write_bytes(result.stdout+result.stderr); rows.append(dict(Name='memory.build',Exit=result.returncode))
        assert result.returncode==0,(result.stdout+result.stderr)[-3000:]
        for name,before,after in changes:
            assert original.count(before)>=1,(name,before)
            changed=original.replace(before,after); path=out/(name+'.c'); path.write_text(changed,encoding='utf-8')
            binary=out/(name+suffix)
            command=[cc,*flags,'-include',path,host,memoryObject,*links,'-o',binary]
            result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=240)
            (out/(name+'.build.log')).write_bytes(result.stdout+result.stderr); rows.append(dict(Name=name+'.build',Exit=result.returncode))
            assert result.returncode==0,(name,(result.stdout+result.stderr)[-3000:])
            result=subprocess.run([str(binary)],cwd=root,env=env,capture_output=True,timeout=240)
            data=result.stdout+result.stderr; (out/(name+'.reject.log')).write_bytes(data)
            rows.append(dict(Name=name+'.reject',Exit=result.returncode,ExpectedRejection=True))
            assert result.returncode!=0 and b'xlir_module_product_oom_host.c' in data and b'assert' in data.lower(),name
            mutations.append(dict(Name=name,Sha256=sha(path),Rejected=True)); print(name+' rejected',flush=True)
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs); assert report['InputsUnchanged']
        report['Complete']=True
    except Exception as error:report['Error']=repr(error); print(error,file=sys.stderr)
    finally:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
