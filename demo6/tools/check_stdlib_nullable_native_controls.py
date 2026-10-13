"""Nullable native defaults, static/dynamic casts and lazy coalescing: selected C
O0/O2, checked caller-thread allocation prefixes, JIT/module/AOT, corrupt IR
and real bad compiler controls; optional full selected Linux ASan/UBSan.
Authenticated mismatch/partial copy native units included. Not capture/sequence
domains or full53/shipping acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
from check_stdlib_slot_map import stage_private_sdk
from xlir_public_build_inputs import verify_public_build
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true');args=parser.parse_args()
    root,public,out=args.root.resolve(),args.public_directory.resolve(),args.out.resolve()
    cc,nm=shutil.which('gcc'),shutil.which('nm');assert cc and nm and not out.exists() and (not args.sanitize or os.name!='nt')
    build,pinned,_=verify_public_build(root,public);driver=Path(build['Driver'])
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    fixture=root/'test/cases/stdlib_stack_nullable_native_controls.xl';host=root/'test/cases/stdlib_stack_nullable_native_controls_host.c'
    contract=root/'test/cases/xlir_nullable_native_controls_contract.c';planner_source=root/'test/cases/xlir_artifact_runtime_contract.c'
    native_host=root/'test/cases/stdlib_nullable_native_try_host.c'
    memory=root/'test/support/module_product_memory.c';manifest,sdk,_=stage_private_sdk(root)
    pinned.update((Path(__file__).resolve(),Path(cc),Path(nm),runtime,fixture,host,contract,native_host,planner_source,memory,manifest))
    pinned.update(Path(row[key]) for row in sdk for key in ('Source','Path'))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir();rows=[];products=[];prefixes=[];controls=[];unit_prefixes=[];env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Prefixes=prefixes,NegativeControls=controls,
        LifetimesPerOptimization=100,SelectedNativeClosureSanitized=args.sanitize,FaultScope='complete synchronous caller-thread allocation prefixes',
        NativeUnitPrefixes=unit_prefixes,Full53Acceptance=False,ShippingArtifact=False,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include','-Ilib/tcc','-Ilib/xruntime','-Ilib/xrt','-I'+str(public/'include'),
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=systems+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)]);extension='.exe' if os.name=='nt' else ''
    objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)))
    def run(name,command,bad=False):
        result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=180);data=result.stdout+result.stderr
        log=out/(name+'.log');log.write_bytes(data);rows.append(dict(Name=name,Exit=result.returncode,ExpectedRejection=bad,Log=str(log),LogSha256=sha(log)))
        assert (result.returncode!=0 if bad else result.returncode==0),(name,data[-4000:])
        if bad:assert b'assert' in data.lower(),(name,data[-4000:])
        return data.decode(errors='replace')
    try:
        generated=out/'program.c';repeat=out/'repeat.c'
        run('generate',[driver,fixture,'cgen-default',generated]);run('repeat',[driver,fixture,'cgen-default',repeat]);assert sha(generated)==sha(repeat)
        for path in (generated,repeat):product(path)
        proof=out/('contract'+extension);run('contract-build',[cc,*flags,'-O0',contract,*objects,runtime,*links,'-o',proof]);product(proof)
        report['IrContracts']=run('contract-run',[proof,fixture])
        body=(root/'src/ir/xl6_ir.c').read_text(encoding='utf-8-sig')
        include='"src/ir/xl6_ir_native_value.c"';assert body.count(include)==1
        profile=(root/'src/ir/xl6_ir_native_value.c').read_text(encoding='utf-8-sig')
        selector='case XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_TRY:return plan->tryProjectSymbol;';assert profile.count(selector)==1
        bad=out/'wrong-try-selector.inc';bad.write_text(profile.replace(selector,selector.replace('tryProjectSymbol','projectSymbol')),encoding='utf-8');product(bad)
        ownership=(root/'src/ir/xl6_ir_ownership.c').read_text(encoding='utf-8-sig');op='    case XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_TRY:\n';assert ownership.count(op)==1
        bad_owner=out/'unknown-try-input.inc';bad_owner.write_text(ownership.replace(op,''),encoding='utf-8');product(bad_owner)
        owner_include='"xl6_ir_ownership.c"';assert body.count(owner_include)==1
        for name,changed in [('wrong-try-selector',body.replace(include,'"'+bad.as_posix()+'"')),
                             ('unknown-try-input',body.replace(owner_include,'"'+bad_owner.as_posix()+'"'))]:
            source=out/(name+'.c');source.write_text(changed,encoding='utf-8');obj=out/(name+'.o');binary=out/(name+extension)
            run(name+'-build',[cc,*flags,'-Isrc/ir','-O0','-c',source,'-o',obj]);product(source);product(obj)
            replaced=[obj if Path(path).name=='xl6_ir.o' else path for path in objects]
            run(name+'-link',[cc,*flags,'-O0',contract,*replaced,runtime,*links,'-o',binary]);product(binary)
            run(name+'-reject',[binary,fixture],True);controls.append(dict(Name=name,Rejected=True))
        sem=(root/'src/sem/xl6_sem.c').read_text(encoding='utf-8-sig')
        admitted='xl6TypeRegistryIsNativeValueAbi(typeInfo->abiId))\n        return concreteType->childCount == 0;';assert sem.count(admitted)==1
        source=out/'old-sem-gate.c';source.write_text(sem.replace(admitted,admitted.replace('return concreteType','return nullableInner == NULL && concreteType')),encoding='utf-8')
        obj=out/'old-sem-gate.o';binary=out/('old-sem-gate'+extension);run('old-sem-build',[cc,*flags,'-Isrc/sem','-O0','-c',source,'-o',obj]);product(source);product(obj)
        replaced=[obj if Path(path).name=='xl6_sem.o' else path for path in objects]
        run('old-sem-link',[cc,*flags,'-O0',contract,*replaced,runtime,*links,'-o',binary]);product(binary)
        run('old-sem-reject',[binary,fixture],True);controls.append(dict(Name='old-sem-gate',Rejected=True))
        planner=out/('plan'+extension);run('planner-build',[cc,*flags,'-O0',planner_source,*objects,runtime,*links,'-o',planner]);product(planner)
        implementation=out/'native.c';text=run('native-plan',[planner,'--emit-native',fixture.as_posix(),implementation])
        plan=[json.loads(line) for line in text.splitlines() if line.startswith('{"Fixture":')];assert len(plan)==1
        suppliers=plan[0]['Sources'];report['Sources']=suppliers;assert 'src/runtime/xl6_nullable_slot_map_key.c' in suppliers
        raw=implementation.read_text();assert 'XRT_MODULE_ALL' not in raw and 'XRUNTIME_MODULE_ALL' not in raw
        implementation.write_text('#define XRT_MODULE_MEMORY_DEBUG\n'+raw);product(implementation)
        all_providers={};wrap=['-Wl,--wrap=xl6SlotMapKeyUnboxV1']
        for optimization in ('O0','O2'):
            providers=[]
            for index,source in enumerate([implementation,*[root/name for name in suppliers],memory]):
                obj=out/(optimization+'-'+str(index)+'.o');run(obj.stem,[cc,*flags,'-'+optimization,'-c',source,'-o',obj]);providers.append(obj);product(obj)
            all_providers[optimization]=providers
            binary=out/(optimization+extension);run(optimization+'-link',[cc,*flags,'-'+optimization,'-include',generated,host,*providers,*systems,'-o',binary]);product(binary)
            run(optimization+'-lifetimes',[binary]);assert 'xlcCreateContext' not in run(optimization+'-symbols',[nm,'--defined-only',binary])
            for at in range(4096):
                text=run(optimization+'-prefix-'+str(at),[binary,at])
                if 'hit=0' in text:break
            else:raise AssertionError('unfinished allocation prefix')
            assert at>0;prefixes.append(dict(Optimization=optimization,Runs=at+1,Complete=True))
            unit=out/(optimization+'-native'+extension);run(optimization+'-native-link',[cc,*flags,'-'+optimization,native_host,*providers,*systems,*wrap,'-o',unit]);product(unit)
            run(optimization+'-native-vectors',[unit])
            for lane in ('copy','partial','diagnostic','oom'):
                for at in range(128):
                    text=run(optimization+'-native-'+lane+'-'+str(at),[unit,lane,at])
                    if 'hit=0' in text:break
                else:raise AssertionError('unfinished native prefix')
                if lane!='oom':assert at>0
                unit_prefixes.append(dict(Optimization=optimization,Lane=lane,Runs=at+1,Complete=True))
        native=(root/'src/runtime/xl6_nullable_slot_map_key.c').read_text(encoding='utf-8-sig')
        authentication='xl6SlotMapKeyValueAdmitsV1(input)';assert native.count(authentication)==1
        try_signature='bool xl6NullableSlotMapKeyTryUnboxV1('
        assert native.count(try_signature)==1
        before_try,try_body=native.split(try_signature)
        finish='return finish(&scope,ok);';assert try_body.count(finish)==1
        index=suppliers.index('src/runtime/xl6_nullable_slot_map_key.c')+1
        for name,changed,argv in [('type-id-only-auth',native.replace(authentication,'xrtValueTypeId(input)==XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID'),[]),
            ('false-success-status',before_try+try_signature+try_body.replace(finish,'(void)finish(&scope,ok);return true;'),['copy',0])]:
            source=out/(name+'.c');source.write_text(changed,encoding='utf-8');obj=out/(name+'.o');binary=out/(name+extension)
            run(name+'-build',[cc,*flags,'-Isrc/runtime','-O0','-c',source,'-o',obj]);product(source);product(obj)
            providers=all_providers['O0'][:];providers[index]=obj
            run(name+'-link',[cc,*flags,'-O0',native_host,*providers,*systems,*wrap,'-o',binary]);product(binary)
            run(name+'-reject',[binary,*argv],True);controls.append(dict(Name=name,Rejected=True))
        for mode in ('run','module'):run(mode,[driver,fixture,mode,out/mode])
        exe=out/'aot.exe';run('aot-build',[driver,fixture,'exe',out/'aot' if os.name=='nt' else exe]);run('aot-run',[exe]);product(exe)
        if args.sanitize:
            sf=flags+['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie'];sl=['-fsanitize=address,undefined','-no-pie']
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1');providers=[]
            for index,source in enumerate([implementation,*[root/name for name in suppliers],memory]):
                obj=out/('san-'+str(index)+'.o');run(obj.stem,[cc,*sf,'-O1','-c',source,'-o',obj]);providers.append(obj);product(obj)
            binary=out/'san';run('san-link',[cc,*sf,'-O1','-include',generated,host,*providers,*systems,*sl,'-o',binary]);run('san-lifetimes',[binary]);product(binary)
            unit=out/'san-native';run('san-native-link',[cc,*sf,'-O1',native_host,*providers,*systems,*wrap,*sl,'-o',unit]);product(unit);run('san-native-vectors',[unit])
            for lane in ('copy','partial','diagnostic','oom'):run('san-native-'+lane,[unit,lane,0])
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);report['Complete']=report['InputsUnchanged']
    finally:
        report['PhaseCount']=len(rows);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'];print('Nullable native controls and authenticated try-casts: verified',flush=True)
if __name__=='__main__':main()
