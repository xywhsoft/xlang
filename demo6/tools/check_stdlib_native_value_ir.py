"""Actual owning-native-record source/IR/C smoke and malformed IR refusal.

Fresh source compiler, genuine map-independent keys passed through public var
arguments, direct source owning records, strict O0/O2, checked result carriers,
products and Value Set content semantics. Resident object links, not a pruned
shipping/full53 acceptance. Frame/cell/nullable and public SlotMap remain
separate. The optional collection fixture covers sealed native-value recipes,
not universal direct-slot reads or a pruned shipping/full53 acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--fixture',default='stdlib_native_value_ir.xl',help='Named source fixture in test/cases; same genuine-key host contract')
    a=p.parse_args(); root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve()
    cc=shutil.which('gcc'); assert cc and not out.exists()
    manifest=public/'build.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig')); driver=Path(build['Driver'])
    assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    assert Path(a.fixture).name==a.fixture and a.fixture.endswith('.xl')
    source=root/'test/cases'/a.fixture; host=root/'test/cases/stdlib_native_value_ir_host.c'
    contract=root/'test/cases/xlir_native_value_contract.c'; memory=root/'test/support/module_product_memory.c'
    files={Path(__file__).resolve(),manifest,driver,runtime,source,host,contract,memory,Path(cc),
        root/'test/cases/xlir_artifact_runtime_contract.c'}
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]; out.mkdir(); rows=[]; products=[]
    prefixes=[]; controls=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Prefixes=prefixes,NegativeControls=controls,
        PrunedShippingArtifact=False,LanguageNativeValueSyncCutover=True,Full53Acceptance=False,Scope=__doc__,
        LanguageNativeValueCollections=a.fixture=='stdlib_native_value_collections.xl',LanguageSlotMapCutover=False)
    env=dict(os.environ)
    if os.name!='nt': env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    else: shutil.copy2(runtime,out/'xl.dll')
    def run(name,command,negative=False):
        r=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=600)
        log=out/(name+'.log'); log.write_bytes(r.stdout+r.stderr)
        rows.append(dict(Name=name,Exit=r.returncode,Log=str(log),LogSha256=sha(log)))
        assert bool(r.returncode)==negative,(name,(r.stdout+r.stderr)[-9000:])
        return (r.stdout+r.stderr).decode(errors='replace')
    try:
        generated=out/'native.c'; repeated=out/'native-repeat.c'
        run('generate',[driver,source,'cgen-default',generated]); run('repeat',[driver,source,'cgen-default',repeated])
        assert sha(generated)==sha(repeated)
        body=generated.read_text(encoding='utf-8-sig')
        for symbol in ('xl6SlotMapKeyCopyValueV1','xl6SlotMapKeyDropValueV1','xl6SlotMapKeyEqualV1',
                       'xl6SlotMapKeyBoxV1','xl6SlotMapKeyUnboxV1'): assert symbol+'(' in body,symbol
        assert '#include <__xl_runtime/xl6_slot_map_key_abi.h>' in body
        assert 'typedef struct XL6_SlotMapKeyV1' not in body
        products.extend(dict(Path=str(f),Sha256=sha(f)) for f in (generated,repeated))
        run('public-jit',[driver,source,'run',out/'jit'])
        for mode in ('ir-typed','ir-lowered'):
            target=out/(mode+'.json'); run(mode,[driver,source,mode,target]); products.append(dict(Path=str(target),Sha256=sha(target)))
        lowered=json.loads((out/'ir-lowered.json').read_text(encoding='utf-8-sig'))
        kept=[f for f in lowered['functions'] if f['kindName']=='user' and f['parameterCount']==1 and
            lowered['nodes'][f['declaration']-1]['line']==1 and
            lowered['types'][f['returnType']-1]['nativeValueAbi']==1]
        assert len(kept)==1
        objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
            ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
        flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include','-Isrc/runtime',
            '-I'+str(public/'include'),'-DXL_STATIC_EMBED','-Wno-unused-function','-Wno-unused-variable',
            '-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
            '-Wno-missing-field-initializers','-Wno-pointer-sign']
        flags+=['-DXL6_TEST_NATIVE_KEPT=xlir_fn_'+str(kept[0]['symbol'])]
        links=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
        if os.name!='nt': flags+=['-D_GNU_SOURCE']
        proof=out/('contract'+('.exe' if os.name=='nt' else ''))
        run('contract-build',[cc,*flags,'-Ilib/tcc','-O0',contract,*objects,runtime,*links,'-o',proof])
        text=run('contract',[proof,source]); match=re.search(r'native values copy=(\d+) equal=(\d+) box=(\d+) project=(\d+) mutations=(\d+)',text)
        assert match; report['Contracts']=dict(zip(('Copy','Equal','Box','Project','RefusedMutations'),map(int,match.groups())))
        products.append(dict(Path=str(proof),Sha256=sha(proof)))
        memory_object=out/'memory.o'; run('memory-build',[cc,*flags,'-O0','-c',memory,'-o',memory_object])
        products.append(dict(Path=str(memory_object),Sha256=sha(memory_object)))
        if a.fixture=='stdlib_native_value_collections.xl':
            boundary=root/'test/cases/stdlib_native_value_extract.c'
            report['Inputs'].append(dict(Path=str(boundary),Sha256=sha(boundary)))
            for opt in ('O0','O2'):
                proof=out/('extract-'+opt+('.exe' if os.name=='nt' else ''))
                run('extract-'+opt+'-build',[cc,*flags,'-'+opt,boundary,memory_object,*objects,runtime,*links,'-o',proof])
                run('extract-'+opt,[proof]); products.append(dict(Path=str(proof),Sha256=sha(proof)))
        for opt in ('O0','O2'):
            product=out/(opt+('.exe' if os.name=='nt' else ''))
            run(opt+'-build',[cc,*flags,'-'+opt,'-include',generated,host,memory_object,*objects,runtime,*links,'-o',product])
            products.append(dict(Path=str(product),Sha256=sha(product))); run(opt+'-native-keys',[product])
            fault=out/(opt+'-fault'+('.exe' if os.name=='nt' else ''))
            symbols=('xl6SlotMapKeyCopyValueV1','xl6SlotMapKeyUnboxV1','xl6SlotMapKeyBoxV1','xl6ArrayExtractNativeValueV1')
            wraps=['-Wl,--wrap='+symbol for symbol in symbols]
            run(opt+'-fault-build',[cc,*flags,'-'+opt,'-DXL6_NATIVE_VALUE_TEST_FAULTS','-include',generated,
                host,memory_object,*objects,runtime,*links,*wraps,'-o',fault])
            products.append(dict(Path=str(fault),Sha256=sha(fault)))
            for lane in (('copy','project','box','extract','oom') if a.fixture=='stdlib_native_value_collections.xl' else ('copy','project','box','oom')):
                for at in range(256):
                    message=run(opt+'-'+lane+'-'+str(at),[fault,lane,at])
                    assert 'prefix='+lane+' at='+str(at)+' hit=' in message
                    if 'prefix='+lane+' at='+str(at)+' hit=0' in message: break
                else: raise AssertionError('No final no-hit prefix: '+lane)
                prefixes.append(dict(Optimization=opt,Lane=lane,Runs=at+1,Complete=True))
        mutants=(('copy-owner','xl6_slot_map_key_value',
            '{ return xl6NativeLeafValueCopyV1(&profile,&input,output); }','{ *output=input; return true; }'),
            ('drop-owner','xl6_slot_map_key','{ keyDrop(&value,xl6SlotMapKeyTypeV1()); }','{ (void)value; }'),
            ('instance-domain','xl6_slot_map_key','{ return left.instance==right.instance && left.slot==right.slot; }',
             '{ return left.slot==right.slot; }'))
        if a.fixture=='stdlib_native_value_collections.xl':
            mutants+=(('wrong-language-id','xl6_slot_map_key_value',
                '#define KEY_VALUE_ID XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID',
                '#define KEY_VALUE_ID UINT64_C(0x5BF65A92DC10A954)'),
                ('descriptor-empty-only','xl6_slot_map_key',
                'const XL6_SlotMapKeyV1* input=source; XL6_SlotMapKeyV1* output=target;\n    if ((!output->instance!=!output->slot)',
                'const XL6_SlotMapKeyV1* input=source; XL6_SlotMapKeyV1* output=target;\n    if ((output->instance || output->slot)'))
        for name,stem,old,new in mutants:
            original=(root/'src/runtime'/ (stem+'.c')).read_text(encoding='utf-8-sig'); assert original.count(old)==1
            mutation=out/(name+'.c'); mutation.write_text(original.replace(old,new)); obj=out/(name+'.o')
            run(name+'-object',[cc,*flags,'-O2','-c',mutation,'-o',obj])
            replacement=[obj if Path(o).name==stem+'.o' else o for o in objects]
            assert sum(Path(o)==obj for o in replacement)==1
            product=out/(name+('.exe' if os.name=='nt' else ''))
            run(name+'-build',[cc,*flags,'-O2','-include',generated,host,memory_object,*replacement,runtime,*links,'-o',product])
            run(name+'-reject',[product],negative=True)
            products.extend(dict(Path=str(f),Sha256=sha(f)) for f in (mutation,obj,product))
            controls.append(dict(Name=name,Rejected=True,Source=str(mutation),SourceSha256=sha(mutation),
                Product=str(product),ProductSha256=sha(product)))
        report['Complete']=True
    finally:
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    assert report['Complete'] and report['InputsUnchanged']; print('Owning native value source/IR/C: checked copies, real keys, 1000 lifetimes and malformed IR refusal')
if __name__=='__main__': main()
