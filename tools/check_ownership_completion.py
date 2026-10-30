"""Canonical XRT completion descriptors through the language ownership plan.

Checks actual native cycles, preparation, physical pins and old plan regressions
against a verified complete source build. Optional Linux sanitizers instrument
all C in these focused programs, including the real XRT implementation. This
does not admit language TaskGroup graphs or certify shipping/full53 products.
"""
from pathlib import Path
import argparse,json,os,shutil,subprocess,sys
from xlir_public_build_inputs import sha,verify_public_build

PROGRAMS=(
    ('native-finish','xlir_ownership_native_finish_runtime','xl6_ownership_plan',
        'Native completion ABI: 200 physical cycles, 800 BUSY turns, 100 diagnostic-free failures, 100 restored registrations; exact native descriptors, all pins retained, zero live delta'),
    ('finish','xlir_ownership_finish_runtime','xl6_ownership_plan',
        'Ownership completion: 501 real cycles, 900 BUSY turns, 200 diagnostic-free failures, 100 invalid outcomes, 400 pre-trace descriptor refusals, 2 rejected reentries; exact pins, no repeated completed duty, zero live delta'),
    ('preparation','xlir_ownership_preparation_runtime','xl6_ownership_plan',
        'Ownership preparation: 150 actual cycles, 600 pre-trace descriptor refusals, 150 busy paths, 50 explicit preparation failures, 25 resurrections, 50 new preparation duties, 200 notifications before 300 finalizers, 200 cross-thread freezes and zero live delta'),
    ('plan','xlir_ownership_plan_runtime','xl6_ownership_plan',
        'Ownership plans: 400 physical cycles across commit/resurrection/new-node/finalizer-error paths, 100 busy and 100 admission refusals, 140 real preparation faults; 1632 allocations retired, zero live delta; adapter engine, not module acceptance'),
    ('value','xlir_ownership_value_plan_runtime','xl6_ownership_plan',
        'Value ownership plans: 1600 physical Array/IntMap/Object/Set COW transactions, 40000 concurrent quarantined promotions, 84 preparation/refresh/commit faults, 1696 real native owners retired; zero live delta; custom policies remain refused'),
    ('tail','xlir_code_lease_retirement_tail','xl6_code_lease',
        'Code lease retirement tails: 100 direct and 100 scoped final releases, exclusive graph freezes and native worker joins, exact diagnostic and allocator balance'))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--tcc',type=Path)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve()
    assert not out.exists() and not (os.name=='nt' and args.sanitize)
    assert not args.tcc or (os.name=='nt' and not args.sanitize)
    build,pinned,compared=verify_public_build(root,args.public_directory)
    cc=shutil.which('gcc');assert cc
    native=[Path(row['Path']) for row in build['Objects'] if Path(row['Path']).stem=='xrt_single_header_impl']
    assert len(native)==1
    pinned.update((Path(__file__).resolve(),Path(sys.executable),Path(cc),native[0],
        root/'tools/xrt_single_header_impl.c',root/'src/runtime/xl6_ownership_plan.h',root/'src/runtime/xl6_code_lease.h',
        root/'test/support/ownership_test_thread.h'))
    for _,fixture,runtime,_ in PROGRAMS:
        pinned.update((root/'test/cases'/(fixture+'.c'),root/'src/runtime'/(runtime+'.c')))
    if args.tcc:pinned.update((args.tcc.resolve(),Path('C:/Windows/System32/ucrtbase.dll')))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir(parents=True);rows=[];products=[];env=dict(os.environ)
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,
        ComparedSources=compared,CanonicalCompletionAcceptance=False,PlanRegressionAcceptance=False,
        AllFocusedProvidersInstrumented=args.sanitize,TaskGroupGraphAcceptance=False,
        ShippingLibraryAcceptance=False,Full53Acceptance=False,ContinuationAccepted=False,ContinuationNegativeAccepted=False,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root)]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    san=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if args.sanitize else []
    if args.sanitize:env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
        UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name,command,reject=False,refusal=None):
        command=list(map(str,command));process=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=180)
        data=process.stdout+process.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=process.returncode,ExpectedRejection=reject,Log=str(log),LogSha256=sha(log)))
        print(name+': '+str(process.returncode),flush=True)
        if reject:
            assert process.returncode!=0 and refusal and refusal.encode() in data,(name,'not the actual continuation assertion')
        else:assert process.returncode==0,(name,data[-5000:].decode(errors='replace'))
        return data.decode(errors='replace').replace('\r\n','\n')
    try:
        if args.sanitize:
            instrumented=out/'xrt.instrumented.o'
            run('xrt.instrumented.build',[cc,*flags,*san,'-O1','-c',root/'tools/xrt_single_header_impl.c','-o',instrumented])
            native=[instrumented];products.append(dict(Path=str(instrumented),Sha256=sha(instrumented)))
        for lane in ('O0','O2',*(('tcc',) if args.tcc else ())):
            for name,fixture,runtime,marker in PROGRAMS:
                target=out/(name+'.'+lane+('.exe' if os.name=='nt' else ''))
                if lane=='tcc':
                    command=[args.tcc,'-std=c11','-Wall','-Werror','-I'+str(root),
                        root/'test/cases'/(fixture+'.c'),root/'src/runtime'/(runtime+'.c'),
                        root/'tools/xrt_single_header_impl.c','C:/Windows/System32/ucrtbase.dll',*systems,'-o',target]
                else:
                    command=[cc,*flags,*san,'-'+lane,root/'test/cases'/(fixture+'.c'),
                        root/'src/runtime'/(runtime+'.c'),*native,*systems,'-o',target]
                run(name+'.'+lane+'.build',command)
                output=run(name+'.'+lane+'.run',[target]);assert output.count(marker)==1,(name,output)
                if name=='preparation':assert '25 real allocation failures after completed notification' in output
                if name=='plan':assert 'Ownership preparation continuation: 96 real self-cycles' in output
                products.append(dict(Path=str(target),Sha256=sha(target)))
        # Restore the old snapshot-BUSY early return. This must compile, then
        # strand the already-started phase-1 continuation at its second poll.
        body=(root/'src/runtime/xl6_ownership_plan.c').read_text(encoding='utf8')
        variants=(('read-before-continuation','        bool continuing=result==XL6_OWNERSHIP_PLAN_BUSY;',
            '        bool continuing=false;\n        if(result==XL6_OWNERSHIP_PLAN_BUSY){freeze_end(&freeze);return XL6_OWNERSHIP_STEP_BUSY;}',
            'root->preparationPhase==2&&root->preparationCalls==2'),
            ('unstarted-duty-on-busy','(!continuing||node->preparationStarted)','true','root->preparationCalls==0'))
        for name,old,new,proof in variants:
            assert body.count(old)==1
            mutant=out/('bad-'+name+'.c');mutant.write_text(body.replace(old,new,1),encoding='utf8');products.append(dict(Path=str(mutant),Sha256=sha(mutant)))
            target=out/('bad-'+name+('.exe' if os.name=='nt' else ''))
            run('bad-'+name+'.build',[cc,*flags,*san,'-O0','-I'+str(root/'src/runtime'),root/'test/cases/xlir_ownership_plan_runtime.c',mutant,*native,*systems,'-o',target])
            products.append(dict(Path=str(target),Sha256=sha(target)))
            run('bad-'+name+'.refused',[target],True,proof)
        report.update(CanonicalCompletionAcceptance=True,PlanRegressionAcceptance=True,ContinuationAccepted=True,ContinuationNegativeAccepted=True,Complete=True)
    finally:
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['InputsUnchanged']

if __name__=='__main__':main()
