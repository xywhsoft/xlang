"""Native required/nullable callable Stack: checked projection before pop,
closure/code leases, cycles, IR refusal, C O0/O2, complete synchronous caller
allocation prefixes, JIT/module/AOT, optional fully selected ASan/UBSan.
Not executor-thread faults, full53 or shipping artifact acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
from check_stdlib_slot_map import stage_private_sdk
from xlir_public_build_inputs import verify_public_build
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    cc,nm=shutil.which('gcc'),shutil.which('nm');assert cc and nm and not out.exists() and (not a.sanitize or os.name!='nt')
    build,pinned,_=verify_public_build(root,public);driver=Path(build['Driver'])
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    case=root/'test/cases/stdlib_stack_callable.xl';unit=root/'test/cases/stdlib_stack_callable_unit.c';host=root/'test/cases/stdlib_stack_callable_host.c'
    contract=root/'test/cases/xlir_stack_callable_contract.c';planner_source=root/'test/cases/xlir_artifact_runtime_contract.c'
    memory=root/'test/support/module_product_memory.c';manifest,sdk,_=stage_private_sdk(root)
    regressions=('stdlib_stack_future_reference','stdlib_stack_native_value','stdlib_stack_nullable_native_value','stdlib_concurrent_queue_callable_future','stdlib_stack_callable_read_only')
    pinned.update((Path(__file__).resolve(),Path(cc),Path(nm),runtime,case,unit,host,contract,planner_source,memory,manifest))
    pinned.update(root/('test/cases/'+name+'.xl') for name in regressions)
    pinned.update(Path(row[key]) for row in sdk for key in ('Source','Path'))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(pinned)]
    out.mkdir();rows=[];products=[];prefixes=[];controls=[];env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Prefixes=prefixes,NegativeControls=controls,
        LifetimesPerOptimization=100,AllSelectedProvidersSanitized=a.sanitize,FaultScope='complete synchronous caller-thread allocation prefixes',
        Full53Acceptance=False,ShippingArtifact=False,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include','-Ilib/tcc','-Ilib/xruntime','-Ilib/xrt','-I'+str(public/'include'),
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=systems+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)]);ext='.exe' if os.name=='nt' else ''
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    def product(f):products.append(dict(Path=str(f),Sha256=sha(f)))
    def run(name,cmd,bad=False):
        r=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=300);data=r.stdout+r.stderr
        log=out/(name+'.log');log.write_bytes(data);rows.append(dict(Name=name,Exit=r.returncode,ExpectedRejection=bad,Log=str(log),LogSha256=sha(log)))
        assert (r.returncode!=0 if bad else r.returncode==0),(name,data[-6000:])
        if bad:assert b'assert' in data.lower(),(name,r.returncode)
        return data.decode(errors='replace')
    try:
        generated=out/'program.c';repeat=out/'repeat.c'
        run('generate',[driver,case,'cgen-default',generated]);run('repeat',[driver,case,'cgen-default',repeat]);assert sha(generated)==sha(repeat)
        product(generated);product(repeat);body=generated.read_text();assert 'xl6StackCallableTakeV1' in body and 'xl6SequenceCallableCreateV1(2,' in body
        proof=out/('contract'+ext);run('contract-build',[cc,*flags,'-O0',contract,*objects,runtime,*links,'-o',proof]);product(proof)
        report['IrContracts']=run('contract-execute',[proof,case])
        planner=out/('planner'+ext);run('planner-build',[cc,*flags,'-O0',planner_source,*objects,runtime,*links,'-o',planner]);product(planner)
        native=out/'native.c';message=run('native-plan',[planner,'--emit-native',case.as_posix(),native])
        plans=[json.loads(line) for line in message.splitlines() if line.startswith('{"Fixture":')];assert len(plans)==1
        sources=plans[0]['Sources'];report['Sources']=sources
        assert 'src/runtime/xl6_stack_callable.c' in sources and 'src/runtime/xl6_array_callable_admission.c' in sources
        assert 'src/runtime/xl6_array_future_admission.c' not in sources
        minimal=root/'test/cases/stdlib_stack_callable_read_only.xl';minimal_native=out/'minimal-native.c'
        message=run('minimal-plan',[planner,'--emit-native',minimal.as_posix(),minimal_native])
        minimal_plan=[json.loads(line) for line in message.splitlines() if line.startswith('{"Fixture":')];assert len(minimal_plan)==1
        assert 'src/runtime/xl6_stack_callable.c' in minimal_plan[0]['Sources'] and 'src/runtime/xl6_stack.c' not in minimal_plan[0]['Sources']
        report['MinimalSources']=minimal_plan[0]['Sources'];product(minimal_native)
        raw=native.read_text();assert 'XRT_MODULE_ALL' not in raw and 'XRUNTIME_MODULE_ALL' not in raw
        native.write_text('#define XRT_MODULE_MEMORY_DEBUG\n'+raw);product(native);suppliers={};unit_sources=['src/runtime/xl6_ownership_plan.c'];report['UnitSources']=unit_sources
        for opt in ('O0','O2'):
            providers=[]
            for i,source in enumerate([native,*[root/name for name in sources],memory]):
                obj=out/(opt+'-'+str(i)+'.o');run(obj.stem,[cc,*flags,'-'+opt,'-c',source,'-o',obj]);product(obj);providers.append(obj)
            binary=out/(opt+ext);run(opt+'-link',[cc,*flags,'-'+opt,'-include',generated,host,*providers,*systems,'-o',binary]);product(binary)
            run(opt+'-lifetimes',[binary]);assert 'xlcCreateContext' not in run(opt+'-symbols',[nm,'--defined-only',binary])
            for point in range(4096):
                message=run(opt+'-prefix-'+str(point),[binary,point])
                if 'hit=0' in message:break
            else:raise AssertionError('unfinished allocation prefix')
            assert point>0;prefixes.append(dict(Optimization=opt,Runs=point+1,Complete=True))
            for i,source in enumerate(unit_sources):
                obj=out/(opt+'-unit-provider-'+str(i)+'.o');run(obj.stem,[cc,*flags,'-'+opt,'-c',root/source,'-o',obj]);product(obj);providers.append(obj)
            suppliers[opt]=providers
            binary=out/(opt+'-unit'+ext);run(opt+'-unit-link',[cc,*flags,'-'+opt,unit,*providers,*systems,'-o',binary]);product(binary)
            assert '200 lifetimes, 2 graphs' in run(opt+'-unit-vectors',[binary])
        original=(root/'src/runtime/xl6_stack_callable.c').read_text(encoding='utf-8-sig')
        for tag in ('pop-before-projection','wrong-signature','ignored-projection-error'):
            body=original
            if tag=='pop-before-projection':
                commit='if(pop && !xrtTypedStackPop(storage,&moved)){if(!xrtGetError())xrtSetErrorKind(XERR_STATE);goto done;}'
                needle='acquired=xl6ClosureRuntimeV2Get()->CreateValue(owner,entry,source);'
                assert body.count(commit)==1 and body.count(needle)==1;body=body.replace(commit,'').replace(needle,commit+'\n        '+needle)
            elif tag=='wrong-signature':
                needle='xrtValueTypeId(source)!=signature || ';assert body.count(needle)==1;body=body.replace(needle,'')
            else:
                needle='if(xrtGetError())goto done;';assert body.count(needle)==1;body=body.replace(needle,'')
            bad=out/(tag+'.c');bad.write_text(body);product(bad);obj=out/(tag+'.o')
            run(tag+'-build',[cc,*flags,'-O0','-c',bad,'-o',obj]);product(obj);providers=suppliers['O0'][:]
            providers[sources.index('src/runtime/xl6_stack_callable.c')+1]=obj
            binary=out/(tag+ext);run(tag+'-link',[cc,*flags,'-O0',unit,*providers,*systems,'-o',binary]);product(binary)
            run(tag+'-refused',[binary],True);controls.append(tag)
        for name,source in [('callable',case),*[(name,root/('test/cases/'+name+'.xl')) for name in regressions]]:
            for mode in ('run','module'):run(name+'-'+mode,[driver,source,mode,out/(name+'-'+mode)])
            binary=out/(name+'.exe');run(name+'-aot-build',[driver,source,'exe',out/name if os.name=='nt' else binary]);run(name+'-aot-run',[binary]);product(binary)
        if a.sanitize:
            sf=flags+['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie'];sl=['-fsanitize=address,undefined','-no-pie'];providers=[]
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            for i,source in enumerate([native,*[root/name for name in sources],memory]):
                obj=out/('san-'+str(i)+'.o');run(obj.stem,[cc,*sf,'-O1','-c',source,'-o',obj]);product(obj);providers.append(obj)
            obj=out/'san-host.o';run('san-host-build',[cc,*sf,'-O0','-include',generated,host,'-c','-o',obj]);product(obj)
            binary=out/'san';run('san-link',[cc,*sf,obj,*providers,*systems,*sl,'-o',binary]);product(binary);run('san-lifetimes',[binary])
            for i,source in enumerate(unit_sources):
                obj=out/('san-unit-provider-'+str(i)+'.o');run(obj.stem,[cc,*sf,'-O1','-c',root/source,'-o',obj]);product(obj);providers.append(obj)
            binary=out/'san-unit';run('san-unit-link',[cc,*sf,'-O1',unit,*providers,*systems,*sl,'-o',binary]);product(binary);run('san-unit-vectors',[binary])
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs);report['Complete']=report['InputsUnchanged']
    finally:
        report['PhaseCount']=len(rows);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'];print('Callable Stack verified',flush=True)
if __name__=='__main__':main()
