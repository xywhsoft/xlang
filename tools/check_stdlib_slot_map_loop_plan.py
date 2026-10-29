"""SlotMap key-only/paired typed plans and closed native backend admission.
Fresh public-source build, exact input hashes, O0/O2 contract mutations and real
bad-implementation controls. Full lifetime/fault/pruning acceptance is separate.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); root=args.root.resolve(); public=args.public_directory.resolve(); out=args.out.resolve()
    build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'))
    driver=Path(build['Driver']); cc=shutil.which('gcc')
    assert cc and not out.exists() and build['SourceXrt'] and not build['RuntimeOnly']
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    fixture=root/'test/cases/stdlib_slot_map_loop_plan.xl'
    contract=root/'test/cases/xlir_slot_map_loop_plan_contract.c'
    files={Path(__file__).resolve(),public/'build.json',driver,Path(cc),runtime,fixture,contract}
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(),row['Path']; files.add(Path(row['Path']))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir(); rows=[]; products=[]; env=dict(os.environ)
    if os.name=='nt': shutil.copy2(runtime,out/'xl.dll')
    else: env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,
        TypedSlotMapLoopPlans=False,LanguagePairedIterationAcceptance=False,Full53Acceptance=False,
        ShippingSdkAcceptance=False,Scope=__doc__)
    def run(name,cmd,success=True):
        p=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=600)
        log=out/(name+'.log'); log.write_bytes(p.stdout+p.stderr)
        rows.append(dict(Name=name,Exit=p.returncode,Log=str(log),LogSha256=sha(log)))
        assert (p.returncode==0)==success,(name,(p.stdout+p.stderr)[-7000:])
        return (p.stdout+p.stderr).decode(errors='replace')
    try:
        typed=out/'typed.json'; run('typed-plan',[driver,fixture,'ir-typed',typed])
        ir=json.loads(typed.read_text(encoding='utf-8-sig'))
        loops=[n for n in ir['nodes'] if n['loopAbiName'].startswith('slot-map-iterator-')]
        assert len(loops)==18 and sum(n['loopAbiName']=='slot-map-iterator-key' for n in loops)==3
        assert sum(n['loopKeyAbiName']=='discard' for n in loops)==3
        for n in loops:
            assert n['loopKeyAbiName'] in ('native-value-copy','discard')
            assert n['loopErrorAbiName']=='may-raise' and n['loopResourceAbiName']=='xvalue-iterator-destroy'
            assert n['loopKeyLifetimeAbiName']==('none' if n['loopKeyAbiName']=='discard' else 'owned-iteration-release')
        report['TypedSlotMapLoopPlans']=True; products.append(dict(Path=str(typed),Sha256=sha(typed)))
        admitted=[]
        modes=('ir-lowered','cgen-default','cgen-xlir','emit') if os.name=='nt' else ('ir-lowered','cgen-default')
        for mode in modes:
            target=out/('backend-'+mode)
            run('backend-'+mode,[driver,fixture,mode,target])
            if mode=='emit': assert Path(str(target)+'.c').stat().st_size
            else: assert target.stat().st_size
            admitted.append(mode)
        report['NativeBackendAdmissionModes']=admitted
        negatives={
            'key-not-int':'auto slots=SlotMap::<int>()\nfor key in slots\n slots.get(1)\nend\n',
            'value-not-key':'auto slots=SlotMap::<int>()\nfor key,value in slots\n slots.contains(value)\nend\n',
            'three-targets':'auto slots=SlotMap::<int>()\nfor a,b,c in slots\nend\n',
            'closure-domain':'auto slots=SlotMap::<func(int) as int>()\nfor key,value in slots\nend\n'}
        for name,body in negatives.items():
            source=out/(name+'.xl'); source.write_text(body,encoding='utf-8')
            text=run(name,[driver,source,'ir-typed',out/(name+'.json')],False)
            assert 'failed:' in text
        report['RejectedSourceContracts']=list(negatives)
        objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
        flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include','-Isrc/runtime','-Isrc/ir',
            '-Ilib/xruntime','-Ilib/xrt','-Ilib/tcc','-I'+str(public/'include'),'-DXL_STATIC_EMBED',
            '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
            '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
        links=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
        if os.name!='nt': flags+=['-D_GNU_SOURCE']
        ext='.exe' if os.name=='nt' else ''
        counts=[]
        for opt in ('O0','O2'):
            binary=out/('contract-'+opt+ext)
            run('contract-build-'+opt,[cc,*flags,'-'+opt,contract,*objects,runtime,*links,'-o',binary])
            text=run('contract-'+opt,[binary,fixture])
            match=re.search(r'plans=(\d+) key-only=(\d+) pairs=(\d+) discard=(\d+) mutations=(\d+); native backend admitted',text)
            assert match; counts.append(list(map(int,match.groups())))
            products.append(dict(Path=str(binary),Sha256=sha(binary)))
        assert counts[0]==counts[1] and counts[0]==[18,3,15,3,216]
        report['LoopPlans'],report['KeyOnly'],report['Pairs'],report['DiscardedKeys'],report['RefusedIrMutations']=counts[0]
        original=(root/'src/ir/xl6_ir.c').read_text(encoding='utf-8-sig')
        mutations={
            'integer-key':('''        : slotMapIterator
            ? XL6_IR_LOOP_KEY_NATIVE_VALUE_COPY''','''        : slotMapIterator
            ? XL6_IR_LOOP_KEY_INTEGER_I64''','loop->loopKeyAbi==XL6_IR_LOOP_KEY_NATIVE_VALUE_COPY'),
            'trivial-key-lifetime':('''    case XL6_IR_LOOP_KEY_NATIVE_VALUE_COPY:
        return XL6_IR_LOOP_TARGET_LIFETIME_OWNED_ITERATION_RELEASE;''','''        return XL6_IR_LOOP_TARGET_LIFETIME_OWNED_ITERATION_RELEASE;
    case XL6_IR_LOOP_KEY_NATIVE_VALUE_COPY:
        return XL6_IR_LOOP_TARGET_LIFETIME_TRIVIAL_REPLACE;''','loop->loopKeyLifetimeAbi==XL6_IR_LOOP_TARGET_LIFETIME_OWNED_ITERATION_RELEASE')}
        controls=[]
        for name,(old,new,oracle) in mutations.items():
            assert original.count(old)==1,name
            source=out/(name+'.c'); source.write_text(original.replace(old,new),encoding='utf-8')
            obj=out/(name+'.o'); binary=out/(name+ext)
            run(name+'-compile',[cc,*flags,'-O0','-c',source,'-o',obj])
            selected=[p for p in objects if Path(p).name!='xl6_ir.o']; assert len(selected)==len(objects)-1
            run(name+'-link',[cc,*flags,'-O0',contract,*selected,obj,runtime,*links,'-o',binary])
            text=run(name+'-refused',[binary,fixture],False); assert oracle in text,(name,text[-2500:])
            controls.append(name)
        report['BadImplementationControls']=controls
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    finally:
        report['PhaseCount']=len(rows)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('SlotMap typed loop plans: 18 shapes, 216 IR mutations, two real bad controls; native backend admitted')
if __name__=='__main__': main()
