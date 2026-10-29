"""Owning native and nullable native closure cells: shared bindings, checked
copy and owning exchange, selected strict C O0/O2, caller-thread fault prefixes,
IR corruption, source JIT/module/AOT, fully selected optional ASan/UBSan.
Not executor-thread fault injection, full53 or shipping.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
from check_stdlib_slot_map import stage_private_sdk
from xlir_public_build_inputs import verify_public_build
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true');a=parser.parse_args()
    root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    cc,nm=shutil.which('gcc'),shutil.which('nm');assert cc and nm and not out.exists()
    assert not a.sanitize or os.name!='nt'
    build,pinned,_=verify_public_build(root,public);driver=Path(build['Driver'])
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    case=root/'test/cases/stdlib_slot_map_native_cell.xl';host=root/'test/cases/stdlib_native_cell_host.c'
    unit=root/'test/cases/stdlib_native_cell_unit.c';contract=root/'test/cases/xlir_native_cell_contract.c'
    planner_src=root/'test/cases/xlir_artifact_runtime_contract.c';memory=root/'test/support/module_product_memory.c'
    manifest,sdk,_=stage_private_sdk(root)
    pinned.update((Path(__file__).resolve(),Path(cc),Path(nm),runtime,case,host,unit,contract,planner_src,memory,manifest))
    pinned.update(Path(row[k]) for row in sdk for k in ('Source','Path'))
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(pinned)];out.mkdir()
    env=dict(os.environ);rows=[];products=[];prefixes=[];controls=[]
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,
        Prefixes=prefixes,NegativeControls=controls,Full53Acceptance=False,ShippingArtifact=False,
        LifetimesPerOptimization=100,AllSelectedProvidersSanitized=a.sanitize,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include',
        '-Ilib/tcc','-Ilib/xruntime','-Ilib/xrt','-I'+str(public/'include'),'-Wno-unused-function',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=systems+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)]);ext='.exe' if os.name=='nt' else ''
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    def product(p):products.append(dict(Path=str(p),Sha256=sha(p)))
    def run(name,command,bad=False):
        result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=600 if str(command[0])==cc else 180)
        data=result.stdout+result.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Exit=result.returncode,ExpectedRejection=bad,Log=str(log),LogSha256=sha(log)))
        assert (result.returncode!=0 if bad else result.returncode==0),(name,data[-5000:])
        if bad:assert b'assert' in data.lower(),(name,data[-5000:])
        return data.decode(errors='replace')
    try:
        generated=out/'program.c';repeat=out/'repeat.c'
        run('generate',[driver,case,'cgen-default',generated]);run('repeat',[driver,case,'cgen-default',repeat]);assert sha(generated)==sha(repeat)
        product(generated);product(repeat);raw=generated.read_text()
        assert 'xl6CellCreateNativeV2' in raw and 'xl6CellRuntimeV1Get()->Exchange' in raw
        proof=out/('contract'+ext);run('contract-build',[cc,*flags,'-O0',contract,*objects,runtime,*links,'-o',proof]);product(proof)
        report['IrContracts']=run('contract-run',[proof,case])
        planner=out/('planner'+ext);run('planner-build',[cc,*flags,'-O0',planner_src,*objects,runtime,*links,'-o',planner]);product(planner)
        native=out/'native.c';text=run('plan',[planner,'--emit-native',case.as_posix(),native])
        plan=[json.loads(line) for line in text.splitlines() if line.startswith('{"Fixture":')];assert len(plan)==1
        sources=plan[0]['Sources'];report['Sources']=sources
        assert 'src/runtime/xl6_cell_native.c' in sources and 'src/runtime/xl6_nullable_slot_map_key_capture.c' in sources
        body=native.read_text();assert 'XRT_MODULE_ALL' not in body and 'XRUNTIME_MODULE_ALL' not in body
        native.write_text('#define XRT_MODULE_MEMORY_DEBUG\n'+body);product(native)
        wrap=['-Wl,--wrap=xl6SlotMapKeyCopyValueV1'];providers_by_opt={}
        for opt in ('O0','O2'):
            providers=[]
            for i,source in enumerate([native,*[root/p for p in sources],memory]):
                obj=out/(opt+'-'+str(i)+'.o');run(obj.stem,[cc,*flags,'-'+opt,'-c',source,'-o',obj]);product(obj);providers.append(obj)
            providers_by_opt[opt]=providers
            binary=out/(opt+ext);run(opt+'-link',[cc,*flags,'-'+opt,'-include',generated,host,*providers,*systems,*wrap,'-o',binary]);product(binary)
            run(opt+'-lifetimes',[binary]);assert 'xlcCreateContext' not in run(opt+'-symbols',[nm,'--defined-only',binary])
            for lane in ('copy','oom'):
                for at in range(4096):
                    message=run(opt+'-'+lane+'-'+str(at),[binary,lane,at])
                    if 'hit=0' in message:break
                else:raise AssertionError('unfinished fault prefix')
                assert at>0;prefixes.append(dict(Optimization=opt,Lane=lane,Runs=at+1,Complete=True))
            binary=out/(opt+'-unit'+ext);run(opt+'-unit-link',[cc,*flags,'-'+opt,unit,*providers,*systems,'-o',binary]);product(binary)
            assert 'detach=1002' in run(opt+'-unit-vectors',[binary])
        source=root/'src/runtime/xl6_cell_native.c';body=source.read_text(encoding='utf-8-sig')
        check='{((const XL6_NativeCaptureProfileV1*)raw)->DetachCleanup(value,NULL);}';assert body.count(check)==1
        corrupted=out/'forget-native-drop.c';corrupted.write_text(body.replace(check,'{(void)raw;(void)value;}'),encoding='utf-8');product(corrupted)
        obj=out/'forget-native-drop.o';run('bad-drop-build',[cc,*flags,'-Isrc/runtime','-O0','-c',corrupted,'-o',obj]);product(obj)
        providers=providers_by_opt['O0'][:];providers[sources.index('src/runtime/xl6_cell_native.c')+1]=obj
        binary=out/('bad-drop'+ext);run('bad-drop-link',[cc,*flags,'-O0',unit,*providers,*systems,'-o',binary]);product(binary)
        run('bad-drop-refused',[binary],True);controls.append(dict(Name='omitted-native-destruction',Rejected=True))
        body=(root/'src/runtime/xl6_cell.c').read_text(encoding='utf-8-sig')
        check='if(!xrtOwnershipScopeEnd(&mutation)) abort();\n    if(cell->kind==XL6_CELL_STRING)';assert body.count(check)==1
        corrupted=out/'drop-inside-mutation.c';corrupted.write_text(body.replace(check,'if(cell->kind==XL6_CELL_STRING)'),encoding='utf-8');product(corrupted)
        obj=out/'drop-inside-mutation.o';run('bad-scope-build',[cc,*flags,'-Isrc/runtime','-O0','-c',corrupted,'-o',obj]);product(obj)
        providers=providers_by_opt['O0'][:];providers[sources.index('src/runtime/xl6_cell.c')+1]=obj
        binary=out/('bad-scope'+ext);run('bad-scope-link',[cc,*flags,'-O0',unit,*providers,*systems,'-o',binary]);product(binary)
        run('bad-scope-refused',[binary],True);controls.append(dict(Name='destruction-inside-cell-mutation',Rejected=True))
        for mode in ('run','module'):run(mode,[driver,case,mode,out/mode])
        binary=out/'aot.exe';run('aot-build',[driver,case,'exe',out/'aot' if os.name=='nt' else binary]);run('aot-run',[binary]);product(binary)
        if a.sanitize:
            sf=flags+['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie'];sl=['-fsanitize=address,undefined','-no-pie'];providers=[]
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            for i,source in enumerate([native,*[root/p for p in sources],memory]):
                obj=out/('san-'+str(i)+'.o');run(obj.stem,[cc,*sf,'-O1','-c',source,'-o',obj]);product(obj);providers.append(obj)
            host_object=out/'san-host.o';run('san-host-build',[cc,*sf,'-O0','-include',generated,host,'-c','-o',host_object]);product(host_object)
            binary=out/'san';run('san-link',[cc,*sf,host_object,*providers,*systems,*wrap,*sl,'-o',binary]);product(binary);run('san-lifetimes',[binary])
            binary=out/'san-unit';run('san-unit-link',[cc,*sf,'-O1',unit,*providers,*systems,*sl,'-o',binary]);product(binary);run('san-unit-vectors',[binary])
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs);report['Complete']=report['InputsUnchanged']
    finally:
        report['PhaseCount']=len(rows);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'];print('Owning native cells: verified',flush=True)
if __name__=='__main__':main()
