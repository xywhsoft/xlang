"""Native owning records in Stack: closed IR, exact physical slots,
independent C O0/O2 and caller-thread allocation prefixes, JIT/module/AOT,
actual old-IR/Cgen rejection controls and full selected Linux ASan/UBSan.
Not nullable-native/Future/callable Stack domains or full53/shipping acceptance.
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
    fixture=root/'test/cases/stdlib_stack_native_value.xl';host=root/'test/cases/stdlib_stack_native_value_host.c'
    contract=root/'test/cases/xlir_stack_native_value_contract.c';planner_source=root/'test/cases/xlir_artifact_runtime_contract.c'
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
        proof=out/('contract'+extension);run('contract-build',[cc,*flags,'-O0',contract,*objects,runtime,*links,'-o',proof]);product(proof)
        report['IrContracts']=run('contract-run',[proof,fixture])
        body=(root/'src/ir/xl6_ir.c').read_text(encoding='utf-8-sig')
        begin=body.index('static int xl6IrStackConstructionContractDepth(');end=body.index('int xl6IrStackConstructionContract(',begin)
        function=body[begin:end]
        native='    case XL6_IR_SEQUENCE_LAYOUT_THREAD_REFERENCE_V1:\n    case XL6_IR_SEQUENCE_LAYOUT_NATIVE_VALUE_V1: return 1;'
        maximum='xl6IrSlotMapResourceOpIs(xl6IrSlotMapResourceOp(instruction->runtimeOp)) ||\n                          (instruction->runtimeOp>=XL6_IR_RUNTIME_STACK_CREATE &&\n                           instruction->runtimeOp<=XL6_IR_RUNTIME_STACK_CAPACITY)'
        assert function.count(native)==1 and body.count(maximum)==1
        for name,changed in [('missing-native-domain',body[:begin]+function.replace(native,'    case XL6_IR_SEQUENCE_LAYOUT_THREAD_REFERENCE_V1: return 1;')+body[end:]),
                             ('old-payload-limit',body.replace(maximum,'xl6IrSlotMapResourceOpIs(xl6IrSlotMapResourceOp(instruction->runtimeOp))'))]:
            bad=out/(name+'.c');bad.write_text(changed,encoding='utf-8');obj=out/(name+'.o');binary=out/(name+extension)
            run(name+'-build',[cc,*flags,'-Isrc/ir','-O0','-c',bad,'-o',obj]);replaced=[obj if Path(path).name=='xl6_ir.o' else path for path in objects]
            run(name+'-link',[cc,*flags,'-O0',contract,*replaced,runtime,*links,'-o',binary]);run(name+'-refused',[binary,fixture],True)
            for path in (bad,obj,binary):product(path)
            controls.append(name)
        emit=(root/'src/cgen/xl6_ir_cgen_collection_emit.c').read_text(encoding='utf-8-sig')
        start=emit.index('static int xl6IrCgenEmitStackRuntime(');left,right=emit[:start],emit[start:]
        needle='payloadAbi = xl6IrCgenSequenceTransferAbiToken(';assert right.count(needle)==1
        bad=out/'old-channel-token-emit.c';bad.write_text(left+right.replace(needle,'payloadAbi = xl6IrCgenChannelPayloadAbiToken('),encoding='utf-8')
        source=out/'old-channel-token.c';body=(root/'src/cgen/xl6_ir_cgen.c').read_text(encoding='utf-8-sig')
        needle='"src/cgen/xl6_ir_cgen_collection_emit.c"';assert body.count(needle)==1
        source.write_text(body.replace(needle,'"'+bad.as_posix()+'"'),encoding='utf-8');obj=out/'old-channel-token.o';binary=out/('old-channel-token'+extension)
        run('old-token-build',[cc,*flags,'-Isrc/cgen','-O0','-c',source,'-o',obj]);replaced=[obj if Path(path).name=='xl6_ir_cgen.o' else path for path in objects]
        run('old-token-link',[cc,*flags,'-O0',contract,*replaced,runtime,*links,'-o',binary]);run('old-token-refused',[binary,fixture],True)
        for path in (bad,source,obj,binary):product(path)
        controls.append('old-channel-token')
        planner=out/('plan'+extension);run('planner-build',[cc,*flags,'-O0',planner_source,*objects,runtime,*links,'-o',planner]);product(planner)
        generated=out/'program.c';repeat=out/'repeat.c'
        run('generate',[driver,fixture,'cgen-default',generated]);run('repeat',[driver,fixture,'cgen-default',repeat]);assert sha(generated)==sha(repeat)
        for path in (generated,repeat):product(path)
        implementation=out/'native.c';text=run('native-plan',[planner,'--emit-native',fixture.as_posix(),implementation])
        plan=[json.loads(line) for line in text.splitlines() if line.startswith('{"Fixture":')];assert len(plan)==1
        suppliers=plan[0]['Sources'];report['Sources']=suppliers
        assert 'src/runtime/xl6_array_native_value_admission.c' in suppliers and 'src/runtime/xl6_slot_map_key.c' in suppliers
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
    assert report['Complete'];print('Stack native owning values and checked cleanup: verified',flush=True)
if __name__=='__main__':main()
