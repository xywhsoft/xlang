"""Public SlotMap core calls: source JIT, strict standalone C and checked faults.
Not snapshot/paired-iterator/frame/persistence or pruned shipping acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args(); root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve()
    build=json.loads((public/'build.json').read_text(encoding='utf-8-sig')); driver=Path(build['Driver'])
    cc=shutil.which('gcc'); assert cc and not out.exists() and build['SourceXrt'] and not build['RuntimeOnly']
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    source=root/'test/cases/stdlib_slot_map.xl'; host=root/'test/cases/stdlib_slot_map_host.c'
    contract=root/'test/cases/xlir_slot_map_contract.c'; memory=root/'test/support/module_product_memory.c'
    errors=root/'test/cases/stdlib_slot_map_errors.xl'
    files={Path(__file__).resolve(),public/'build.json',driver,Path(cc),runtime,source,errors,host,contract,memory}
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]; out.mkdir(); rows=[]; products=[]; prefixes=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Prefixes=prefixes,
        LanguageSlotMapCoreCalls=True,LanguageSlotMapFullCutover=False,Full53Acceptance=False,
        PrunedShippingArtifact=False,Scope=__doc__)
    env=dict(os.environ)
    if os.name!='nt': env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    else: shutil.copy2(runtime,out/'xl.dll')
    def run(name,cmd):
        p=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=600)
        log=out/(name+'.log'); log.write_bytes(p.stdout+p.stderr)
        rows.append(dict(Name=name,Exit=p.returncode,Log=str(log),LogSha256=sha(log)))
        assert p.returncode==0,(name,(p.stdout+p.stderr)[-7000:])
        return (p.stdout+p.stderr).decode(errors='replace')
    try:
        generated=out/'slot-map.c'; repeat=out/'repeat.c'
        run('generate',[driver,source,'cgen-default',generated]); run('repeat',[driver,source,'cgen-default',repeat]); assert sha(generated)==sha(repeat)
        body=generated.read_text(encoding='utf-8-sig')
        assert re.search(r'xl6SlotMapInsertV1\(v\d+,&slot_map_layout,&v\d+,&v\d+\)',body)
        assert re.search(r'xl6SlotMapReadRequiredV1\(v\d+,&slot_map_layout,v\d+,&v\d+\)',body)
        assert 'xl6SlotMapKeyBoxV1(' not in body and 'xl6SlotMapKeyUnboxV1(' not in body
        assert '#include <__xl_runtime/xl6_slot_map.h>' in body
        run('public-jit',[driver,source,'run',out/'jit'])
        run('checked-errors',[driver,errors,'run',out/'errors-jit'])
        negatives={
            'key-type':'auto map = SlotMap::<int>()\nmap.contains(1)\n',
            'value-type':'auto map = SlotMap::<int>()\nmap.insert("bad")\n',
            'arity':'auto map = SlotMap::<int>()\nauto key = map.insert(1)\nmap.set(key)\n',
            'closure-domain':'auto map = SlotMap::<func(int) as int>()\n'}
        report['RejectedSourceContracts']=[]
        for label,body in negatives.items():
            fixture=out/(label+'.xl'); fixture.write_text(body,encoding='utf-8')
            p=subprocess.run([str(driver),str(fixture),'cgen-default',str(out/(label+'.c'))],cwd=root,env=env,capture_output=True,timeout=120)
            log=out/(label+'.log'); log.write_bytes(p.stdout+p.stderr)
            rows.append(dict(Name=label,Exit=p.returncode,Log=str(log),LogSha256=sha(log)))
            assert p.returncode and b'failed:' in p.stdout+p.stderr
            report['RejectedSourceContracts'].append(label)
        for mode in ('ir-typed','ir-lowered'):
            target=out/(mode+'.json'); run(mode,[driver,source,mode,target]); products.append(dict(Path=str(target),Sha256=sha(target)))
        objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
        flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include','-Isrc/runtime','-I'+str(public/'include'),'-DXL_STATIC_EMBED',
            '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable',
            '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
        links=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
        if os.name!='nt': flags+=['-D_GNU_SOURCE']
        ext='.exe' if os.name=='nt' else ''
        proof=out/('contract'+ext); run('contract-build',[cc,*flags,'-Ilib/tcc','-O0',contract,*objects,runtime,*links,'-o',proof])
        text=run('contract',[proof,source]); match=re.search(r'slot-map calls=(\d+) mutations=(\d+)',text); assert match
        report['ContractCalls'],report['RefusedMutations']=map(int,match.groups())
        memory_object=out/'memory.o'; run('memory-build',[cc,*flags,'-O0','-c',memory,'-o',memory_object])
        for opt in ('O0','O2'):
            exe=out/(opt+ext); run(opt+'-build',[cc,*flags,'-'+opt,'-include',generated,host,memory_object,*objects,runtime,*links,'-o',exe])
            run(opt+'-lifetimes',[exe]); products.append(dict(Path=str(exe),Sha256=sha(exe)))
            fault=out/(opt+'-fault'+ext); wraps=['-Wl,--wrap='+s for s in ('xl6SlotMapInsertV1','xl6SlotMapReadRequiredV1','xl6SlotMapSizeV1')]
            run(opt+'-fault-build',[cc,*flags,'-'+opt,'-DXL6_SLOT_MAP_TEST_FAULTS','-include',generated,host,memory_object,*objects,runtime,*links,*wraps,'-o',fault])
            products.append(dict(Path=str(fault),Sha256=sha(fault)))
            for lane in ('insert','get','size','oom'):
                for at in range(512):
                    text=run(opt+'-'+lane+'-'+str(at),[fault,lane,at]); assert 'prefix='+lane+' at='+str(at)+' hit=' in text
                    if 'hit=0' in text: break
                else: raise AssertionError('Unfinished prefix '+lane)
                prefixes.append(dict(Optimization=opt,Lane=lane,Runs=at+1,Complete=True))
        for p in (generated,repeat,proof,memory_object): products.append(dict(Path=str(p),Sha256=sha(p)))
        report['LifetimesPerOptimization']=1000; report['Complete']=True
    finally:
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'] and report['InputsUnchanged']; print('public SlotMap core calls verified',flush=True)
if __name__=='__main__': main()
