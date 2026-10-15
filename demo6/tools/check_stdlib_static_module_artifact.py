"""Exact selected resident/XRT closure for the static-module native foundation.

The production catalog emits the implementation and source list; the original
native graph host runs in relocated independent executables. MEMORY_DEBUG is
an explicitly added test oracle. This is not generated-language AOT acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
from xlir_public_build_inputs import verify_public_build

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve()
    assert not out.exists();build,pinned,compared=verify_public_build(root,args.public_directory)
    windows=os.name=='nt';cc=shutil.which('gcc');inspect=shutil.which('objdump' if windows else 'readelf')
    assert cc and inspect
    helper=root/'test/support/static_module_artifact.c';host=root/'test/cases/stdlib_static_module_native_host.c'
    native=root/'release/xl.dll' if windows else Path(build['NativeRoot'])/'release/libxl.so'
    pinned.update((Path(__file__).resolve(),Path(sys.executable).resolve(),Path(cc),Path(inspect),helper,host,native))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir(parents=True);relocated=out/'relocated';relocated.mkdir();rows=[];prefixes=[]
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),
        '-I'+str(root/'release'),'-I'+str(root/'release/include'),
        '-I'+str(root/'lib/xruntime'),'-I'+str(root/'lib/xrt')]
    libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-pthread','-ldl','-lm']
    if not windows:flags+=['-D_GNU_SOURCE']
    suffix='.exe' if windows else ''
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,
        ComparedSources=compared,Scope=__doc__,NativeClosureAcceptance=False,
        GeneratedEntryAcceptance=False,AotGraphRetirementAcceptance=False,Full53Acceptance=False,
        ShippingLibraryAcceptance=False,TestOnlyNativeModules=['XRT_MODULE_MEMORY_DEBUG'])
    def run(name,command,directory=root):
        command=list(map(str,command));start=time.monotonic()
        try:
            process=subprocess.run(command,cwd=directory,capture_output=True,timeout=300)
            data,code=process.stdout+process.stderr,process.returncode
        except subprocess.TimeoutExpired as error:data,code=(error.stdout or b'')+(error.stderr or b''),'timeout'
        log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=code,Seconds=time.monotonic()-start,Log=str(log),LogSha256=sha(log)))
        assert code==0,(name,data[-6000:].decode(errors='replace'));return data
    try:
        objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).stem not in
            ('xlir_public_source_driver','xlir_public_source_linux_driver')]
        if windows:shutil.copy2(native,out/'xl.dll')
        links=[native,*libraries] if windows else [native,*libraries,'-Wl,-rpath,'+str(native.parent),'-Wl,--export-dynamic']
        producer=out/('producer'+suffix)
        run('producer.build',[cc,*flags,'-O0',helper,*objects,*links,'-o',producer])
        prefix=out/'selected';run('producer.run',[producer,prefix])
        plan=json.loads((out/'selected.plan.json').read_text());report['Plan']=plan
        required={'src/runtime/xl6_static_module.c','src/runtime/xl6_code_lease.c',
            'src/runtime/xl6_code_finalizer.c','src/runtime/xl6_ownership_plan.c',
            'src/runtime/xl6_sequence.c','src/runtime/xl6_stack.c','src/api/xl_foreign_error.c'}
        assert set(plan['Sources'])==required and len(plan['Sources'])==len(required),plan
        assert {'XRT_MODULE_MUTEX','XRT_MODULE_VALUE_GRAPH'}<=set(plan['NativeModules'])
        assert not any(any(word in name for word in ('NETWORK','TLS','PROCESS','REGEX','LOGGER')) for name in plan['NativeModules'])
        for opt in ('O0','O2'):
            image=out/(opt+suffix)
            run(opt+'.build',[cc,*flags,'-'+opt,'-DXRT_MODULE_MEMORY_DEBUG',host,out/'selected.xrt.c',
                *[root/name for name in plan['Sources']],*libraries,'-o',image])
            independent=relocated/image.name;shutil.copy2(image,independent)
            data=run(opt+'.imports',[inspect,'-p' if windows else '-d',independent],relocated)
            deps=re.findall(rb'DLL Name:\s*([^\s]+)',data) if windows else re.findall(rb'Shared library:\s*\[([^\]]+)\]',data)
            assert deps and not {item.decode().lower() for item in deps}&{'xl.dll','xlc.dll','libxl.so','libxlc.so'}
            if not windows:assert b'(RPATH)' not in data and b'(RUNPATH)' not in data
            report.setdefault('IndependentProducts',[]).append(dict(Path=str(independent),Dependencies=[item.decode() for item in deps]))
            for mode in range(17):
                data=run(opt+'.mode'+str(mode),[independent,str(mode)],relocated)
                assert b'once-only destructor, checked recovery balanced' in data
            data=run(opt+'.oom',[independent,'--oom'],relocated)
            match=re.search(rb'full prefixes=(\d+) first-no-hit=(\d+) balanced',data);assert match
            runs,first=map(int,match.groups());assert runs==first+1 and first>0
            prefixes.append(dict(Optimization=opt,Runs=runs,FirstNoHit=first))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);assert report['InputsUnchanged']
        report.update(Complete=True,NativeClosureAcceptance=True,PhaseCount=len(rows))
    except Exception as error:report['Error']=repr(error);print(repr(error),flush=True)
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Static-module exact native closure phases: '+str(len(rows))+' complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
