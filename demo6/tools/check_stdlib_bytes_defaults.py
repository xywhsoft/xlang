"""Explicit-length binary literal defaults in dynamic methods and callables.
Generated/host instrumentation only; linked providers are not instrumented.
Not all dynamic/Native codecs, worker OOM or full53/shipping acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys
from xlir_public_build_inputs import verify_public_build
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true');args=parser.parse_args()
    root,public,out=args.root.resolve(),args.public_directory.resolve(),args.out.resolve()
    assert not out.exists();build,pinned,compared=verify_public_build(root,public)
    cc,nm=shutil.which('gcc'),shutil.which('nm');assert cc and nm and not (args.sanitize and os.name=='nt')
    names=('stdlib_bytes_defaults','stdlib_bytes_defaults_process','cgen_dynamic_signed_defaults',
        'stdlib_float32_as','stdlib_callable_nullable','xlir_stdlib_dynamic_native.pending','stdlib_bytes_defaults_reject')
    fixtures=[root/'test/cases'/(name+'.xl') for name in names]
    assert all(path.exists() for path in fixtures)
    contract=root/'test/support/bytes_defaults_contract.c';artifact=root/'test/support/container_projection_artifact.c'
    host=root/'test/cases/xlir_module_product_oom_host.c';memory=root/'test/support/module_product_memory.c'
    child=root/'test/cases/xlir_process_child.c'
    native=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    pinned.update((Path(__file__).resolve(),Path(cc),Path(nm),*fixtures,contract,artifact,host,memory,child,native))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir(parents=True);rows=[];prefixes=[];products=[];env=dict(os.environ);suffix='.exe' if os.name=='nt' else ''
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,Products=products,
        ComparedSources=compared,BytesDefaultsAcceptance=False,Full53Acceptance=False,ShippingLibraryAcceptance=False,
        GeneratedInstrumented=args.sanitize,OtherProvidersInstrumented=False,Scope=__doc__)
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
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links += [str(native),*systems,*([] if os.name=='nt' else ['-Wl,-rpath,'+str(native.parent)])]
    def run(name,command,reject=False):
        result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=600)
        data=result.stdout+result.stderr;(out/(name+'.log')).write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=result.returncode,ExpectedRejection=reject))
        print(name+': '+str(result.returncode),flush=True)
        assert result.returncode!=0 if reject else result.returncode==0,(name,data[-5000:].decode(errors='replace'))
        return data.decode(errors='replace').replace('\r\n','\n')
    try:
        childBinary=out/('child'+suffix);run('child.build',[cc,'-std=c11','-D_GNU_SOURCE',*(['-municode'] if os.name=='nt' else []),child,'-o',childBinary])
        env['XL6_PROCESS_CHILD']=str(childBinary)
        binary=out/('contract'+suffix);run('contract.build',[cc,*flags,'-O0',contract,*links,'-o',binary])
        for fixture in fixtures[:2]:assert 'rejected before output' in run(fixture.stem+'.contract',[binary,fixture])
        artifactBinary=out/('artifact-plan'+suffix);run('artifact-plan.build',[cc,*flags,'-O0',artifact,*links,'-o',artifactBinary])
        objects={}
        for opt in (0,2):
            objects[opt]=out/('memory.O'+str(opt)+'.o');run('memory.O'+str(opt),[cc,*flags,'-O'+str(opt),'-c',memory,'-o',objects[opt]])
        for fixture in fixtures:
            name=fixture.stem;generated=out/(name+'.c');repeat=out/(name+'.repeat.c')
            run(name+'.cgen',[build['Driver'],fixture,'cgen-default',generated])
            run(name+'.repeat',[build['Driver'],fixture,'cgen-default',repeat]);assert sha(generated)==sha(repeat)
            for opt in (0,2):
                binary=out/(name+'.O'+str(opt)+suffix)
                run(name+'.O'+str(opt)+'.build',[cc,*flags,'-O'+str(opt),'-include',generated,host,objects[opt],*links,'-o',binary])
                assert 'balanced' in run(name+'.O'+str(opt)+'.run',[binary])
                if fixture==fixtures[0]:
                    log=run(name+'.O'+str(opt)+'.oom',[binary,'--all']);match=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',log);assert match
                    runs,first=map(int,match.groups());assert runs==first+1;prefixes.append(dict(Lane='O'+str(opt),Runs=runs,FirstNoHit=first))
            for mode in ('run','module'):run(name+'.'+mode,[build['Driver'],fixture,mode,out/(name+'.'+mode+'.result')])
            target=out/(name+'.aot');run(name+'.aot.build',[build['Driver'],fixture,'exe',target])
            executable=Path(str(target)+'.exe') if os.name=='nt' else target;run(name+'.aot.run',[executable])
        name=fixtures[0].stem;generated=out/(name+'.c');prefix=out/(name+'.pruned')
        run(name+'.plan',[artifactBinary,fixtures[0],prefix]);plan=json.loads(Path(str(prefix)+'.plan.json').read_text())
        assert not any('process' in source or 'regex' in source for source in plan['Sources'])
        standalone=Path(str(prefix)+'.main.c');standalone.write_text('#include "'+generated.name+'"\nint main(void) { if(!xlir_module_init())return 1; xlir_module_fini(); return xrtGetError() ? 2 : 0; }\n')
        binary=Path(str(prefix)+suffix)
        run(name+'.pruned.build',[cc,*[flag for flag in flags if flag not in ('-fsanitize=address,undefined','-fno-pie','-no-pie')],
            '-I'+str(root/'release'),'-I'+str(root/'lib/xruntime'),'-I'+str(root/'lib/xrt'),'-O0',standalone,Path(str(prefix)+'.xrt.c'),
            *[root/source for source in plan['Sources']],*systems,'-o',binary])
        run(name+'.pruned.run',[binary]);symbols=run(name+'.pruned.symbols',[nm,'--defined-only',binary]);assert re.search(r'\bxlir_module_init$',symbols,re.M)
        original=generated.read_text(encoding='utf-8')
        changed,count=re.subn(r'(\{ xvalue\* value = default_bytes\d+; default_bytes\d+ = NULL; )xrtValueRelease\(value\);',r'\1(void)value;',original);assert count
        mutant=out/'lost-default-owner.c';mutant.write_text(changed,encoding='utf-8');binary=out/('lost-default-owner'+suffix)
        run('lost-default-owner.build',[cc,*flags,'-O0','-include',mutant,host,objects[0],*links,'-o',binary]);run('lost-default-owner.reject',[binary],True)
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);assert report['InputsUnchanged']
        products.extend(dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.iterdir()) if path.is_file() and (path.suffix in ('.h','.c','.o','.exe') or os.access(path,os.X_OK)))
        report['BytesDefaultsAcceptance']=True;report['Complete']=True
    except Exception as error:report['Error']=repr(error);print(error,file=sys.stderr)
    finally:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
