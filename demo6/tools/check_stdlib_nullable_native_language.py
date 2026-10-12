"""Owning nullable native keys in actual language programs: strict selected C
O0/O2, checked caller-thread allocation prefixes, JIT/module/AOT, corrupt IR
and real bad compiler controls; optional full selected Linux ASan/UBSan.
Not all nullable/capture/sequence domains or full53/shipping acceptance.
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
    fixture=root/'test/cases/stdlib_stack_nullable_native_value.xl';host=root/'test/cases/stdlib_stack_nullable_native_value_host.c'
    contract=root/'test/cases/xlir_nullable_native_language_contract.c';planner_source=root/'test/cases/xlir_artifact_runtime_contract.c'
    memory=root/'test/support/module_product_memory.c';manifest,sdk,_=stage_private_sdk(root)
    pinned.update((Path(__file__).resolve(),Path(cc),Path(nm),runtime,fixture,host,contract,planner_source,memory,manifest))
    pinned.update(Path(row[key]) for row in sdk for key in ('Source','Path'))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir();rows=[];products=[];prefixes=[];controls=[];env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Prefixes=prefixes,NegativeControls=controls,
        LifetimesPerOptimization=100,SelectedNativeClosureSanitized=args.sanitize,FaultScope='complete synchronous caller-thread allocation prefixes',
        Full53Acceptance=False,ShippingArtifact=False,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include','-Ilib/tcc','-Ilib/xruntime','-Ilib/xrt','-I'+str(public/'include'),
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
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
        present='if(nullableNative){';pod='type->cAbi=XL6_IR_C_ABI_NATIVE_VALUE;type->ownershipAbi=XL6_IR_OWNERSHIP_ABI_MANAGED;'
        view='if(source && xl6IrNullableNativeValuePlan(&module->types,xl6IrTypeGet(&module->types,source->type)))\n                return XL6_IR_OWNERSHIP_BORROW;'
        assert body.count(present)==body.count(pod)==body.count(view)==1
        for name,changed in [('missing-native-nullable',body.replace(present,'if(0 && nullableNative){')),
            ('pod-native-nullable',body.replace(pod,pod.replace('MANAGED','TRIVIAL'))),
            ('owned-native-view',body.replace(view,view.replace('BORROW','MOVE')))]:
            source=out/(name+'.c');source.write_text(changed,encoding='utf-8');obj=out/(name+'.o');binary=out/(name+extension)
            run(name+'-build',[cc,*flags,'-Isrc/ir','-O0','-c',source,'-o',obj]);product(source);product(obj)
            replaced=[obj if Path(path).name=='xl6_ir.o' else path for path in objects]
            run(name+'-link',[cc,*flags,'-O0',contract,*replaced,runtime,*links,'-o',binary]);product(binary)
            run(name+'-reject',[binary,fixture],True);controls.append(dict(Name=name,Rejected=True))
        collection=(root/'src/cgen/xl6_ir_cgen_collection_emit.c').read_text(encoding='utf-8-sig')
        projection='xl6IrOptionalResultPayloadTagged(module,product->params[1],instruction->payloadType)'
        assert collection.count(projection)==1
        bad=out/'lost-native-payload-field.inc';bad.write_text(collection.replace(projection,'0'),encoding='utf-8');product(bad)
        cgen=(root/'src/cgen/xl6_ir_cgen.c').read_text(encoding='utf-8-sig');include='"src/cgen/xl6_ir_cgen_collection_emit.c"';assert cgen.count(include)==1
        source=out/'lost-native-payload-field.c';source.write_text(cgen.replace(include,'"'+bad.as_posix()+'"'),encoding='utf-8');product(source)
        obj=out/'lost-native-payload-field.o';binary=out/('lost-native-payload-field'+extension)
        run('bad-cgen-build',[cc,*flags,'-Isrc/cgen','-O0','-c',source,'-o',obj]);product(obj)
        replaced=[obj if Path(path).name=='xl6_ir_cgen.o' else path for path in objects]
        run('bad-cgen-link',[cc,*flags,'-O0',contract,*replaced,runtime,*links,'-o',binary]);product(binary)
        run('bad-cgen-reject',[binary,fixture],True);controls.append(dict(Name='lost-native-payload-field',Rejected=True))
        planner=out/('plan'+extension);run('planner-build',[cc,*flags,'-O0',planner_source,*objects,runtime,*links,'-o',planner]);product(planner)
        implementation=out/'native.c';text=run('native-plan',[planner,'--emit-native',fixture.as_posix(),implementation])
        plan=[json.loads(line) for line in text.splitlines() if line.startswith('{"Fixture":')];assert len(plan)==1
        suppliers=plan[0]['Sources'];report['Sources']=suppliers;assert 'src/runtime/xl6_nullable_slot_map_key.c' in suppliers
        raw=implementation.read_text();assert 'XRT_MODULE_ALL' not in raw and 'XRUNTIME_MODULE_ALL' not in raw
        implementation.write_text('#define XRT_MODULE_MEMORY_DEBUG\n'+raw);product(implementation)
        for optimization in ('O0','O2'):
            providers=[]
            for index,source in enumerate([implementation,*[root/name for name in suppliers],memory]):
                obj=out/(optimization+'-'+str(index)+'.o');run(obj.stem,[cc,*flags,'-'+optimization,'-c',source,'-o',obj]);providers.append(obj);product(obj)
            binary=out/(optimization+extension);run(optimization+'-link',[cc,*flags,'-'+optimization,'-include',generated,host,*providers,*systems,'-o',binary]);product(binary)
            run(optimization+'-lifetimes',[binary]);assert 'xlcCreateContext' not in run(optimization+'-symbols',[nm,'--defined-only',binary])
            for at in range(4096):
                text=run(optimization+'-prefix-'+str(at),[binary,at])
                if 'hit=0' in text:break
            else:raise AssertionError('unfinished allocation prefix')
            assert at>0;prefixes.append(dict(Optimization=optimization,Runs=at+1,Complete=True))
        for mode in ('run','module'):run(mode,[driver,fixture,mode,out/mode])
        exe=out/'aot.exe';run('aot-build',[driver,fixture,'exe',out/'aot' if os.name=='nt' else exe]);run('aot-run',[exe]);product(exe)
        if args.sanitize:
            sf=flags+['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie'];sl=['-fsanitize=address,undefined','-no-pie']
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1');providers=[]
            for index,source in enumerate([implementation,*[root/name for name in suppliers],memory]):
                obj=out/('san-'+str(index)+'.o');run(obj.stem,[cc,*sf,'-O1','-c',source,'-o',obj]);providers.append(obj);product(obj)
            binary=out/'san';run('san-link',[cc,*sf,'-O1','-include',generated,host,*providers,*systems,*sl,'-o',binary]);run('san-lifetimes',[binary]);product(binary)
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);report['Complete']=report['InputsUnchanged']
    finally:
        report['PhaseCount']=len(rows);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'];print('Nullable native language ownership: verified',flush=True)
if __name__=='__main__':main()
