"""Resident TaskGroup API, checked failures and ordinary scope cleanup.

Exact full-source objects, strict C, source JIT/module, independent moved AOT
and pruning. This does NOT certify TaskGroup cyclic ownership, unknown native
observers, product VFS/shipping libraries or all 53 modules.
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
    native=root/'test/support/task_group_resident_native.c';lifetime=root/'test/support/process_ownership_public.c'
    planner=root/'test/support/container_projection_artifact.c'
    configured=root/'test/cases/stdlib_task_group_configured_native.c'
    contract=root/'test/cases/xlir_task_group_configured_contract.c'
    starter=root/'test/cases/stdlib_task_group_start_native.c'
    starter_contract=root/'test/cases/xlir_task_group_start_contract.c'
    quiescence=root/'test/support/static_module_quiescence.c'
    zero_contract=root/'test/cases/xlir_zero_suspend_async_contract.c'
    fixtures=[root/'test/cases'/(name+'.xl') for name in (
        'stdlib_task_group','stdlib_task_group_methods','stdlib_task_group_allocations','stdlib_task_group_sync_only',
        'stdlib_task_group_configured','stdlib_task_group_start','stdlib_task_group_start_types',
        'stdlib_task_group_start_async','xlir_nested_callable_type_context','xlir_static_method_value',
        'xlir_zero_suspend_async')]
    negatives=[root/'test/cases'/(name+'.xl') for name in ('xlir_task_group_bad_future','xlir_task_group_bad_effect',
        'sem_task_group_private_handle','sem_task_group_raw_construct','sem_task_group_snapshot_default',
        'bad_task_group_start_return','bad_task_group_start_arity','bad_task_group_start_width',
        'bad_task_group_start_no_raise','bad_static_method_value_private')]
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    pinned.update((Path(__file__).resolve(),Path(sys.executable),Path(cc),Path(inspector),native,lifetime,planner,
        configured,contract,starter,starter_contract,quiescence,zero_contract,runtime,*fixtures,*negatives))
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
        ComparedSources=compared,Scope=__doc__,NativeApiAcceptance=False,OrdinaryScopeCleanupAcceptance=False,
        TaskGroupGraphAcceptance=False,ShippingAcceptance=False,Full53Acceptance=False,ProvidersInstrumented=False)
    suffix='.exe' if os.name=='nt' else ''
    def run(name,command,directory=root,reject=False):
        command=list(map(str,command));actual=command
        # Hundreds of exact source-object paths exceed Windows' command limit.
        # Preserve the logical argv and hash the response bytes GCC consumes.
        response=None
        if command[0]==cc:
            response=out/(name+'.rsp')
            response.write_text('\n'.join('"'+v.replace('\\','/').replace('"','\\"')+'"' for v in command[1:])+'\n',encoding='utf-8')
            actual=[cc,'@'+str(response)]
        process=subprocess.run(actual,cwd=directory,env=environment,capture_output=True,timeout=300)
        data=process.stdout+process.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=command,ActualCommand=actual,ResponseSha256=sha(response) if response else None,
            Exit=process.returncode,ExpectedRejection=reject,Log=str(log),LogSha256=sha(log)))
        assert process.returncode!=0 if reject else process.returncode==0,(name,data[-5000:].decode(errors='replace'))
        print(name+': '+str(process.returncode),flush=True);return data.decode(errors='replace')
    try:
        for optimization in ('O0','O2'):
            binary=out/('native.'+optimization+suffix)
            run(optimization+'.build',[cc,*flags,'-'+optimization,native,*objects,*libraries,'-o',binary])
            output=run(optimization+'.run',[binary])
            assert 'rejected cancelled second parent cannot cancel' in output and 'independent consumers and cancellation balanced' in output
            matches=re.findall(r'operation=(\d+) prefixes=(\d+) first-no-hit=(\d+) balanced',output)
            assert len(matches)==5 and [(int(op),int(first)) for op,_,first in matches]==[(0,6),(1,7),(2,1),(3,1),(4,3)]
            for operation,runs,first in matches:
                runs,first=int(runs),int(first);assert runs==first+1
                prefixes.append(dict(Optimization=optimization,Operation=operation,Runs=runs,FirstNoHit=first))
            configured_image=out/('configured.'+optimization+suffix)
            run(optimization+'.configured.build',[cc,*flags,'-'+optimization,configured,*objects,*libraries,'-o',configured_image])
            output=run(optimization+'.configured.run',[configured_image])
            assert 'operation=0 prefix=6 balanced' in output and 'operation=1 prefix=7 balanced' in output and 'operation=2 prefix=1 balanced' in output
            start_image=out/('start.'+optimization+suffix)
            run(optimization+'.start.build',[cc,*flags,'-'+optimization,starter,*objects,*libraries,'-o',start_image])
            output=run(optimization+'.start.run',[start_image])
            assert 'TaskGroup starter: actual code scope' in output
            tail_image=out/('failed-tail.'+optimization+suffix)
            run(optimization+'.tail.build',[cc,*flags,'-'+optimization,quiescence,*objects,*libraries,'-o',tail_image])
            output=run(optimization+'.tail.run',[tail_image])
            assert '192 real finalizer-to-worker handoffs' in output
        strict=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
            '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
        for fixture in fixtures:
            generated=out/(fixture.stem+'.c');run(fixture.stem+'.cgen',[build['Driver'],fixture,'cgen-default',generated])
            run(fixture.stem+'.strict',[cc,*flags,*strict,'-I'+str(args.public_directory/'include'),'-O2','-fsyntax-only',generated])
            for mode in ('run','module'):run(fixture.stem+'.'+mode,[build['Driver'],fixture,mode,out/(fixture.stem+'.'+mode)])
        for fixture in negatives:
            generated=out/(fixture.stem+'.invalid.c')
            output=run(fixture.stem+'.reject',[build['Driver'],fixture,'cgen-default',generated],reject=True)
            assert ('XLIR' in output if fixture.stem.startswith('xlir_') else
                '[sem]' in output or '[checked]' in output)
            assert not generated.exists() or not generated.stat().st_size
        contract_image=out/('configured-contract'+suffix)
        run('contract.build',[cc,*flags,'-I'+str(root/'lib/tcc'),'-O2',contract,*objects,*libraries,'-o',contract_image])
        output=run('contract.run',[contract_image,fixtures[4]])
        assert 'pre-emission refusals' in output
        start_contract_image=out/('start-contract'+suffix)
        run('start.contract.build',[cc,*flags,'-I'+str(root/'lib/tcc'),'-O2',starter_contract,*objects,*libraries,'-o',start_contract_image])
        output=run('start.contract.run',[start_contract_image,*fixtures[5:8]])
        assert 'zero-C/header refusals' in output
        zero_image=out/('zero-contract'+suffix)
        run('zero.contract.build',[cc,*flags,'-I'+str(root/'lib/tcc'),'-O2',zero_contract,*objects,*libraries,'-o',zero_image])
        output=run('zero.contract.run',[zero_image,fixtures[10]])
        assert 'no implicit task pool' in output
        host=out/('public-lifetime'+suffix)
        run('lifetime.build',[cc,*flags,'-O2',lifetime,*objects,*libraries,'-o',host])
        tcc=str(root/'res/tcc/win64/x86_64') if os.name=='nt' else str(Path(build['NativeRoot'])/'build/linux/tcc-dist')
        output=run('lifetime.run',[host,runtime,tcc,args.public_directory/'include',fixtures[1],out/'lifetime-temps'])
        assert output.count('task-group-resident-methods')==5 and 'complete XRT release ledger balanced' in output
        artifact=out/('artifact-plan'+suffix)
        run('artifact.build',[cc,*flags,'-O0',planner,*objects,*libraries,'-o',artifact])
        for fixture,asynchronous in ((fixtures[3],False),(fixtures[1],True),(fixtures[5],False),
            (fixtures[6],False),(fixtures[7],True),(fixtures[10],False)):
            prefix=out/(fixture.stem+'.plan');run(fixture.stem+'.plan',[artifact,fixture,prefix])
            plan=json.loads(Path(str(prefix)+'.plan.json').read_text());sources={Path(path).name for path in plan['Sources']}
            assert ('xl6_task_group_future.c' in sources)==asynchronous
            assert ('xl6_task_group_start.c' in sources)==(fixture in fixtures[5:8])
            assert not {'XRT_MODULE_TASK_GROUP_POOL','XRT_MODULE_TASK_GROUP_COROUTINE','XRT_MODULE_TASK_POOL','XRT_MODULE_COROUTINE'}&set(plan['NativeModules'])
        image=out/('task-group.aot'+suffix);run('owner.aot.build',[build['Driver'],fixtures[1],'exe',image])
        actual=Path(str(image)+'.exe') if os.name=='nt' and not image.exists() else image
        moved=relocated/actual.name;shutil.copy2(actual,moved)
        imports=run('owner.aot.imports',[inspector,'-p' if os.name=='nt' else '-d',moved],relocated)
        dependencies=re.findall(r'DLL Name:\s*([^\s]+)',imports) if os.name=='nt' else re.findall(r'Shared library:\s*\[([^\]]+)\]',imports)
        assert dependencies and not {name.lower() for name in dependencies}&{'xl.dll','xlc.dll','libxl.so','libxlc.so'}
        if os.name!='nt':assert '(RPATH)' not in imports and '(RUNPATH)' not in imports
        for trial in range(3):assert run('owner.aot.moved'+str(trial),[moved],relocated).count('task-group-resident-methods')==1
        report['AotDependencies']=dependencies
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);assert report['InputsUnchanged']
        report.update(Complete=True,NativeApiAcceptance=True,OrdinaryScopeCleanupAcceptance=True,
            LanguageFixtureCount=len(fixtures),SourceAotAcceptance=True,AsyncProviderPruningAcceptance=True,
            JitImageRetirementProof='MEM_FREE' if os.name=='nt' else 'non-executable')
    except Exception as error:failures.append(repr(error));print(repr(error),file=sys.stderr)
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
