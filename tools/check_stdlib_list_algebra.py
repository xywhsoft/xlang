"""List shallow copy/right-biased merge/without: resident native and C/JIT gates.

Requires a fresh complete source compiler. This migrates known List copy and
algebra consumers, NOT source factories, open-var/deep copy or persistence.
Only selected native providers are sanitized on Linux; no shipping/full53 claim.
"""
from __future__ import annotations
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
from pathlib import Path

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--test-root',type=Path,help='Explicit hashed test-only hosts/fixture, never providers')
    p.add_argument('--sanitize',action='store_true')
    args=p.parse_args(); root=args.root.resolve(); public_dir=args.public_directory.resolve(); out=args.out.resolve()
    assert not out.exists() and (not args.sanitize or os.name!='nt')
    manifest=public_dir/'build.json'; public=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert public['SourceXrt'] and not public['RuntimeOnly'] and public['EnableExeCompiler']==1
    if os.name!='nt': assert public['Complete'] and public['InputsUnchanged']
    cc=shutil.which('gcc'); assert cc
    driver=Path(public['Driver']); assert sha(driver).lower()==public['DriverSha256'].lower()
    product=root/'release/xl.dll' if os.name=='nt' else Path(public['NativeRoot'])/'release/libxl.so'
    cases=(args.test_root.resolve() if args.test_root else root)/'test/cases'
    fixture=cases/'stdlib_list_algebra.xl'; continuation=cases/'stdlib_list_algebra_continuation.xl'
    contract=cases/'xlir_list_algebra_contract.c'
    host=cases/'stdlib_list_algebra_native_host.c'; language_host=cases/'stdlib_list_algebra_language_host.c'
    sources=['test/support/list_algebra_native.c','test/support/module_product_memory.c',
        'src/runtime/xl6_list.c','src/runtime/xl6_list_methods.c','src/runtime/xl6_list_algebra.c','src/runtime/xl6_sequence.c',
        'src/runtime/xl6_array_reference_admission.c','src/runtime/xl6_array_reference_value.c',
        'src/runtime/xl6_string_value.c','src/runtime/xl6_nullable_i64_value.c','src/api/xl_foreign_error.c',
        'src/runtime/xl6_array_nominal_admission.c','src/runtime/xl6_array_primitive_admission.c',
        'src/runtime/xl6_array_c_scalar_admission.c','src/runtime/xl6_value_admission.c',
        'src/runtime/xl6_nominal_admission.c','src/runtime/xl6_string_type.c',
        'src/runtime/xl6_nullable_string.c','src/runtime/xl6_nullable_i64.c','src/runtime/xl6_ownership_plan.c',
        'src/runtime/xl6_array_future_admission.c','src/runtime/xl6_char_type.c','src/runtime/xl6_char_value.c']
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),manifest,driver,product,fixture,continuation,contract,host,language_host,
        cases/'stdlib_list_methods_native_host.c',cases/'xlir_artifact_runtime_contract.c',
        cases/'cgen_basic.xl',cases/'stdlib_array_copy_language.xl',
        cases/'xlir_module_product_oom_host.c',root/'test/support/list_native.c',*[root/name for name in sources]}
    for folder in ('src','lib','release/include','test/support'): files.update((root/folder).rglob('*.h'))
    for row in public['Inputs']+public['Headers']+public['Objects']:
        assert sha(row['Path']).lower()==row['Sha256'].lower(),row['Path']; files.add(Path(row['Path']))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(parents=True); rows=[]; sweeps=[]; native_objects=[]
    report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,Platform=sys.platform,
        ClosedPrefixes=sweeps,Objects=native_objects,ImplementationNegativeControls=[],PrunedExecutions=[],
        SelectedNativeClosureSanitized=args.sanitize,WholeCompilerSanitized=False,
        LanguageListMigrationComplete=False,ShippingProductAcceptance=False,Full53Complete=False)
    environment=os.environ.copy(); suffix='.exe' if os.name=='nt' else ''
    if os.name=='nt': shutil.copy2(product,out/'xl.dll')
    else: environment['XL6_SOURCE_NATIVE_ROOT']=public['NativeRoot']
    if args.sanitize: environment.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
        UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-Wno-unused-function']+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
    wraps=['-Wl,--wrap='+name for name in ('xl6ErrorScopeBegin','xl6ListErasedCloneV1','xl6ListMergeV1',
        'xl6ListErasedSetV1','xl6ListErasedKeyV1')]
    sanitized=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if args.sanitize else []
    full_flags=flags+['-DXL_STATIC_EMBED','-I'+str(root/'lib/tcc'),'-I'+str(public_dir/'include'),
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    objects=[r['Path'] for r in public['Objects'] if Path(r['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    system_links=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    full_links=[str(product),*system_links]+([] if os.name=='nt' else ['-Wl,-rpath,'+str(product.parent)])
    def run(name,command,reject=False):
        command=list(map(str,command)); start=time.monotonic()
        result=subprocess.run(command,cwd=root,env=environment,capture_output=True,timeout=240)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        accepted=result.returncode!=0 if reject else result.returncode==0
        rows.append(dict(Name=name,Command=command,Exit=result.returncode,Seconds=time.monotonic()-start,
            Complete=accepted,ExpectedRejection=reject)); print(name+': '+str(result.returncode),flush=True)
        assert accepted,(name,data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    def closed(name,binary,mode,language=False):
        log=run(name,[binary,mode]); prefix='List algebra language' if language else 'List algebra'
        matches=re.findall(prefix+r' closed-batch operation=(\d+) runs=(\d+) first-no-hit=(\d+)',log)
        assert len(matches)==(4 if language else 11)
        for operation,runs,first in matches:
            operation,runs,first=map(int,(operation,runs,first)); assert runs==first+1
            points=re.findall(prefix+r' operation='+str(operation)+r' point=(\d+) hit=(\d) balanced',log)
            assert points==[(str(i),str(int(i<first))) for i in range(runs)]
            sweeps.append(dict(Lane=name,Operation=operation,Mode=mode,Runs=runs,FirstNoHit=first))
    try:
        native={}
        for level in (0,2):
            lane='O'+str(level); compiled={}
            for source in [host,*[root/name for name in sources]]:
                obj=out/(lane+'.'+source.stem+'.o')
                run(lane+'.provider.'+source.stem,[cc,*flags,*sanitized,'-O'+str(level),'-c',source,'-o',obj])
                compiled[source]=obj; native_objects.append(dict(Path=str(obj),Source=str(source),Sha256=sha(obj)))
            native[level]=compiled; binary=out/(lane+'.native'+suffix)
            run(lane+'.build',[cc,*sanitized,*compiled.values(),*wraps,*system_links,'-o',binary])
            assert 'balanced' in run(lane+'.vectors',[binary,'vectors'])
            for mode in ('oom','scopes','providers'): closed(lane+'.'+mode,binary,mode)
        for name,source,before,after in (
            ('aliased-clone','src/runtime/xl6_list.c',
             'owner ? clonePinned(owner,0,false,false,0,&scope) : NULL','owner ? xrtValueRetain(value) : NULL'),
            ('wrong-merge-bias','src/runtime/xl6_list_algebra.c',
             'xl6ListMergeV1(result,right,&layout,true)','xl6ListMergeV1(result,right,&layout,false)'),
            ('lost-remove','src/runtime/xl6_list_algebra.c',
             'xl6ListErasedKeyV1(result,key,true,&found)','xl6ListErasedKeyV1(result,key,false,&found)')):
            body=(root/source).read_text(encoding='utf-8'); assert body.count(before)==1
            overlay=out/('overlay-'+name); changed=overlay/source; changed.parent.mkdir(parents=True)
            changed.write_text(body.replace(before,after,1),encoding='utf-8'); obj=out/('mutant-'+name+'.o')
            run(name+'.provider',[cc,*flags,*sanitized,'-O0','-c',changed,'-o',obj])
            binary=out/('mutant-'+name+suffix)
            run(name+'.build',[cc,*sanitized,*[obj if s==root/source else o for s,o in native[0].items()],
                *wraps,*system_links,'-o',binary]); run(name+'.reject',[binary,'vectors'],reject=True)
            report['ImplementationNegativeControls'].append(dict(Name=name,Rejected=True,Source=str(changed),Sha256=sha(changed)))
        binding=out/'bindings.h'; binary=out/('contract'+suffix)
        run('contract.build',[cc,*full_flags,'-O0',contract,*objects,*full_links,'-o',binary])
        proof=run('contract.run',[binary,fixture.as_posix(),binding]); assert 'bindings verified' in proof
        plans=[json.loads(line) for line in proof.splitlines() if line.startswith('{')]; assert len(plans)==1
        plan=plans[0]; generated=out/'algebra.c'; repeat=out/'repeat.c'
        run('cgen',[driver,fixture,'cgen-default',generated]); run('cgen.repeat',[driver,fixture,'cgen-default',repeat])
        assert sha(generated)==sha(repeat); body=generated.read_text(encoding='utf-8-sig')
        for symbol in ('xl6ListValueCloneV1','xl6ListValueMergeRightV1','xl6ListValueWithoutKeyV1'): assert symbol+'(' in body
        assert 'xlir_xvalue_list_merge_right' not in body and 'xlir_xvalue_list_without_i64_key' not in body
        for level in (0,2):
            lane='language.O'+str(level); memory_obj=out/(lane+'.memory.o')
            run(lane+'.memory',[cc,*flags,'-O'+str(level),'-c',root/'test/support/module_product_memory.c','-o',memory_obj])
            module=out/(lane+'.module'+suffix)
            run(lane+'.module.build',[cc,*full_flags,'-O'+str(level),'-include',generated,cases/'xlir_module_product_oom_host.c',
                memory_obj,*objects,*full_links,'-o',module]); assert 'balanced' in run(lane+'.module.run',[module])
            binary=out/(lane+suffix)
            run(lane+'.build',[cc,*full_flags,'-O'+str(level),'-include',generated,'-include',binding,language_host,
                memory_obj,*objects,*wraps,*full_links,'-o',binary]); assert 'balanced' in run(lane+'.vectors',[binary,'vectors'])
            for mode in ('oom','providers'): closed(lane+'.'+mode,binary,mode,True)
            impl=out/(lane+'.pruned.c'); pruned=out/(lane+'.pruned'+suffix); selected=[]
            impl.write_text(''.join('#define '+m+'\n' for m in plan['NativeModules'])+
                '#define XRT_MODULE_MEMORY_DEBUG\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
            for i,source in enumerate([impl,*[root/name for name in plan['Sources']]]):
                obj=out/(lane+'.pruned.'+str(i)+'.o')
                run(lane+'.pruned.provider.'+str(i),[cc,*full_flags,'-O'+str(level),'-c',source,'-o',obj]); selected.append(obj)
            run(lane+'.pruned.build',[cc,*full_flags,'-O'+str(level),'-include',generated,cases/'xlir_module_product_oom_host.c',
                memory_obj,*selected,*system_links,'-o',pruned]); assert 'balanced' in run(lane+'.pruned.run',[pruned])
            report['PrunedExecutions'].append(dict(Optimization=level,Sources=plan['Sources'],NativeModules=plan['NativeModules']))
        run('jit',[driver,fixture,'run',out/'jit.result'])
        later=out/'continuation.c'
        run('continuation.cgen',[driver,continuation,'cgen-default',later])
        later_body=later.read_text(encoding='utf-8-sig')
        for symbol in ('xl6ListValueCloneV1','xl6ListValueMergeRightV1','xl6ListValueWithoutKeyV1'): assert symbol+'(' in later_body
        assert 'xlir_xvalue_list_merge_right' not in later_body and 'xlir_xvalue_list_without_i64_key' not in later_body
        for level in (0,2):
            lane='continuation.O'+str(level); binary=out/(lane+suffix)
            run(lane+'.build',[cc,*full_flags,'-O'+str(level),'-include',later,cases/'xlir_module_product_oom_host.c',
                out/('language.O'+str(level)+'.memory.o'),*objects,*full_links,'-o',binary])
            assert 'balanced' in run(lane+'.run',[binary])
        run('continuation.jit',[driver,continuation,'run',out/'continuation.jit.result'])
        # The real artifact visitor must exclude this component from Array-only
        # and scalar programs; an empty near-miss symbol is not the pruning test.
        for name in ('cgen_basic.xl','stdlib_array_copy_language.xl'):
            proof=run('unrelated.'+Path(name).stem,[out/('contract'+suffix),cases/name,binding,'absent'])
            assert 'excludes List algebra: verified' in proof
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('List algebra gate: '+str(len(rows))+' phases, complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__': raise SystemExit(main())
