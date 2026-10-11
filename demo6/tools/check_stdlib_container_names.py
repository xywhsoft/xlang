"""OrderedMap source migration and the transparent Queue/MpmcQueue identity.
Actual source compiler, migrated fixture batch, strict/pruned C and public AOT.
Not new tree result APIs, concurrent stress, shipping DLL/VFS or full53.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
from check_stdlib_slot_map import stage_private_sdk
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args(); root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve()
    build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'))
    driver=Path(build['Driver']); cc=shutil.which('gcc'); nm=shutil.which('nm')
    assert cc and nm and not out.exists() and build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    manifest,sdk,skipped=stage_private_sdk(root)
    primary=root/'test/cases/stdlib_container_public_names.xl'; queue=root/'test/cases/stdlib_default_queue.xl'
    cases=sorted(p for p in (root/'test/cases').glob('*.xl') if 'OrderedMap' in p.read_text(encoding='utf-8-sig'))
    example=root/'release/examples/stdlib_special_collections.xl'; cases=sorted(set([*cases,queue,example]))
    host=root/'test/cases/stdlib_container_names_host.c'; memory=root/'test/support/module_product_memory.c'
    artifact=root/'test/cases/xlir_artifact_runtime_contract.c'; contract=root/'test/cases/xlir_tree_native_contract.c'
    guards=[root/'tools'/name for name in ('check_xlir_avl_resource_abi.ps1','check_xlir_cgen_runtime_matrix.ps1',
        'check_xlir_contract.ps1','check_xlir_migration_coverage.ps1','gen_xrt_xlang_matrix.py')]
    assert all(not re.search(r'\bAvlTree\b',p.read_text(encoding='utf-8-sig')) for p in guards)
    diagnostic='XLIR runtime OrderedMap operation has no exact key, value, order, ownership or failure ABI'
    assert diagnostic in (root/'src/ir/xl6_ir.c').read_text(encoding='utf-8-sig')
    assert diagnostic in (root/'tools/check_xlir_contract.ps1').read_text(encoding='utf-8-sig')
    files={Path(__file__).resolve(),Path(__file__).with_name('check_stdlib_slot_map.py'),manifest,driver,Path(cc),Path(nm),
        public/'build.json',runtime,host,memory,artifact,contract,*cases,*guards}
    files.update(Path(row[key]) for row in sdk for key in ('Path','Source'))
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(files)]; out.mkdir(); rows=[]; products=[]; failures=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Failures=failures,
        PublicContainerNames=True,QueueAliasIdentity=False,SourceGuardNamesReviewed=True,
        PhysicalPrivateSourceSdk=sdk,UnstagedCompilerImportResources=skipped,
        PrunedSourceExecutables=False,PublicAotExecutable=False,Full53Acceptance=False,PrunedShippingArtifact=False,Scope=__doc__)
    env=dict(os.environ)
    if os.name!='nt': env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    else: shutil.copy2(runtime,out/'xl.dll')
    def run(name,cmd,rejected=False):
        p=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=600)
        log=out/(name+'.log'); log.write_bytes(p.stdout+p.stderr)
        rows.append(dict(Name=name,Exit=p.returncode,Log=str(log),LogSha256=sha(log)))
        assert (p.returncode!=0 and b'failed:' in p.stdout+p.stderr) if rejected else p.returncode==0,(name,(p.stdout+p.stderr)[-6000:])
        return (p.stdout+p.stderr).decode(errors='replace')
    def product(p): products.append(dict(Path=str(p),Sha256=sha(p)))
    def case_id(p): return ('example-' if p.is_relative_to(root/'release') else '')+p.stem
    try:
        report['MigratedFixtures']=[str(p) for p in cases]
        report['MigratedNegativeFixtures']=['sem_thread_projection_copy.xl']
        for fixture in cases:
            try:
                tag=case_id(fixture); target=out/(tag+'.c')
                negative=fixture.name in report['MigratedNegativeFixtures']
                text=run(tag+'-compile',[driver,fixture,'cgen-default',target],rejected=negative)
                if negative: assert '不可复制类型不能从已有值初始化' in text and ':9:20:' in text
                else: product(target)
            except Exception as e: failures.append(dict(Fixture=str(fixture),Error=repr(e)))
        assert not failures,failures
        negatives={
            'retired-tree':'auto map = AvlTree::<int, int>()\n',
            'wrong-queue-domain':'MpmcQueue<int> q = Queue::<string>(4)\n',
            'wrong-queue-role':'MpscQueue<int> q = Queue::<int>(4)\n',
            'wrong-queue-arity':'auto q = Queue::<int, string>(4)\n',
            'duplicate-alias-overload':'func f(Queue<int> q) as int\n return 1\nend\nfunc f(MpmcQueue<int> q) as int\n return 2\nend\n'}
        report['RejectedSourceContracts']=list(negatives)
        for tag,text in negatives.items():
            source=out/(tag+'.xl'); source.write_text(text,encoding='utf-8'); product(source)
            run(tag,[driver,source,'cgen-default',out/(tag+'.c')],rejected=True)
        for fixture in [primary,queue,root/'test/cases/xlir_avl_resource_abi.xl',
            root/'test/cases/stdlib_tree_native_projection.xl',root/'test/cases/stdlib_tree_native_reference.xl',
            root/'test/cases/stdlib_special_collections.xl',root/'test/cases/stdlib_tree_queue_values.xl',
            root/'test/cases/stdlib_nullable_projection_char.xl',root/'test/cases/stdlib_set_recipes.xl',
            root/'test/cases/stdlib_dict_recipes.xl',example]:
            tag=case_id(fixture); run(tag+'-jit',[driver,fixture,'run',out/(tag+'-jit')])
        ir=out/'names-ir.json'; run('names-ir',[driver,primary,'ir-lowered',ir]); product(ir)
        types=json.loads(ir.read_text(encoding='utf-8-sig'))['types']
        queues=[t for t in types if t['runtimeResourceAbiName']=='mpmc-queue']
        assert queues and all(t['name']=='MpmcQueue' for t in queues)
        concrete=[t for t in queues if t['params']]
        assert len(concrete)==1 and len(concrete[0]['params'])==1 and concrete[0]['concurrentQueueSchemaType']
        assert not any(t['name'] in ('Queue','AvlTree') for t in types)
        assert any(t['name']=='OrderedMap' and t['runtimeResourceAbiName']=='avl-tree' for t in types)
        report['QueueAliasIdentity']=True
        ext='.exe' if os.name=='nt' else ''
        flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include','-Isrc/runtime',
            '-Ilib/xruntime','-Ilib/xrt','-I'+str(public/'include'),'-DXL_STATIC_EMBED',
            '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable',
            '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
        if os.name!='nt': flags+=['-D_GNU_SOURCE']
        systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
        links=[*systems] if os.name=='nt' else [*systems,'-Wl,-rpath,'+str(runtime.parent)]
        objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).name not in ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
        planner=out/('planner'+ext); proof=out/('tree-contract'+ext)
        run('planner-build',[cc,*flags,'-Ilib/tcc','-O0',artifact,*objects,runtime,*links,'-o',planner]); product(planner)
        run('contract-build',[cc,*flags,'-Ilib/tcc','-O0',contract,*objects,runtime,*links,'-o',proof]); product(proof)
        report['Contracts']=[]
        for fixture in (primary,root/'test/cases/stdlib_tree_native_reference.xl'):
            text=run(fixture.stem+'-contract',[proof,fixture])
            found=re.search(r'(\d+) actual factories \((\d+) nominal\), (\d+) operations, (\d+) rejected mutations',text); assert found
            report['Contracts'].append(dict(zip(('Factories','Nominal','Operations','RefusedMutations'),map(int,found.groups()))))
        report['PrunedPlans']=[]
        for tag,fixture in (('full',primary),('queue',queue)):
            code=out/(fixture.stem+'.c'); repeat=out/(tag+'-repeat.c')
            run(tag+'-repeat',[driver,fixture,'cgen-default',repeat]); assert sha(code)==sha(repeat); product(repeat)
            implementation=out/(tag+'-runtime.c')
            text=run(tag+'-plan',[planner,'--emit-native',fixture.as_posix(),implementation])
            plans=[json.loads(line) for line in text.splitlines() if line.startswith('{"Fixture":')]; assert len(plans)==1
            plan=plans[0]; raw=implementation.read_text(); assert '#define XRT_MODULE_ALL' not in raw and '#define XRUNTIME_MODULE_ALL' not in raw
            implementation.write_text('#define XRT_MODULE_MEMORY_DEBUG\n'+raw); product(implementation)
            report['PrunedPlans'].append(dict(Fixture=tag,Sources=plan['Sources'],NativeModules=plan['NativeModules']))
            for opt in ('O0','O2'):
                providers=[]
                for at,provider in enumerate([implementation,*[root/s for s in plan['Sources']],memory]):
                    obj=out/(tag+'-'+opt+'-'+str(at)+'.o')
                    run(tag+'-'+opt+'-provider-'+str(at),[cc,*flags,'-'+opt,'-c',provider,'-o',obj]); providers.append(obj); product(obj)
                binary=out/(tag+'-'+opt+'-pruned'+ext)
                run(tag+'-'+opt+'-link',[cc,*flags,'-'+opt,'-include',code,host,*providers,*systems,'-o',binary])
                run(tag+'-'+opt+'-lifetimes',[binary]); product(binary)
                symbols=run(tag+'-'+opt+'-symbols',[nm,'--defined-only',binary])
                if tag=='queue': assert not re.search(r'\bxl6(?:SpscQueue|MpscQueue|Tree)\w*\b',symbols)
            run(tag+'-module',[driver,fixture,'module',out/(tag+'-module')])
            executable=out/(tag+'-aot.exe')
            run(tag+'-aot-build',[driver,fixture,'exe',out/(tag+'-aot') if os.name=='nt' else executable])
            run(tag+'-aot-run',[executable]); product(executable)
        report['PrunedSourceExecutables']=report['PublicAotExecutable']=True
        report['LifetimesPerOptimization']=1000; report['Complete']=True
    finally:
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'] and report['InputsUnchanged']; print('Public container names and alias identity verified',flush=True)
if __name__=='__main__': main()
