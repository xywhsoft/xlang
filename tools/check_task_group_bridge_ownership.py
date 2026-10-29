"""Actual TaskGroup bridge references, owned parent registration and retirement.

Full-source input verification; optional Linux ASan/UBSan instruments the
bridge, plan, fixture and entire XRT implementation used by this gate. The
closed native resolver is not automatic global language graph admission.
"""
from pathlib import Path
import argparse,json,os,shutil,subprocess,sys
from xlir_public_build_inputs import sha,verify_public_build

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true');parser.add_argument('--tcc',type=Path)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve()
    assert not out.exists() and not (args.sanitize and os.name=='nt')
    assert not args.tcc or (os.name=='nt' and not args.sanitize)
    build,pinned,compared=verify_public_build(root,args.public_directory)
    cc=shutil.which('gcc');assert cc
    native=[Path(row['Path']) for row in build['Objects'] if Path(row['Path']).stem=='xrt_single_header_impl'];assert len(native)==1
    sources=[root/'test/support/task_group_bridge_ownership.c',root/'src/api/xl_task_group_bridge.c',root/'src/runtime/xl6_ownership_plan.c']
    pinned.update((Path(__file__).resolve(),Path(sys.executable),Path(cc),native[0],*sources,
        root/'src/api/xl_task_group_bridge.h',root/'src/runtime/xl6_ownership_plan.h',root/'tools/xrt_single_header_impl.c'))
    if args.tcc:pinned.update((args.tcc.resolve(),Path('C:/Windows/System32/ucrtbase.dll')))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir(parents=True);temporary=out/'tmp';temporary.mkdir();rows=[];products=[]
    env=dict(os.environ,TMPDIR=str(temporary),TEMP=str(temporary),TMP=str(temporary))
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,ComparedSources=compared,
        BridgePhysicalRetirementAcceptance=False,AllFocusedProvidersInstrumented=args.sanitize,
        TaskGroupGraphAcceptance=False,Full53Acceptance=False,ShippingLibraryAcceptance=False,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
    libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    san=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if args.sanitize else []
    if args.sanitize:env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name,command):
        command=list(map(str,command));process=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=240)
        data=process.stdout+process.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=process.returncode,Log=str(log),LogSha256=sha(log)))
        print(name+': '+str(process.returncode),flush=True)
        assert not process.returncode,(name,data[-5000:].decode(errors='replace'))
        return data
    try:
        if args.sanitize:
            instrumented=out/'xrt.instrumented.o'
            run('xrt.instrumented.build',[cc,*flags,*san,'-O1','-c',root/'tools/xrt_single_header_impl.c','-o',instrumented])
            native=[instrumented];products.append(dict(Path=str(instrumented),Sha256=sha(instrumented)))
        for lane in ('O0','O2',*(('tcc',) if args.tcc else ())):
            target=out/('bridge.'+lane+('.exe' if os.name=='nt' else ''))
            command=[args.tcc,*flags,*sources,root/'tools/xrt_single_header_impl.c','C:/Windows/System32/ucrtbase.dll',*libraries,'-o',target] if lane=='tcc' else [cc,*flags,*san,'-'+lane,*sources,*native,*libraries,'-o',target]
            run(lane+'.build',command)
            output=run(lane+'.run',[target])
            assert b'160 real cycles, 80 restored registrations, 80 reentrant callbacks, 20 concurrent wait/destroy tails' in output
            assert b'exact physical roots and full ledger balanced' in output
            products.append(dict(Path=str(target),Sha256=sha(target)))
        report.update(Complete=True,BridgePhysicalRetirementAcceptance=True)
    finally:
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['InputsUnchanged']

if __name__=='__main__':main()
