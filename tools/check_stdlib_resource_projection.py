"""Resident resource identity projections and lossless F32 casts.
Generated C/hosts and selected resident providers can be instrumented; not all
linked XRT/compiler providers. This is not shipping or full53 acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys
from xlir_public_build_inputs import verify_public_build

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args(); root=args.root.resolve(); public=args.public_directory.resolve(); out=args.out.resolve()
    assert not out.exists(); build,pinned,compared=verify_public_build(root,public)
    cc=shutil.which('gcc'); nm=shutil.which('nm'); assert cc and nm and not (args.sanitize and os.name=='nt')
    names=('stdlib_resource_projection','stdlib_resource_projection_domains','stdlib_float32_as',
        'stdlib_resource_projection_status','stdlib_float32_as_status','stdlib_resource_projection_only',
        'stdlib_float32_as_only','stdlib_channel_projection_only','stdlib_stack_factory_only','stdlib_resource_projection_sync')
    fixtures=[root/'test/cases'/(name+'.xl') for name in names]
    contracts=[root/'test/support'/name for name in ('sequence_projection_contract.c','float32_as_contract.c')]
    hosts=[root/'test/support'/name for name in ('resource_projection_status_host.c','float32_as_status_host.c')]
    runtime=root/'test/support/resource_projection_runtime.c'; artifact=root/'test/support/container_projection_artifact.c'
    host=root/'test/cases/xlir_module_product_oom_host.c'; memory=root/'test/support/module_product_memory.c'
    helpers=[root/('src/runtime/xl6_'+name+'.c') for name in ('container_projection','value_domain_projection',
        'value_admission','channel_admission','slot_map_admission','slot_map')]
    native=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    pinned.update((Path(__file__).resolve(),Path(cc),Path(nm),*fixtures,*contracts,*hosts,runtime,artifact,host,memory,native,*helpers))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir(parents=True); rows=[]; prefixes=[]; products=[]; env=dict(os.environ); suffix='.exe' if os.name=='nt' else ''
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,Products=products,
        ComparedSources=compared,ResourceProjectionAcceptance=False,Full53Acceptance=False,ShippingLibraryAcceptance=False,
        GeneratedInstrumented=args.sanitize,ProjectionProvidersInstrumented=args.sanitize,OtherProvidersInstrumented=False,
        WorkerAllocationPrefixes=False,Scope=__doc__)
    if os.name=='nt':shutil.copy2(native,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release/include'),'-I'+str(public/'include'),
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    if args.sanitize:
        flags+=['-g','-fno-var-tracking-assignments','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    links=[row['Path'] for row in build['Objects'] if Path(row['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    links += [str(native),*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(native.parent)])]
    def run(name,command,reject=False):
        result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=600)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=result.returncode,ExpectedRejection=reject))
        print(name+': '+str(result.returncode),flush=True)
        assert result.returncode!=0 if reject else result.returncode==0,(name,data[-5000:].decode(errors='replace'))
        return data.decode(errors='replace').replace('\r\n','\n')
    try:
        for contract,selected in zip(contracts,(fixtures[:2],fixtures[2:3])):
            binary=out/(contract.stem+suffix); run(contract.stem+'.build',[cc,*flags,'-O0',contract,*links,'-o',binary])
            for fixture in selected:assert 'rejected before output' in run(fixture.stem+'.contract',[binary,fixture])
        artifactBinary=out/('artifact-plan'+suffix)
        run('artifact-plan.build',[cc,*flags,'-O0',artifact,*links,'-o',artifactBinary])
        objects={}; providers={}; remaining=[path for path in links if Path(path).stem not in {helper.stem for helper in helpers}]
        for opt in (0,2):
            objects[opt]=out/('memory.O'+str(opt)+'.o')
            run('memory.O'+str(opt),[cc,*flags,'-O'+str(opt),'-c',memory,'-o',objects[opt]])
            providers[opt]=[]
            for helper in helpers:
                obj=out/(helper.stem+'.O'+str(opt)+'.o')
                run(helper.stem+'.O'+str(opt),[cc,*flags,'-I'+str(root/'release'),'-O'+str(opt),'-c',helper,'-o',obj])
                providers[opt].append(obj)
            binary=out/('native.O'+str(opt)+suffix)
            run('native.O'+str(opt)+'.build',[cc,*flags,'-I'+str(root/'release'),'-O'+str(opt),
                runtime,objects[opt],*providers[opt],*remaining,'-Wl,--wrap=xrtValueGetFloat','-o',binary])
            assert 'lossless/null-diagnostic outputs balanced' in run('native.O'+str(opt)+'.run',[binary])
        for fixture in fixtures:
            name=fixture.stem; generated=out/(name+'.c'); repeat=out/(name+'.repeat.c')
            run(name+'.cgen',[build['Driver'],fixture,'cgen-default',generated])
            run(name+'.repeat',[build['Driver'],fixture,'cgen-default',repeat]); assert sha(generated)==sha(repeat)
            body=generated.read_text(encoding='utf-8')
            if name.startswith(('stdlib_resource_','stdlib_channel_')):assert 'xl6ValueDomainProjectionWitnessTakeV1(' in body
            if name.startswith('stdlib_float32_'):assert 'xl6ValueReadF32V1(' in body
            for opt in (0,2):
                binary=out/(name+'.O'+str(opt)+suffix); selected=host; extras=[]
                if name==names[3]:selected=hosts[0]; extras=['-Wl,--wrap=xl6ValueDomainProjectionWitnessTakeV1']
                if name==names[4]:selected=hosts[1]; extras=['-Wl,--wrap=xl6ValueReadF32V1']
                if name=='stdlib_stack_factory_only':extras+=['-DXL6_PRODUCT_USER_ENTRY']
                run(name+'.O'+str(opt)+'.build',[cc,*flags,'-O'+str(opt),'-include',generated,*extras,selected,
                    objects[opt],*providers[opt],*remaining,'-o',binary])
                assert 'balanced' in run(name+'.O'+str(opt)+'.run',[binary])
                # Domain fixture runs a worker. Its current-thread fault ledger
                # is not a closure over worker allocations; keep it out here.
                if fixture in (fixtures[0],fixtures[2],fixtures[-1]):
                    log=run(name+'.O'+str(opt)+'.oom',[binary,'--all'])
                    match=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',log); assert match
                    runs,first=map(int,match.groups()); assert runs==first+1
                    prefixes.append(dict(Fixture=name,Lane='O'+str(opt),Runs=runs,FirstNoHit=first))
            for mode in ('run','module'):run(name+'.'+mode,[build['Driver'],fixture,mode,out/(name+'.'+mode+'.result')])
            target=out/(name+'.aot'); run(name+'.aot.build',[build['Driver'],fixture,'exe',target])
            executable=Path(str(target)+'.exe') if os.name=='nt' else target
            run(name+'.aot.run',[executable])
            if name.endswith('_only'):
                prefix=out/(name+'.pruned'); run(name+'.plan',[artifactBinary,fixture,prefix])
                plan=json.loads(Path(str(prefix)+'.plan.json').read_text())
                domain=name in (names[5],names[7]); channel=name==names[7]; f32=name==names[6]
                for source,present in [('value_domain_projection',domain),('container_projection',domain),('channel_admission',channel),('slot_map_admission',name==names[5])]:
                    assert ('src/runtime/xl6_'+source+'.c' in plan['Sources'])==present,(name,source)
                standalone=Path(str(prefix)+'.main.c')
                entry='int main(void) { if(!xlir_module_init())return 1; xlir_module_fini(); return xrtGetError() ? 2 : 0; }\n'
                if name=='stdlib_stack_factory_only':
                    entry='extern xvalue* xrtValueArray(void);\nint main(void) { if(!xlir_module_init())return 1; xvalue* args=xrtValueArray(); int code=args ? xlang_main(args) : 1; xrtValueRelease(args); xlir_module_fini(); return code || xrtGetError() ? 2 : 0; }\n'
                standalone.write_text('#include "'+generated.name+'"\n'+entry,encoding='utf-8')
                binary=Path(str(prefix)+suffix)
                run(name+'.pruned.build',[cc,*[flag for flag in flags if flag not in ('-fsanitize=address,undefined','-fno-pie','-no-pie')],
                    '-I'+str(root/'release'),'-I'+str(root/'lib/xruntime'),'-I'+str(root/'lib/xrt'),'-O0',
                    standalone,Path(str(prefix)+'.xrt.c'),*[root/source for source in plan['Sources']],
                    *(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']),'-o',binary])
                run(name+'.pruned.run',[binary]); symbols=run(name+'.pruned.symbols',[nm,'--defined-only',binary])
                assert re.search(r'\bxlir_module_init$',symbols,re.M),'Require a real symbol table'
                assert bool(re.search(r'\bxl6ValueDomainProjectionWitnessTakeV1$',symbols,re.M))==domain
                # GCC's unstripped O0 product selects whole components. Other
                # users of VALUE_ADMISSION legitimately retain its F32 helper.
                assert bool(re.search(r'\bxl6ValueReadF32V1$',symbols,re.M))==('src/runtime/xl6_value_admission.c' in plan['Sources'])
                if f32:assert 'src/runtime/xl6_value_admission.c' in plan['Sources']
        for name,host,symbol in zip(names[3:5],hosts,('xl6ValueDomainProjectionWitnessTakeV1','xl6ValueReadF32V1')):
            original=(out/(name+'.c')).read_text(encoding='utf-8')
            status=re.findall(r'bool projection_ok_(\d+) = '+symbol+r'\(',original); assert len(status)==1
            changed,count=re.subn(r'if \(projection_ok_'+status[0]+r'\)','if (true)',original); assert count==1
            mutant=out/(name+'.lost-status.c'); mutant.write_text(changed,encoding='utf-8'); binary=out/(name+'.lost-status'+suffix)
            run(name+'.lost-status.build',[cc,*flags,'-O0','-include',mutant,'-Wl,--wrap='+symbol,host,
                objects[0],*providers[0],*remaining,'-o',binary])
            run(name+'.lost-status.reject',[binary],True)
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs); assert report['InputsUnchanged']
        products.extend(dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.iterdir())
            if path.is_file() and (path.suffix in ('.h','.c','.o','.exe') or os.access(path,os.X_OK)))
        report['ResourceProjectionAcceptance']=True; report['Complete']=True
    except Exception as error:report['Error']=repr(error); print(error,file=sys.stderr)
    finally:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
