"""Invocation-time dynamic method defaults, prefix and receiver ownership.
Full source providers; optional sanitizers cover generated C and the host only.
Erased named/anonymous callable defaults, full53 and shipping acceptance are not claimed.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys
from xlir_public_build_inputs import verify_public_build

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    assert not out.exists();build,pinned,compared=verify_public_build(root,public)
    cc,nm=shutil.which('gcc'),shutil.which('nm');assert cc and nm and not (a.sanitize and os.name=='nt')
    compile_only=('stdlib_class_catalog','stdlib_generic_declaration')
    runtime=('stdlib_runtime_method_defaults','stdlib_runtime_method_defaults_errors',
        'stdlib_integer_defaults','stdlib_integer_defaults_boundary','stdlib_integer_defaults_file',
        'stdlib_dynamic_callbacks','stdlib_dynamic_callbacks_reject','stdlib_float32_as','stdlib_bytes_defaults')
    fixtures=[root/'test/cases'/(name+'.xl') for name in (*compile_only,*runtime)]
    contract=root/'test/support/runtime_method_defaults_contract.c'
    integerContract=root/'test/support/integer_defaults_contract.c'
    artifact=root/'test/support/container_projection_artifact.c'
    host=root/'test/cases/xlir_module_product_oom_host.c';memory=root/'test/support/module_product_memory.c'
    native=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    pinned.update((Path(__file__).resolve(),Path(cc),Path(nm),*fixtures,contract,integerContract,artifact,host,memory,native))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir(parents=True);rows=[];prefixes=[];products=[];env=dict(os.environ);suffix='.exe' if os.name=='nt' else ''
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,Products=products,
        ComparedSources=compared,RuntimeMethodDefaultAcceptance=False,Full53Acceptance=False,ShippingLibraryAcceptance=False,
        GeneratedInstrumented=a.sanitize,OtherProvidersInstrumented=False,Scope=__doc__)
    if os.name=='nt':shutil.copy2(native,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release/include'),'-I'+str(public/'include'),
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    san=['-g','-fno-var-tracking-assignments','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if a.sanitize else []
    if a.sanitize:env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    tail=[str(native),*systems,*([] if os.name=='nt' else ['-Wl,-rpath,'+str(native.parent)])]
    links=[*objects,*tail]
    def run(name,command,reject=False):
        r=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=600)
        data=r.stdout+r.stderr;(out/(name+'.log')).write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=r.returncode,ExpectedRejection=reject))
        print(name+': '+str(r.returncode),flush=True)
        assert r.returncode!=0 if reject else r.returncode==0,(name,data[-5000:].decode(errors='replace'))
        return data.decode(errors='replace').replace('\r\n','\n')
    try:
        binary=out/('contract'+suffix);run('contract.build',[cc,*flags,'-O0',contract,*links,'-o',binary])
        for name in runtime[:2]:
            assert 'rejected before output' in run(name+'.contract',[binary,root/'test/cases'/(name+'.xl')])
        binary=out/('integer-contract'+suffix);run('integer-contract.build',[cc,*flags,'-O0',integerContract,*links,'-o',binary])
        for name in runtime[2:5]:
            assert 'rejected before output' in run(name+'.contract',[binary,root/'test/cases'/(name+'.xl')])
        artifactBinary=out/('artifact-plan'+suffix);run('artifact-plan.build',[cc,*flags,'-O0',artifact,*links,'-o',artifactBinary])
        memoryObjects={}
        for opt in (0,2):
            memoryObjects[opt]=out/('memory.O'+str(opt)+'.o')
            run('memory.O'+str(opt),[cc,*flags,*san,'-O'+str(opt),'-c',memory,'-o',memoryObjects[opt]])
        for fixture in fixtures:
            name=fixture.stem;generated=out/(name+'.c');repeat=out/(name+'.repeat.c')
            run(name+'.cgen',[build['Driver'],fixture,'cgen-default',generated])
            run(name+'.repeat',[build['Driver'],fixture,'cgen-default',repeat]);assert sha(generated)==sha(repeat)
            if name in compile_only:
                for opt in (0,2):run(name+'.O'+str(opt)+'.compile',[cc,*flags,*san,'-O'+str(opt),'-c',generated,'-o',out/(name+'.O'+str(opt)+'.o')])
                continue
            for opt in (0,2):
                binary=out/(name+'.O'+str(opt)+suffix)
                run(name+'.O'+str(opt)+'.build',[cc,*flags,*san,'-O'+str(opt),'-include',generated,host,memoryObjects[opt],*links,'-o',binary])
                assert 'balanced' in run(name+'.O'+str(opt)+'.run',[binary])
                if name in runtime[:2]:
                    log=run(name+'.O'+str(opt)+'.oom',[binary,'--all']);match=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',log);assert match
                    runs,first=map(int,match.groups());assert runs==first+1;prefixes.append(dict(Fixture=name,Lane='O'+str(opt),Runs=runs,FirstNoHit=first))
            for mode in ('run','module'):run(name+'.'+mode,[build['Driver'],fixture,mode,out/(name+'.'+mode+'.result')])
            target=out/(name+'.aot');run(name+'.aot.build',[build['Driver'],fixture,'exe',target])
            executable=Path(str(target)+'.exe') if os.name=='nt' else target;run(name+'.aot.run',[executable])
        for name in runtime[:2]:
            generated=out/(name+'.c');prefix=out/(name+'.pruned')
            run(name+'.plan',[artifactBinary,root/'test/cases'/(name+'.xl'),prefix]);plan=json.loads(Path(str(prefix)+'.plan.json').read_text())
            assert not any('regex' in source or 'process' in source for source in plan['Sources'])
            assert not {'XRT_MODULE_FILE','XRT_MODULE_FILE_MAP','XRT_MODULE_FILE_LOCK','XRT_MODULE_FILE_TEMP'}.intersection(plan['NativeModules'])
            standalone=Path(str(prefix)+'.main.c');standalone.write_text('#include "'+generated.name+'"\nint main(void) { if(!xlir_module_init())return 1; xlir_module_fini(); return xrtGetError() ? 2 : 0; }\n')
            binary=Path(str(prefix)+suffix)
            run(name+'.pruned.build',[cc,*flags,'-I'+str(root/'release'),'-I'+str(root/'lib/xruntime'),'-I'+str(root/'lib/xrt'),'-O0',standalone,Path(str(prefix)+'.xrt.c'),
                *[root/source for source in plan['Sources']],*systems,'-o',binary])
            run(name+'.pruned.run',[binary]);symbols=run(name+'.pruned.symbols',[nm,'--defined-only',binary]);assert re.search(r'\bxlir_module_init$',symbols,re.M)
        original=(out/'stdlib_runtime_method_defaults.c').read_text(encoding='utf-8')
        changed,count=re.subn(r'xlir_string_drop\(a(\d+)\); a\1 = \(xlir_string\)\{0\};',r'a\1 = (xlir_string){0};',original)
        assert count, 'Dynamic adapters must retire their owned borrowed-string projections and defaults'
        mutant=out/'lost-default-owner.c';mutant.write_text(changed,encoding='utf-8');binary=out/('lost-default-owner'+suffix)
        run('lost-default-owner.build',[cc,*flags,*san,'-O0','-include',mutant,host,memoryObjects[0],*links,'-o',binary])
        run('lost-default-owner.reject',[binary],True)
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);assert report['InputsUnchanged']
        products.extend(dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.iterdir()) if path.is_file() and (path.suffix in ('.h','.c','.o','.exe') or os.access(path,os.X_OK)))
        report['RuntimeMethodDefaultAcceptance']=True;report['Complete']=True
    except Exception as error:report['Error']=repr(error);print(error,file=sys.stderr)
    finally:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
