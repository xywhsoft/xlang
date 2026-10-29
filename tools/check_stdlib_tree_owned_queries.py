"""Verify public owned OrderedMap queries using a fresh source compiler.
Exact signature/effects, missing versus present-null, JIT/module/AOT, real IR
mutation refusal, complete allocation/failure prefixes, O0/O2 owned lifetimes
and independently linked selected closures. Not shipping DLL/VFS, whole-runtime
sanitization, Future/owning-record codecs or full 53-module acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
from check_stdlib_slot_map import stage_private_sdk

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true')
    a=parser.parse_args(); root=a.root.resolve(); out=a.out.resolve(); public=a.public_directory.resolve()
    build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'))
    driver=Path(build['Driver']); cc=shutil.which('gcc'); nm=shutil.which('nm')
    assert cc and nm and not out.exists() and build['SourceXrt'] and not build['RuntimeOnly']
    assert not a.sanitize or os.name!='nt'
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    case=root/'test/cases/stdlib_tree_owned_queries.xl'
    host=root/'test/cases/stdlib_tree_owned_language_host.c'; memory=root/'test/support/module_product_memory.c'
    contract=root/'test/cases/xlir_tree_native_contract.c'; artifact=root/'test/cases/xlir_artifact_runtime_contract.c'
    names=('stdlib_tree_owned_queries','stdlib_tree_owned_errors','stdlib_tree_owned_continuation','stdlib_tree_owned_generic',
        'stdlib_tree_native_reference','stdlib_tree_native_projection','stdlib_nullable_resource_transport',
        'stdlib_nullable_text_resource_transport','stdlib_tree_resource_values','stdlib_tree_semaphore_values',
        'stdlib_tree_sync_values','stdlib_thread_collections','stdlib_special_collections','stdlib_special_collections_async',
        'stdlib_tree_queue_values','stdlib_container_public_names','xlir_avl_resource_abi','stdlib_tree_query_selection')
    cases=[root/'test/cases'/(name+'.xl') for name in names]
    manifest,sdk,skipped=stage_private_sdk(root)
    files={Path(__file__).resolve(),Path(__file__).resolve().parent/'check_stdlib_slot_map.py',
        public/'build.json',driver,Path(cc),Path(nm),Path(os.sys.executable),runtime,host,memory,contract,artifact,manifest,*cases}
    files.update(Path(row[key]) for row in sdk for key in ('Source','Path'))
    files.update(p for folder in ('src','lib','release/include','test/support')
        for p in (root/folder).rglob('*') if p.suffix in ('.c','.h','.inc'))
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(files)]
    out.mkdir(); rows=[]; products=[]; prefixes=[]; contracts=[]; negatives=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,
        Prefixes=prefixes,Contracts=contracts,RejectedSourceContracts=negatives,LanguageOwnedTreeQueries=True,
        Full53Acceptance=False,PrunedShippingArtifact=False,PrunedSourceExecutables=False,PublicAotExecutable=False,
        SelectedClosureSanitizers=a.sanitize,WholeRuntimeInstrumented=False,PhysicalPrivateSourceSdk=sdk,
        UnstagedCompilerImportResources=skipped,Scope=__doc__)
    env=dict(os.environ)
    if os.name!='nt': env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    else: shutil.copy2(runtime,out/'xl.dll')
    def run(name,cmd,refused=False,diagnostic=None):
        command=list(map(str,cmd)); p=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=600)
        data=p.stdout+p.stderr; log=out/(name+'.log'); log.write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=p.returncode,Log=str(log),LogSha256=sha(log)))
        if refused:
            assert p.returncode!=0 and (not diagnostic or diagnostic.encode() in data),(name,data[-7000:])
        else: assert p.returncode==0,(name,data[-7000:])
        return data.decode(errors='replace')
    def product(path): products.append(dict(Path=str(path),Sha256=sha(path)))
    def prefix(binary,opt,lane):
        for point in range(4097):
            text=run(opt+'-'+lane+'-'+str(point),[binary,lane,point])
            assert 'prefix='+lane+' at='+str(point)+' hit=' in text
            if 'hit=0' in text:
                prefixes.append(dict(Lane=lane,Optimization=opt,FirstNoHit=point,Runs=point+1,Complete=True)); return
        raise AssertionError('incomplete failure prefix '+opt+'/'+lane)
    ext='.exe' if os.name=='nt' else ''
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include',
        '-Isrc/runtime','-Ilib/xruntime','-Ilib/xrt','-I'+str(public/'include')]
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=[runtime,*systems]+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)])
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    generatedFlags=flags+['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    try:
        generated=out/'queries.c'; repeat=out/'repeat.c'; ir=out/'queries.ir'
        run('generate',[driver,case,'cgen-default',generated]); run('repeat',[driver,case,'cgen-default',repeat])
        assert sha(generated)==sha(repeat)
        run('lowered-ir',[driver,case,'ir-lowered',ir]); body=generated.read_text(encoding='utf-8-sig')
        for token in ('GET','TAKE','FIRST','LAST','LOWER','UPPER'):
            assert body.count('XL6_TREE_QUERY_'+token+'_V2')>=2,token
        for name in ('get','take','first','last','lowerBound','upperBound','tryGet','tryTake','tryFirst','tryLast','tryLowerBound','tryUpperBound'):
            assert 'tree-query-'+name in ir.read_text(encoding='utf-8-sig'),name
        assert 'xl6TreeQueryV2(' in body and 'xl6TreeGetV1(' not in body and 'xl6TreeEndpointV1(' not in body
        assert re.search(r'projection_ok_\d+ = xl6TreeQueryV2\(v\d+,&tree_layout,',body)
        assert '#include <__xl_runtime/xl6_tree.h>' in body
        for fixture in cases: run(fixture.stem+'-jit',[driver,fixture,'run',out/(fixture.stem+'-jit')])
        proof=out/('contract'+ext); planner=out/('planner'+ext)
        run('contract-build',[cc,*flags,'-Ilib/tcc','-O0',contract,*objects,*links,'-o',proof]); product(proof)
        run('planner-build',[cc,*flags,'-Ilib/tcc','-O0',artifact,*objects,*links,'-o',planner]); product(planner)
        # Every Tree fixture gets actual typed/lowered contract mutations; async
        # and generic templates are exercised through their real JIT producers.
        for fixture in cases:
            if fixture.stem in ('stdlib_tree_owned_continuation','stdlib_tree_owned_generic','stdlib_special_collections_async'): continue
            text=run(fixture.stem+'-contract',[proof,fixture])
            match=re.search(r'(\d+) actual factories \((\d+) nominal\), (\d+) operations \((\d+) owned queries\), (\d+) rejected mutations',text)
            assert match,(fixture,text)
            contracts.append(dict(Source=str(fixture),**dict(zip(('Factories','Nominal','Operations','OwnedQueries','RejectedMutations'),map(int,match.groups())))))
        oldNames=('getInto','takeInto','firstInto','lastInto','lowerBoundInto','upperBoundInto')
        negativeBodies={name:'auto tree=OrderedMap::<int, int>()\nint key=0\nint value=0\ntree.'+name+'(key,value)\n' for name in oldNames}
        negativeBodies.update({'wrong-key':'auto tree=OrderedMap::<int, string>()\ntree.get("bad")\n',
            'wrong-arity':'auto tree=OrderedMap::<int, string>()\ntree.tryGet()\n',
            'wrong-return':'auto tree=OrderedMap::<int, string>()\nbool wrong=tree.get(1)\n'})
        for tag,source in negativeBodies.items():
            fixture=out/(tag+'.xl'); fixture.write_text(source,encoding='utf-8'); product(fixture)
            run(tag+'-reject',[driver,fixture,'cgen-default',out/(tag+'.c')],True,'[SC-TREE-OWNED]' if tag in oldNames else None)
            negatives.append(dict(Name=tag,Retired=tag in oldNames,Source=str(fixture)))
        memoryObject=out/'memory.o'
        run('memory-build',[cc,*flags,'-O0','-c',memory,'-o',memoryObject]); product(memoryObject)
        for opt in ('O0','O2'):
            binary=out/('full-'+opt+ext); fault=out/('fault-'+opt+ext)
            run(opt+'-full-build',[cc,*generatedFlags,'-'+opt,'-include',generated,host,memoryObject,*objects,*links,'-o',binary])
            run(opt+'-full-lifetimes',[binary]); product(binary)
            run(opt+'-fault-build',[cc,*generatedFlags,'-'+opt,'-DXL6_TREE_QUERY_TEST_FAULTS','-include',generated,host,memoryObject,
                *objects,*links,'-Wl,--wrap=xl6TreeQueryV2','-o',fault]); product(fault)
            for lane in ('query','oom'): prefix(fault,opt,lane)
        implementation=out/'selected-runtime.c'
        text=run('selected-plan',[planner,'--emit-native',case.as_posix(),implementation])
        plans=[json.loads(line) for line in text.splitlines() if line.startswith('{"Fixture":')]; assert len(plans)==1
        plan=plans[0]; assert 'src/runtime/xl6_tree.c' in plan['Sources']
        raw=implementation.read_text(); assert '#define XRT_MODULE_ALL' not in raw and '#define XRUNTIME_MODULE_ALL' not in raw
        implementation.write_text('#define XRT_MODULE_MEMORY_DEBUG\n'+raw,encoding='utf-8'); product(implementation)
        report['PrunedPlan']=plan
        instrumentation=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if a.sanitize else []
        for opt in ('O0','O2'):
            providers=[]
            for at,source in enumerate([implementation,*[root/s for s in plan['Sources']],memory]):
                obj=out/('selected-'+opt+'-'+str(at)+'.o')
                run('selected-'+opt+'-body-'+str(at),[cc,*generatedFlags,*instrumentation,'-'+opt,'-c',source,'-o',obj]); product(obj); providers.append(obj)
            binary=out/('selected-'+opt+ext)
            run('selected-'+opt+'-link',[cc,*generatedFlags,*instrumentation,'-'+opt,'-include',generated,host,*providers,*systems,'-o',binary])
            run('selected-'+opt+'-lifetimes',[binary]); product(binary)
            symbols=run('selected-'+opt+'-symbols',[nm,'--defined-only',binary]); assert re.search(r'\bxl6TreeQueryV2\b',symbols)
            assert not re.search(r'\bxl6(?:SpscQueue|MpscQueue|MpmcQueue|SlotMap|Regex)\w*\b',symbols)
            fault=out/('selected-'+opt+'-fault'+ext)
            run('selected-'+opt+'-fault-build',[cc,*generatedFlags,*instrumentation,'-'+opt,'-DXL6_TREE_QUERY_TEST_FAULTS','-include',generated,
                host,*providers,*systems,'-Wl,--wrap=xl6TreeQueryV2','-o',fault]); product(fault)
            for lane in ('query','oom'): prefix(fault,'selected-'+opt,lane)
        run('public-module',[driver,case,'module',out/'module'])
        executable=out/'aot.exe'
        run('public-aot-build',[driver,case,'exe',out/'aot' if os.name=='nt' else executable])
        run('public-aot-execute',[executable]); product(executable)
        for p in (generated,repeat,ir): product(p)
        report['LifetimesPerOptimization']=1000; report['PrunedSourceExecutables']=report['PublicAotExecutable']=True
        report['JitCases']=len(cases); report['Complete']=True
    finally:
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'] and report['InputsUnchanged']; print('Public owned OrderedMap queries verified',flush=True)

if __name__=='__main__': main()
