"""Public SlotMap native loops: exact owned keys, four acquisition lanes,
snapshot mutation, finally/nonlocal cleanup, checked failures without diagnostic,
complete allocation prefixes, independent O0/O2 native plans, JIT and public AOT.
Source-product validation only; not shipping DLL/VFS or full 53-module acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
from check_stdlib_slot_map import stage_private_sdk
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args(); root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    cc,nm=shutil.which('gcc'),shutil.which('nm'); assert cc and nm and not out.exists()
    build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly']; driver=Path(build['Driver'])
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    sources=[root/'test/cases/stdlib_slot_map_loop_plan.xl',root/'test/cases/stdlib_slot_map_loop.xl']
    host=root/'test/cases/stdlib_slot_map_loop_host.c'; contract=root/'test/cases/xlir_slot_map_loop_contract.c'
    artifact=root/'test/cases/xlir_artifact_runtime_contract.c'; memory=root/'test/support/module_product_memory.c'
    manifest,sdk,skipped=stage_private_sdk(root)
    files={Path(__file__).resolve(),root/'tools/check_stdlib_slot_map.py',Path(cc),Path(nm),public/'build.json',
        driver,runtime,host,contract,artifact,memory,manifest,*sources}
    files.update(Path(row[k]) for row in sdk for k in ('Source','Path'))
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(),row['Path']; files.add(Path(row['Path']))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir(); rows=[]; products=[]; prefixes=[]; plans=[]; counts=[]; env=dict(os.environ)
    if os.name=='nt': shutil.copy2(runtime,out/'xl.dll')
    else: env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Prefixes=prefixes,
        Plans=plans,Contracts=counts,PhysicalPrivateSourceSdk=sdk,UnstagedCompilerImportResources=skipped,
        LanguageSlotMapLoops=False,PrunedSourceExecutables=False,PublicAotExecutable=False,
        ShippingSdkAcceptance=False,Full53Acceptance=False,Scope=__doc__)
    def run(name,cmd):
        p=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=600)
        log=out/(name+'.log'); log.write_bytes(p.stdout+p.stderr)
        rows.append(dict(Name=name,Exit=p.returncode,Log=str(log),LogSha256=sha(log)))
        assert not p.returncode,(name,(p.stdout+p.stderr)[-6500:])
        return (p.stdout+p.stderr).decode(errors='replace')
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include','-Ilib/tcc','-Ilib/xruntime','-Ilib/xrt','-I'+str(public/'include'),
        '-DXL_STATIC_EMBED','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=systems+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)])
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    ext='.exe' if os.name=='nt' else ''
    try:
        code=[]
        boundary=out/'native-key-suspension.xl'
        boundary.write_text('func keys(SlotMap<string> slots) as generator<string>\n for key, value in slots\n yield value\n assert(slots.contains(key))\n end\nend\nauto slots=SlotMap::<string>()\nslots.insert("a")\nfor text in keys(slots)\n assert(text == "a")\nend\n',encoding='utf-8')
        blocked=out/'native-key-suspension.c'
        p=subprocess.run([str(driver),str(boundary),'cgen-default',str(blocked)],cwd=root,env=env,capture_output=True,timeout=600)
        log=out/'native-key-suspension.log'; log.write_bytes(p.stdout+p.stderr)
        rows.append(dict(Name='native-key-suspension',Exit=p.returncode,Log=str(log),LogSha256=sha(log),ExpectedRejection=True))
        assert p.returncode and b'generator target has no exact machine/item ABI' in p.stdout+p.stderr
        assert not blocked.exists() or not blocked.stat().st_size
        products.append(dict(Path=str(boundary),Sha256=sha(boundary)))
        report['NativeKeySuspensionExplicitlyRejected']=True
        for index,source in enumerate(sources):
            tag=str(index); generated=out/(tag+'.c'); repeat=out/(tag+'-repeat.c')
            run(tag+'-generate',[driver,source,'cgen-default',generated]); run(tag+'-repeat',[driver,source,'cgen-default',repeat])
            assert sha(generated)==sha(repeat)
            body=generated.read_text(encoding='utf-8-sig')
            for name in ('xl6SlotMapPairIteratorCreateV1','xl6SlotMapKeyIteratorCreateV1','xl6SlotMapItemIteratorCreateV1',
                'xl6SlotMapCountIteratorCreateV1','xl6ValueIteratorKeyCopyV3'): assert name+'(' in body,name
            assert re.search(r'projection_ok_\d+ = xl6ValueIteratorKeyCopyV3\(v\d+,&v\d+\)',body)
            # A SlotMap<SlotMapKey> item has its own native-value projection;
            # this is not the complete map-key path and must not be conflated.
            if index==1: assert 'xl6SlotMapKeyBoxV1(' not in body and 'xl6SlotMapKeyUnboxV1(' not in body
            run(tag+'-jit',[driver,source,'run',out/(tag+'-jit')])
            target=out/(tag+'-lowered.json'); run(tag+'-lowered',[driver,source,'ir-lowered',target])
            products.extend(dict(Path=str(p),Sha256=sha(p)) for p in (generated,repeat,target)); code.append(generated)
        proof=out/('contract'+ext)
        run('contract-build',[cc,*flags,'-O0',contract,*objects,runtime,*links,'-o',proof])
        for index,source in enumerate(sources):
            text=run(str(index)+'-contract',[proof,source])
            match=re.search(r'calls=(\d+) keys=(\d+) lanes=(\d+) mutations=(\d+)',text); assert match
            counts.append(dict(Fixture=source.name,Calls=int(match[1]),Keys=int(match[2]),Lanes=int(match[3]),Mutations=int(match[4])))
        products.append(dict(Path=str(proof),Sha256=sha(proof)))
        memory_object=out/'memory.o'
        run('memory-build',[cc,*flags,'-O0','-c',memory,'-o',memory_object])
        products.append(dict(Path=str(memory_object),Sha256=sha(memory_object)))
        wrappers=('xl6SlotMapPairIteratorCreateV1','xl6SlotMapKeyIteratorCreateV1','xl6SlotMapItemIteratorCreateV1',
            'xl6SlotMapCountIteratorCreateV1','xl6ValueIteratorKeyCopyV3')
        for opt in ('O0','O2'):
            for index,generated in enumerate(code):
                binary=out/(str(index)+'-'+opt+ext)
                run(str(index)+'-'+opt+'-build',[cc,*flags,'-'+opt,'-include',generated,host,memory_object,*objects,runtime,*links,'-o',binary])
                run(str(index)+'-'+opt+'-lifetimes',[binary]); products.append(dict(Path=str(binary),Sha256=sha(binary)))
            fault=out/(opt+'-fault'+ext)
            run(opt+'-fault-build',[cc,*flags,'-'+opt,'-DXL6_SLOT_MAP_LOOP_FAULTS','-include',code[1],host,memory_object,*objects,
                runtime,*links,*['-Wl,--wrap='+name for name in wrappers],'-o',fault])
            products.append(dict(Path=str(fault),Sha256=sha(fault)))
            for lane in ('pairs','keys','values','count','key-copy','oom'):
                for at in range(2048):
                    text=run(opt+'-'+lane+'-'+str(at),[fault,lane,at])
                    assert 'prefix='+lane+' at='+str(at)+' hit=' in text
                    if 'hit=0' in text: break
                else: raise AssertionError('Unfinished prefix '+lane)
                prefixes.append(dict(Optimization=opt,Lane=lane,Runs=at+1,Complete=True))
        report['LanguageSlotMapLoops']=True; report['LifetimesPerFixturePerOptimization']=1000
        planner=out/('planner'+ext)
        run('planner-build',[cc,*flags,'-O0',artifact,*objects,runtime,*links,'-o',planner])
        products.append(dict(Path=str(planner),Sha256=sha(planner)))
        lean={
            'keys':'for key in slots\n assert(key == key)\nend\n',
            'values':'for _, value in slots\n assert(value == 42)\nend\n',
            'count':'for _, _ in slots\n count += 1\nend\nassert(count == 1)\n',
            'pairs':'for key, value in slots\n assert(key == key and value == 42)\nend\n'}
        fixtures=[('full',sources[1],code[1])]
        for tag,loop in lean.items():
            source=out/(tag+'.xl'); source.write_text('func main(...) as int\n auto slots = SlotMap::<int>()\n slots.insert(42)\n int count = 0\n'+loop+' return 0\nend\n',encoding='utf-8')
            # A same-stem .c file is a genuine mixed-C sidecar to the importer.
            # Keep generated test code distinct so this remains a pure xlang
            # pruning test rather than intentionally selecting MODULE_ALL.
            generated=out/(tag+'-program.c'); run(tag+'-generate',[driver,source,'cgen-default',generated]); fixtures.append((tag,source,generated))
            products.append(dict(Path=str(source),Sha256=sha(source)))
        for tag,source,generated in fixtures:
            implementation=out/(tag+'-runtime.c')
            text=run(tag+'-plan',[planner,'--emit-native',source.as_posix(),implementation])
            selected=[json.loads(line) for line in text.splitlines() if line.startswith('{"Fixture":')]; assert len(selected)==1
            plan=selected[0]; plans.append(dict(Fixture=tag,Sources=plan['Sources'],NativeModules=plan['NativeModules']))
            raw=implementation.read_text(); assert 'XRT_MODULE_ALL' not in raw and 'XRUNTIME_MODULE_ALL' not in raw
            implementation.write_text('#define XRT_MODULE_MEMORY_DEBUG\n'+raw)
            for opt in ('O0','O2'):
                providers=[]
                for index,provider in enumerate([implementation,*[root/s for s in plan['Sources']],memory]):
                    obj=out/(tag+'-'+opt+'-'+str(index)+'.o')
                    run(tag+'-'+opt+'-provider-'+str(index),[cc,*flags,'-'+opt,'-c',provider,'-o',obj]); providers.append(obj)
                    products.append(dict(Path=str(obj),Sha256=sha(obj)))
                binary=out/(tag+'-'+opt+'-pruned'+ext)
                run(tag+'-'+opt+'-link',[cc,*flags,'-'+opt,'-include',generated,host,*providers,*systems,'-o',binary])
                run(tag+'-'+opt+'-execute',[binary]); symbols=run(tag+'-'+opt+'-symbols',[nm,'--defined-only',binary])
                forbidden=[]
                if tag!='full':
                    for mode,name in lean.items():
                        native={'keys':'xl6SlotMapKeyIteratorCreateV1','values':'xl6SlotMapItemIteratorCreateV1',
                            'count':'xl6SlotMapCountIteratorCreateV1','pairs':'xl6SlotMapPairIteratorCreateV1'}[mode]
                        if mode!=tag: forbidden.append(native)
                if tag in ('values','count'): forbidden+=['xl6SlotMapKeyBoxV1','xl6SlotMapKeyUnboxV1',
                    'xl6SequenceSlotMapKeyCreateV1','xl6SlotMapKeysV1','xl6SlotMapPairsV1']
                for name in forbidden: assert not re.search(r'\b'+name+r'\b',symbols),(tag,name)
                products.append(dict(Path=str(binary),Sha256=sha(binary)))
            products.extend(dict(Path=str(p),Sha256=sha(p)) for p in (generated,implementation))
        report['PrunedSourceExecutables']=True
        run('public-module',[driver,sources[1],'module',out/'module'])
        executable=out/'aot.exe'
        run('public-aot-build',[driver,sources[1],'exe',out/'aot' if os.name=='nt' else executable])
        run('public-aot-execute',[executable]); products.append(dict(Path=str(executable),Sha256=sha(executable)))
        report['PublicAotExecutable']=True
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs); report['Complete']=report['InputsUnchanged']
    finally:
        report['PhaseCount']=len(rows)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete']; print('SlotMap public loops: exact contracts, complete fault prefixes, 10 independent native programs and public AOT verified',flush=True)
if __name__=='__main__': main()
