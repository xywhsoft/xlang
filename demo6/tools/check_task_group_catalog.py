"""Exact TaskGroup resident catalog and generated language retirement tests.

Uses a verified full-source build, not object replacements. Native preparation
does not by itself certify generated catch/finally: that is an independent lane.
No shipping-library or all-53 acceptance is implied.
"""
from pathlib import Path
import argparse,json,os,re,shutil,subprocess,sys
from xlir_public_build_inputs import sha,verify_public_build

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--native-only',action='store_true')
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve();assert not out.exists()
    assert not args.sanitize or (os.name!='nt' and args.native_only)
    build,pinned,compared=verify_public_build(root,args.public_directory)
    cc=shutil.which('gcc');assert cc
    fixture=root/'test/support/task_group_catalog_ownership.c'
    host=root/'test/support/task_group_retirement_public.c'
    quiescence=root/'test/support/ownership_quiescence_native.c'
    cli=root/'src/driver/xl6_xlc_shell.c'
    language=root/'test/cases/xlir_module_task_group_retirement.xl'
    standalone=root/'test/cases/xlir_task_group_aot_retirement.xl'
    pool_language=root/'test/cases/xlir_module_task_pool_retirement.xl'
    static_quiescence=root/'test/support/static_module_quiescence.c'
    group_transient=root/'test/support/task_group_transient_admission.c'
    inspector=shutil.which('objdump' if os.name=='nt' else 'readelf');assert inspector
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    pinned.update((Path(__file__).resolve(),Path(sys.executable),Path(cc),Path(inspector),fixture,host,language,standalone,runtime,quiescence,cli,pool_language,static_quiescence,group_transient))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    objects=[Path(row['Path']) for row in build['Objects'] if Path(row['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
    libraries=[str(runtime)]
    if os.name=='nt':libraries+=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
    else:flags+=['-D_GNU_SOURCE'];libraries+=['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    out.mkdir(parents=True);temporary=out/'tmp';temporary.mkdir();rows=[];blockers=[]
    env=dict(os.environ,TMPDIR=str(temporary),TEMP=str(temporary),TMP=str(temporary))
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    tcc=root/'res/tcc/win64/x86_64' if os.name=='nt' else Path(build['NativeRoot'])/'build/linux/tcc-dist'
    env.update(XL6_TCC_DLL=str(runtime),XL6_TCC_ROOT=str(tcc))
    report=dict(Complete=False,ExecutionComplete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,KnownBlockers=blockers,OneShotAcceptance=False,ComparedSources=compared,
        CatalogPreparationAcceptance=False,GeneratedFinallyAcceptance=False,IndependentAotFinallyAcceptance=False,AllFocusedProvidersInstrumented=args.sanitize,
        Full53Acceptance=False,ShippingAcceptance=False,Scope=__doc__)
    def run(name,command,directory=None):
        command=list(map(str,command));process=subprocess.run(command,cwd=directory or root,env=env,capture_output=True,timeout=240)
        data=process.stdout+process.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=process.returncode,Log=str(log),LogSha256=sha(log)))
        print(name+': '+str(process.returncode),flush=True)
        assert not process.returncode,(name,data[-5000:].decode(errors='replace'))
        return data
    suffix='.exe' if os.name=='nt' else ''
    try:
        focused=[];san=[]
        if args.sanitize:
            san=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-ffunction-sections','-fdata-sections']
            env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            kernel=out/'xrt.instrumented.o'
            run('xrt.instrumented.build',[cc,*flags,*san,'-O1','-c',root/'tools/xrt_single_header_impl.c','-o',kernel])
            focused=[root/('src/'+name) for name in ('api/xl_task_group_bridge.c','runtime/xl6_signal_task_group.c',
                'runtime/xl6_native_handle.c','runtime/xl6_ownership_catalog.c','runtime/xl6_ownership_plan.c',
                'runtime/xl6_ownership_domain_resource_task_group.c','runtime/xl6_ownership_domain_native_future.c',
                'runtime/xl6_continuation_executor.c','runtime/xl6_continuation_queue.c','runtime/xl6_code_lease.c',
                'runtime/xl6_ownership_domain_executor.c','runtime/xl6_ownership_domain_queue.c')]
            objects=[kernel];libraries=['-pthread','-ldl','-lm','-Wl,--gc-sections']
        response=out/'objects.rsp'
        response.write_text(''.join('"'+str(path).replace('\\','/')+'"\n' for path in objects),encoding='utf-8')
        link_objects=['@'+str(response)]
        for opt in ('O0','O2'):
            target=out/('catalog.'+opt+suffix)
            run(opt+'.build',[cc,*flags,*san,'-'+opt,fixture,*focused,*link_objects,*libraries,'-o',target])
            assert b'TaskGroup catalog: 128 real handle graphs' in run(opt+'.run',[target])
            probe_target=out/('quiescence.'+opt+suffix)
            run('quiescence.'+opt+'.build',[cc,*flags,*san,'-'+opt,quiescence,*focused,*link_objects,*libraries,'-o',probe_target])
            assert b'Ownership quiescence: 64 real workers' in run('quiescence.'+opt+'.run',[probe_target])
            transient_target=out/('group-transient.'+opt+suffix)
            run('group-transient.'+opt+'.build',[cc,*flags,*san,'-'+opt,group_transient,*focused,*link_objects,*libraries,'-o',transient_target])
            assert b'TaskGroup transient admission: 80 real cancel/bind/notify windows' in run('group-transient.'+opt+'.run',[transient_target])
        report['CatalogPreparationAcceptance']=True
        report['TransientAdmissionAcceptance']=True
        report['TaskGroupNativeDispatchAcceptance']=True
        if not args.native_only:
            for opt in ('O0','O2'):
                target=out/('static-quiescence.'+opt+suffix)
                run('static-quiescence.'+opt+'.build',[cc,*flags,'-'+opt,static_quiescence,*link_objects,*libraries,'-o',target])
                assert b'Static module quiescence: 64 real finalizer-to-worker handoffs' in run('static-quiescence.'+opt+'.run',[target])
            report['StaticModuleTransientAcceptance']=True
            target=out/('language'+suffix)
            run('language.build',[cc,*flags,'-O2',host,*link_objects,*libraries,'-o',target])
            assert b'TaskGroup language: 24 real module lifetimes' in run('language.run',[target,runtime,tcc,args.public_directory/'include',language,out/'language-temps',pool_language])
            report['GeneratedFinallyAcceptance']=True
            generated=out/'standalone.c'
            run('standalone.cgen',[build['Driver'],standalone,'cgen-default',generated])
            strict=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
                '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
            run('standalone.strict',[cc,*flags,*strict,'-I'+str(args.public_directory/'include'),'-O2','-fsyntax-only',generated])
            for mode in ('run','module'):
                output=run('standalone.'+mode,[build['Driver'],standalone,mode,out/('standalone.'+mode)])
                assert output.count(b'task-group-aot-generated-cleanup')==1
            report['OneShotAcceptance']=not blockers
            image=out/('standalone'+suffix)
            run('standalone.aot.build',[build['Driver'],standalone,'exe',image])
            actual=Path(str(image)+suffix) if os.name=='nt' else image
            relocated=out/'relocated';relocated.mkdir();moved=relocated/image.name;shutil.move(actual,moved)
            imports=run('standalone.imports',[inspector,'-p' if os.name=='nt' else '-d',moved]).decode(errors='replace')
            dependencies=re.findall(r'DLL Name:\s*(\S+)',imports) if os.name=='nt' else re.findall(r'\(NEEDED\).*?\[(.*?)\]',imports)
            assert not any('xl.dll'==name.lower() or 'xlc.dll'==name.lower() or 'libxl' in name.lower() for name in dependencies)
            if os.name!='nt':assert '(RPATH)' not in imports and '(RUNPATH)' not in imports
            for trial in range(64):
                output=run('standalone.moved'+str(trial),[moved],directory=relocated)
                assert output.count(b'task-group-aot-generated-cleanup')==1
            report.update(IndependentAotFinallyAcceptance=True,AotDependencies=dependencies,AotTrialCount=64)
            cli_target=out/('source-cli'+suffix)
            shutil.copytree(args.public_directory/'include',out/'include')
            run('cli.build',[cc,*flags,'-Wno-format-truncation','-O2',cli,*link_objects,*libraries,'-o',cli_target])
            assert run('cli.run',[cli_target,'run',standalone,'--inc-path',args.public_directory/'include','--inc-path',root/'release/include']).count(b'task-group-aot-generated-cleanup')==1
            report['SourceCliRetirementAcceptance']=True
        report.update(ExecutionComplete=True,Complete=not blockers)
    finally:
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['InputsUnchanged']
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
