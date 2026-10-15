"""Exact Process/native/shared-Future graph with phased retirement.

Full source providers, physical-node snapshots, real cycle collection, closed
construction-failure prefixes, original language regressions and moved AOT.
The genuine released TCC core is reused; this is not shipping/VFS or full53
acceptance. Process-backed IO graph admission remains a separate task.
"""
from pathlib import Path
import argparse,json,os,re,shutil,subprocess,sys
from xlir_public_build_inputs import sha,verify_public_build

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve();assert not out.exists()
    build,pinned,compared=verify_public_build(root,args.public_directory)
    cc=shutil.which('gcc');inspector=shutil.which('objdump' if os.name=='nt' else 'readelf');assert cc and inspector
    contract=root/'test/support/process_ownership_native.c';child_source=root/'test/cases/xlir_process_child.c'
    artifact=root/'test/support/container_projection_artifact.c'
    lifetime=root/'test/support/process_ownership_public.c'
    fixtures=[root/'test/cases'/(name+'.xl') for name in (
        'stdlib_process_live_wait','stdlib_process_live_streams','stdlib_process_live_drop',
        'stdlib_process_future','stdlib_process_future_cancel','stdlib_process_future_drop',
        'stdlib_process_future_allocations','stdlib_process_future_cancel_allocations',
        'stdlib_native_scalar_convert','stdlib_native_scalar_as','stdlib_native_scalar_errors',
        'stdlib_future_receivers','stdlib_native_process_live_owner_aot')]
    negative=root/'test/cases/sem_future_receiver_expr.xl'
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    pinned.update((Path(__file__).resolve(),Path(sys.executable),Path(cc),Path(inspector),contract,artifact,lifetime,negative,child_source,runtime,*fixtures))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    objects=[Path(row['Path']) for row in build['Objects'] if Path(row['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
    libraries=[str(runtime)]
    if os.name=='nt':libraries+=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
    else:flags+=['-D_GNU_SOURCE'];libraries+=['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    out.mkdir(parents=True);relocated=out/'relocated';relocated.mkdir()
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    rows=[];failures=[];prefixes=[];environment=dict(os.environ)
    if os.name!='nt':environment['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Failures=failures,Prefixes=prefixes,
        ComparedSources=compared,Scope=__doc__,ProcessGraphAcceptance=False,ProcessIoGraphAcceptance=False,
        ShippingAcceptance=False,Full53Acceptance=False,ProvidersInstrumented=False)
    suffix='.exe' if os.name=='nt' else ''
    def run(name,command,directory=root,reject=False):
        command=list(map(str,command));process=subprocess.run(command,cwd=directory,env=environment,capture_output=True,timeout=300)
        data=process.stdout+process.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=process.returncode,ExpectedRejection=reject,Log=str(log),LogSha256=sha(log)))
        assert process.returncode!=0 if reject else process.returncode==0,(name,data[-5000:].decode(errors='replace'))
        print(name+': '+str(process.returncode),flush=True);return data.decode(errors='replace')
    try:
        child=out/('process-child'+suffix)
        run('child.build',[cc,'-std=c11','-O2',*(['-municode'] if os.name=='nt' else ['-D_GNU_SOURCE']),child_source,'-o',child])
        environment['XL6_PROCESS_CHILD']=str(child)
        invalid=out/'future-receiver.invalid.c'
        output=run('future-receiver.reject',[build['Driver'],negative,'cgen-default',invalid],reject=True)
        assert 'future method argument count mismatch' in output and (not invalid.exists() or invalid.stat().st_size==0)
        for optimization in ('O0','O2'):
            binary=out/('native.'+optimization+suffix)
            run(optimization+'.build',[cc,*flags,'-'+optimization,contract,*objects,*libraries,'-o',binary])
            output=run(optimization+'.run',[binary,child])
            assert '3/11/5 physical nodes' in output and 'opaque legacy observer refused' in output
            assert 'EOF only at Finish' in output
            match=re.search(r'continuation prefixes=(\d+) first-no-hit=(\d+) balanced',output);assert match
            runs,first=map(int,match.groups());assert first>0 and runs==first+1
            prefixes.append(dict(Optimization=optimization,Runs=runs,FirstNoHit=first))
        strict=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
            '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
        marker='native-live-process-finalized'
        for fixture in fixtures:
            generated=out/(fixture.stem+'.c');run(fixture.stem+'.cgen',[build['Driver'],fixture,'cgen-default',generated])
            run(fixture.stem+'.strict',[cc,*flags,*strict,'-I'+str(args.public_directory/'include'),'-O2','-fsyntax-only',generated])
            for mode in ('run','module'):
                output=run(fixture.stem+'.'+mode,[build['Driver'],fixture,mode,out/(fixture.stem+'.'+mode)])
                # These convenience modes do not drain deferred JIT owners.
                # The explicit public lifecycle host below proves retirement.
        host=out/('public-lifetime'+suffix)
        run('lifetime.build',[cc,*flags,'-O2',lifetime,*objects,*libraries,'-o',host])
        tcc=str(root/'res/tcc/win64/x86_64') if os.name=='nt' else str(Path(build['NativeRoot'])/'build/linux/tcc-dist')
        output=run('lifetime.run',[host,runtime,tcc,args.public_directory/'include',fixtures[-1],out/'lifetime-temps'])
        assert output.count(marker)==5 and 'complete XRT release ledger balanced' in output
        output=run('receiver-lifetime.run',[host,runtime,tcc,args.public_directory/'include',fixtures[-2],out/'receiver-lifetime-temps'])
        assert 'complete XRT release ledger balanced' in output
        planner=out/('artifact-plan'+suffix)
        run('artifact.build',[cc,*flags,'-O0',artifact,*objects,*libraries,'-o',planner])
        for name,asynchronous in (('stdlib_process_live_wait',False),('stdlib_process_future',True)):
            prefix=out/(name+'.plan');run(name+'.plan',[planner,root/'test/cases'/(name+'.xl'),prefix])
            plan=json.loads(Path(str(prefix)+'.plan.json').read_text())
            source_names={Path(source).name for source in plan['Sources']}
            assert ('xl6_process_future.c' in source_names)==asynchronous
            assert ('xl6_ownership_domain_process_future.c' in source_names)==asynchronous
            assert ('XRT_MODULE_PROCESS_FUTURE' in plan['NativeModules'])==asynchronous
        fixture=fixtures[-1];image=out/('process-owner.aot'+suffix)
        run('owner.aot.build',[build['Driver'],fixture,'exe',image])
        actual=Path(str(image)+'.exe') if os.name=='nt' and not image.exists() else image
        moved=relocated/actual.name;shutil.copy2(actual,moved)
        imports=run('owner.aot.imports',[inspector,'-p' if os.name=='nt' else '-d',moved],relocated)
        dependencies=re.findall(r'DLL Name:\s*([^\s]+)',imports) if os.name=='nt' else re.findall(r'Shared library:\s*\[([^\]]+)\]',imports)
        assert dependencies and not {name.lower() for name in dependencies}&{'xl.dll','xlc.dll','libxl.so','libxlc.so'}
        if os.name!='nt':assert '(RPATH)' not in imports and '(RUNPATH)' not in imports
        for trial in range(3):assert run('owner.aot.moved'+str(trial),[moved],relocated).count(marker)==1
        report['AotDependencies']=dependencies
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);assert report['InputsUnchanged']
        report.update(Complete=True,ProcessGraphAcceptance=True,LanguageFixtureCount=len(fixtures),SourceAotAcceptance=True,AsyncProviderPruningAcceptance=True,
            JitImageRetirementProof='MEM_FREE' if os.name=='nt' else 'non-executable')
    except Exception as error:failures.append(repr(error));print(repr(error),file=sys.stderr)
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
