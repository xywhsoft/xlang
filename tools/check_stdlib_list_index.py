"""Authentic native List open index/contains and precise Array-only pruning.

Requires a fresh full-source compiler. Language List factories use native
storage; generated checked functions also consume authenticated native inputs.
No shipping-library/VFS, full List carrier or full53 completion claim.
"""
from __future__ import annotations
import argparse, hashlib, json, os, re, shutil, subprocess, sys, time
from pathlib import Path

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true',help='Instrument only the selected real native closure on Linux')
    parser.add_argument('--array-fixture',type=Path,help='Explicit hashed test-only Array fixture; no compiler/provider override')
    parser.add_argument('--test-root',type=Path,help='Explicit hashed current test hosts; no compiler/provider override')
    args=parser.parse_args(); root=args.root.resolve(); public_dir=args.public_directory.resolve(); out=args.out.resolve()
    assert not out.exists() and (not args.sanitize or os.name!='nt')
    manifest=public_dir/'build.json'; public=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert public['SourceXrt'] and not public['RuntimeOnly'] and public['EnableExeCompiler']==1
    if os.name!='nt': assert public['Complete'] and public['InputsUnchanged']
    cc=shutil.which('gcc'); assert cc
    driver=Path(public['Driver']); assert sha(driver).lower()==public['DriverSha256'].lower()
    product=root/'release/xl.dll' if os.name=='nt' else Path(public['NativeRoot'])/'release/libxl.so'
    sources=['test/support/list_native.c','test/support/module_product_memory.c',
        'src/runtime/xl6_set.c','src/runtime/xl6_set_key_policy.c','src/runtime/xl6_dict.c',
        'src/runtime/xl6_native_leaf_value.c',
        'src/runtime/xl6_list.c','src/runtime/xl6_list_methods.c','src/runtime/xl6_sequence.c',
        'src/runtime/xl6_array_reference_admission.c','src/runtime/xl6_array_reference_value.c',
        'src/runtime/xl6_string_value.c','src/runtime/xl6_nullable_i64_value.c','src/api/xl_foreign_error.c',
        'src/runtime/xl6_array_nominal_admission.c','src/runtime/xl6_array_primitive_admission.c',
        'src/runtime/xl6_array_c_scalar_admission.c','src/runtime/xl6_value_admission.c',
        'src/runtime/xl6_nominal_admission.c','src/runtime/xl6_string_type.c',
        'src/runtime/xl6_nullable_string.c','src/runtime/xl6_nullable_i64.c',
        'src/runtime/xl6_ownership_plan.c','src/runtime/xl6_array_future_admission.c',
        'src/runtime/xl6_char_type.c','src/runtime/xl6_char_value.c',
        'src/runtime/xl6_array.c','src/runtime/xl6_array_value_read.c','src/runtime/xl6_array_erased.c',
        'src/runtime/xl6_array_method_find.c','src/runtime/xl6_value_contains.c','src/runtime/xl6_value_wire_contains.c',
        'src/runtime/xl6_list_value_read.c','src/runtime/xl6_value_index_read.c','src/runtime/xl6_value_index_write.c',
        'src/runtime/xl6_value_named_read.c','src/runtime/xl6_value_named_write.c',
        'src/runtime/xl6_value_object_fields.c','src/runtime/xl6_dynamic_fields.c',
        'src/runtime/xl6_native_handle.c','src/runtime/xl6_code_finalizer.c','src/runtime/xl6_code_lease.c']
    cases=(args.test_root.resolve() if args.test_root else root)/'test/cases'; host=cases/'stdlib_list_index_native_host.c'
    language_host=cases/'stdlib_list_index_language_host.c'; contract=cases/'xlir_list_index_contract.c'
    fixtures=[cases/'stdlib_list_index.xl',cases/'stdlib_array_index_prune.xl',cases/'stdlib_set_dict_contains_prune.xl']
    if args.array_fixture: fixtures[1]=args.array_fixture.resolve()
    module_host=cases/'xlir_module_product_oom_host.c'; memory=root/'test/support/module_product_memory.c'
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),manifest,driver,product,host,language_host,
        contract,module_host,cases/'stdlib_list_index_header.c',cases/'stdlib_list_methods_native_host.c',
        cases/'xlir_artifact_runtime_contract.c',*fixtures,*[root/p for p in sources]}
    for folder in ('src','lib','release/include','test/support'): files.update((root/folder).rglob('*.h'))
    for row in public['Inputs']+public['Headers']+public['Objects']:
        assert sha(row['Path']).lower()==row['Sha256'].lower(),row['Path']; files.add(Path(row['Path']))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(parents=True); rows=[]; sweeps=[]; controls=[]
    report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,
        Platform=sys.platform,ClosedPrefixes=sweeps,ImplementationNegativeControls=controls,
        SelectedNativeClosureSanitized=args.sanitize,WholeCompilerSanitized=False,
        LanguageListMigrationComplete=False,ShippingProductAcceptance=False,Full53Complete=False)
    report['ClosedQueryPrunedExecutions']=[]
    report['NativeObjects']=[]
    report['Objects']=report['NativeObjects']
    environment=os.environ.copy(); suffix='.exe' if os.name=='nt' else ''
    if os.name=='nt': shutil.copy2(product,out/'xl.dll')
    else:
        environment['XL6_SOURCE_NATIVE_ROOT']=public['NativeRoot']
        environment['XL6_TCC_LIB']=str(Path(public['NativeRoot'])/'build/linux/tcc-dist')
    if args.sanitize: environment.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
        UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include')]+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
    links=['-Wl,--wrap=xl6ErrorScopeBegin']+([] if os.name=='nt' else ['-pthread','-lm'])
    sanitized=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if args.sanitize else []
    full_flags=flags+['-DXL_STATIC_EMBED','-I'+str(root/'lib/tcc'),'-I'+str(public_dir/'include'),
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    objects=[r['Path'] for r in public['Objects'] if Path(r['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    full_links=[str(product)]+(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else
        ['-Wl,-rpath,'+str(product.parent),'-pthread','-ldl','-lm'])
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
        log=run(name,[binary,mode]); closed=re.findall(r'closed-batch operation=(\d+) runs=(\d+) first-no-hit=(\d+)',log)
        assert len(closed)==3
        prefix='List index language' if language else 'List index'
        for operation,runs,first in closed:
            operation,runs,first=map(int,(operation,runs,first)); assert operation in (0,1,2) and runs==first+1
            points=re.findall(prefix+r' operation='+str(operation)+r' point=(\d+) hit=(\d) balanced',log)
            assert points==[(str(i),str(int(i<first))) for i in range(runs)]
            sweeps.append(dict(Lane=name,Operation=operation,Mode=mode,Runs=runs,FirstNoHit=first))
    try:
        header=out/('header'+suffix)
        run('header.build',[cc,*flags,'-pedantic-errors',cases/'stdlib_list_index_header.c','-o',header])
        run('header.run',[header])
        native_objects={}
        for level in (0,2):
            lane='O'+str(level); binary=out/('native-'+lane+suffix)
            compiled={}
            for source in [host,*[root/p for p in sources]]:
                obj=out/(lane+'.'+source.stem+'.o')
                run(lane+'.native.provider.'+source.stem,[cc,*flags,*sanitized,'-O'+str(level),'-c',source,'-o',obj])
                compiled[source]=obj
                report['NativeObjects'].append(dict(Path=str(obj),Source=str(source),Sha256=sha(obj)))
            native_objects[level]=compiled
            run(lane+'.native.build',[cc,*sanitized,*compiled.values(),*links,'-o',binary])
            assert 'balanced' in run(lane+'.vectors',[binary,'vectors'])
            for mode in ('oom','scopes'): closed(lane+'.'+mode,binary,mode)
        for name,source,before,after in (
            ('lost-native-read','xl6_value_index_read.c',
             'if (list || xrtValueType(source)==XVALUE_INT_MAP) return xl6ListValueReadCopyV1(source,index);',
             'if (xrtValueType(source)==XVALUE_INT_MAP) return xl6ListValueReadCopyV1(source,index);'),
            ('lost-native-write','xl6_value_index_write.c',
             'if (list>0) success=xl6ListErasedSetV1(source,index,item);',
             'if (list>0) success=xl6ArrayValueSetTakeV1(source,index,xrtValueRetain(item));'),
            ('lost-native-contains','xl6_value_contains.c',
             'if (!xl6ListErasedContainsV1(values,item,&found)) xl6ErrorScopeCaptureFailure(&operation);',
             'found=xl6ArrayMethodIndexOfV1(values,item)>=0;')):
            path='src/runtime/'+source; body=(root/path).read_text(encoding='utf-8'); assert body.count(before)==1
            changed=out/('mutant-'+name+'.c'); changed.write_text(body.replace(before,after,1),encoding='utf-8')
            binary=out/('mutant-'+name+suffix)
            changed_object=out/('mutant-'+name+'.o')
            run(name+'.provider',[cc,*flags,*sanitized,'-O0','-c',changed,'-o',changed_object])
            run(name+'.build',[cc,*sanitized,*[changed_object if p==root/path else obj for p,obj in native_objects[0].items()],*links,'-o',binary])
            run(name+'.reject',[binary,'vectors'],reject=True)
            controls.append(dict(Name=name,Rejected=True,Source=str(changed),SourceSha256=sha(changed)))
        binary=out/('contract'+suffix); bindings=out/'bindings.h'
        run('contract.build',[cc,*full_flags,'-O0',contract,*objects,*full_links,'-o',binary])
        proof=run('contract.plans',[binary,fixtures[0].as_posix(),bindings,fixtures[1].as_posix(),fixtures[2].as_posix()])
        assert 'plans verified' in proof
        plans=[json.loads(line) for line in proof.splitlines() if line.startswith('{')]; assert len(plans)==3
        for index,fixture in enumerate(fixtures):
            tag=('list','array','wire')[index]; generated=out/(tag+'.c'); repeat=out/(tag+'.repeat.c')
            run(tag+'.cgen',[driver,fixture,'cgen-default',generated]); run(tag+'.repeat',[driver,fixture,'cgen-default',repeat])
            assert sha(generated)==sha(repeat); body=generated.read_text(encoding='utf-8-sig')
            if index==1:
                assert 'xl6ArrayMethodContainsV1(' in body
                assert 'xl6ValueContainsV1(' not in body and 'xl6ValueIndexReadCopyV1(' not in body and 'xl6ValueIndexSetTakeV1(' not in body
            elif not index:
                assert all(n+'(' in body for n in ('xl6ValueContainsV1','xl6ValueIndexReadCopyV1','xl6ValueIndexSetTakeV1'))
                assert 'xl6ListCreateTakeV1(' in body and 'xrtValueIntMap()' not in body
            else:
                assert 'xl6DictValueHasV1(' in body and 'xl6SetValueHasV1(' in body
                assert 'xl6ValueWireContainsV1(' not in body and 'xl6ValueContainsV1(' not in body
            for level in (0,2):
                lane=tag+'.O'+str(level); memory_obj=out/(lane+'.memory.o'); native=out/(lane+suffix)
                run(lane+'.memory',[cc,*flags,'-O'+str(level),'-c',memory,'-o',memory_obj])
                run(lane+'.module.build',[cc,*full_flags,'-O'+str(level),'-include',generated,module_host,
                    memory_obj,*objects,*full_links,'-o',native])
                assert 'balanced' in run(lane+'.module.run',[native])
                if index:
                    plan=plans[index]; assert Path(plan['Fixture']).resolve()==fixture.resolve()
                    # Compile exactly the declared native module set and
                    # resident provider closure. MemoryDebug is a test-only
                    # ledger extension, not a production component root.
                    impl=out/(lane+'.pruned-native.c'); pruned=out/(lane+'.pruned'+suffix)
                    impl.write_text(''.join('#define '+m+'\n' for m in plan['NativeModules'])+
                        '#define XRT_MODULE_MEMORY_DEBUG\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
                    system_links=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
                    selected=[]
                    for j,source in enumerate([impl,*[root/p for p in plan['Sources']]]):
                        obj=out/(lane+'.pruned.'+str(j)+'.o')
                        run(lane+'.pruned.provider.'+str(j),[cc,*full_flags,'-O'+str(level),'-c',source,'-o',obj])
                        selected.append(obj)
                    run(lane+'.pruned.build',[cc,*full_flags,'-O'+str(level),'-include',generated,module_host,memory_obj,
                        *selected,*system_links,'-o',pruned])
                    assert 'balanced' in run(lane+'.pruned.run',[pruned])
                    report['ClosedQueryPrunedExecutions'].append(dict(Fixture=str(fixture),Optimization=level,
                        Sources=plan['Sources'],NativeModules=plan['NativeModules'],TestOnlyMemoryDebug=True))
                if not index:
                    language=out/(lane+'.language'+suffix)
                    wraps=['-Wl,--wrap='+n for n in ('xl6ErrorScopeBegin','xl6ListErasedReadV1','xl6ListErasedSetV1','xl6ListErasedContainsV1')]
                    run(lane+'.language.build',[cc,*full_flags,'-O'+str(level),'-include',generated,'-include',bindings,
                        language_host,memory_obj,*objects,*wraps,*full_links,'-o',language])
                    for mode in ('oom','providers'): closed(lane+'.language.'+mode,language,mode,True)
            run(tag+'.jit',[driver,fixture,'run',out/(tag+'.jit.result')])
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('List index gate: '+str(len(rows))+' phases, complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__': raise SystemExit(main())
