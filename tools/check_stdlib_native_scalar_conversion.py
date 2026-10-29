"""Safe explicit C scalar conversion and exact checked/nullable projection.
Full source providers; optional sanitizers cover generated C and the host only.
Full53 and shipping acceptance are not claimed.
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
    runtime=('stdlib_native_scalar_convert','stdlib_native_scalar_as','stdlib_native_scalar_errors',
        'stdlib_runtime_method_scalar','stdlib_runtime_method_scalar_nullable','stdlib_runtime_method_scalar_errors',
        'stdlib_runtime_callable_future','stdlib_runtime_method_future','stdlib_runtime_future_errors',
        'stdlib_runtime_callable_func_defaults','stdlib_runtime_callable_func_parameters','stdlib_runtime_callable_func_errors',
        'stdlib_runtime_callable_defaults','stdlib_runtime_callable_defaults_errors',
        'stdlib_runtime_callable_prefix_capture','stdlib_runtime_method_defaults','stdlib_runtime_method_defaults_errors',
        'stdlib_integer_defaults','stdlib_integer_defaults_boundary','stdlib_integer_defaults_file',
        'stdlib_dynamic_callbacks','stdlib_dynamic_callbacks_reject','stdlib_float32_as','stdlib_bytes_defaults',
        'stdlib_stack_dynamic_surface','stdlib_stack_dynamic_try.pending','stdlib_native_scalar_as_only','stdlib_native_scalar_reflection',
        'stdlib_native_scalar_generic','stdlib_native_scalar_fields',
        'stdlib_scalar_conversion_reflection','stdlib_scalar_conversion_length',
        'stdlib_runtime_callable_variadic_func','stdlib_runtime_nullable_method_func','stdlib_runtime_variadic_func_errors')
    fixtures=[root/'test/cases'/(name+'.xl') for name in (*compile_only,*runtime)]
    contract=root/'test/support/native_scalar_ir_contract.c'
    scalarContract=root/'test/support/runtime_method_scalar_contract.c'
    futureContract=root/'test/support/runtime_future_contract.c'
    variadicContract=root/'test/support/runtime_variadic_func_contract.c'
    funcContract=root/'test/support/runtime_callable_func_contract.c'
    defaultContract=root/'test/support/runtime_callable_defaults_contract.c'
    methodContract=root/'test/support/runtime_method_defaults_contract.c'
    integerContract=root/'test/support/integer_defaults_contract.c'
    artifact=root/'test/support/container_projection_artifact.c'
    host=root/'test/cases/xlir_module_product_oom_host.c';memory=root/'test/support/module_product_memory.c'
    variadicErrorHost=root/'test/support/runtime_variadic_func_oom_host.c'
    funcErrorHost=root/'test/support/runtime_callable_func_oom_host.c'
    defaultErrorHost=root/'test/support/runtime_callable_defaults_oom_host.c'
    native=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    frameContract=root/'test/support/native_scalar_convert_contract.c'
    admissionContract=root/'test/support/native_scalar_admission_contract.c'
    languageContract=root/'test/support/scalar_conversion_contract.c'
    languageIrContract=root/'test/support/scalar_conversion_ir_contract.c'
    arrayOnly=root/'test/cases/stdlib_runtime_callable_variadic_array_only.xl'
    asyncMultiReturn=root/'test/cases/sem_async_multi_return.xl'
    reflectionStatic=root/'test/cases/sem_native_scalar_reflection_static.xl'
    genericNegative=[root/'test/cases'/('sem_native_scalar_generic_'+name+'.xl') for name in ('string','nullable')]
    pinned.update((Path(__file__).resolve(),Path(cc),Path(nm),*fixtures,contract,scalarContract,futureContract,funcContract,defaultContract,methodContract,integerContract,artifact,host,funcErrorHost,defaultErrorHost,memory,native,frameContract,admissionContract,languageContract,languageIrContract,arrayOnly,variadicContract,variadicErrorHost,asyncMultiReturn,reflectionStatic,*genericNegative))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir(parents=True);rows=[];prefixes=[];products=[];env=dict(os.environ);suffix='.exe' if os.name=='nt' else ''
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,Products=products,
        ComparedSources=compared,NativeScalarConversionAcceptance=False,NativeScalarReflectionAcceptance=False,NativeScalarGenericAcceptance=False,LanguageScalarConversionAcceptance=False,Full53Acceptance=False,ShippingLibraryAcceptance=False,
        GeneratedInstrumented=a.sanitize,OtherProvidersInstrumented=False,Scope=__doc__)
    if os.name=='nt':shutil.copy2(native,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(public/'include'),
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
        assert 'multi-return' in run('async-multi-return.reject',
            [build['Driver'],asyncMultiReturn,'cgen-default',out/'async-multi-return.invalid.c'],True)
        run('reflection-static.reject',[build['Driver'],reflectionStatic,'cgen-default',out/'reflection-static.invalid.c'],True)
        invalid=out/'reflection-static.invalid.c';assert not invalid.exists() or invalid.stat().st_size==0
        for source in genericNegative:
            target=out/(source.stem+'.invalid.c')
            assert 'closed numeric source' in run(source.stem+'.reject',[build['Driver'],source,'cgen-default',target],True)
            assert not target.exists() or target.stat().st_size==0
        binary=out/('frame-contract'+suffix)
        run('frame-contract.build',[cc,*flags,*san,'-O0',frameContract,memory,*links,'-o',binary])
        frameLog=run('frame-contract.run',[binary]);assert 'balanced' in frameLog
        binary=out/('admission-contract'+suffix)
        run('admission-contract.build',[cc,*flags,*san,'-O0',admissionContract,memory,*links,'-o',binary])
        assert 'balanced' in run('admission-contract.run',[binary])
        binary=out/('contract'+suffix);run('contract.build',[cc,*flags,'-O0',contract,*links,'-o',binary])
        languageBinary=out/('language-contract'+suffix)
        run('language-contract.build',[cc,*flags,*san,'-O0',languageContract,memory,*links,'-o',languageBinary])
        assert 'balanced' in run('language-contract.run',[languageBinary])
        languageBinary=out/('language-ir-contract'+suffix)
        run('language-ir-contract.build',[cc,*flags,'-O0',languageIrContract,*links,'-o',languageBinary])
        for name in ('stdlib_scalar_conversion_reflection','stdlib_scalar_conversion_length'):
            assert 'rejected before output' in run(name+'.contract',[languageBinary,root/'test/cases'/(name+'.xl')])
        for name in (*runtime[:3],'stdlib_native_scalar_reflection','stdlib_native_scalar_generic','stdlib_native_scalar_fields'):
            assert 'rejected before output' in run(name+'.contract',[binary,root/'test/cases'/(name+'.xl')])
        binary=out/('scalar-contract'+suffix);run('scalar-contract.build',[cc,*flags,'-O0',scalarContract,*links,'-o',binary])
        for name in runtime[3:6]:
            assert 'rejected before output' in run(name+'.contract',[binary,root/'test/cases'/(name+'.xl')])
        binary=out/('future-contract'+suffix);run('future-contract.build',[cc,*flags,'-O0',futureContract,*links,'-o',binary])
        for name in runtime[6:9]:
            assert 'rejected before output' in run(name+'.contract',[binary,root/'test/cases'/(name+'.xl')])
        binary=out/('variadic-contract'+suffix);run('variadic-contract.build',[cc,*flags,'-O0',variadicContract,*links,'-o',binary])
        for name in runtime[-3:]:
            assert 'rejected before output' in run(name+'.contract',[binary,root/'test/cases'/(name+'.xl')])
        binary=out/('func-contract'+suffix);run('func-contract.build',[cc,*flags,'-O0',funcContract,*links,'-o',binary])
        for name in runtime[9:12]:
            assert 'rejected before output' in run(name+'.contract',[binary,root/'test/cases'/(name+'.xl')])
        binary=out/('default-contract'+suffix);run('default-contract.build',[cc,*flags,'-O0',defaultContract,*links,'-o',binary])
        for name in runtime[12:15]:
            assert 'rejected before output' in run(name+'.contract',[binary,root/'test/cases'/(name+'.xl')])
        binary=out/('method-contract'+suffix);run('method-contract.build',[cc,*flags,'-O0',methodContract,*links,'-o',binary])
        for name in runtime[15:17]:
            assert 'rejected before output' in run(name+'.contract',[binary,root/'test/cases'/(name+'.xl')])
        binary=out/('integer-contract'+suffix);run('integer-contract.build',[cc,*flags,'-O0',integerContract,*links,'-o',binary])
        for name in runtime[17:20]:
            assert 'rejected before output' in run(name+'.contract',[binary,root/'test/cases'/(name+'.xl')])
        artifactBinary=out/('artifact-plan'+suffix);run('artifact-plan.build',[cc,*flags,'-O0',artifact,*links,'-o',artifactBinary])
        memoryObjects={}
        for opt in (0,2):
            memoryObjects[opt]=out/('memory.O'+str(opt)+'.o')
            run('memory.O'+str(opt),[cc,*flags,*san,'-O'+str(opt),'-c',memory,'-o',memoryObjects[opt]])
        for name in ('stdlib_native_scalar_reflection','stdlib_native_scalar_as_only'):
            for mode in ('ir-typed','ir-lowered'):
                target=out/(name+'.'+mode+'.json');run(name+'.'+mode,[build['Driver'],root/'test/cases'/(name+'.xl'),mode,target])
                document=json.loads(target.read_text(encoding='utf-8-sig'));mapping=document['runtimeTypeMap']
                assert mapping['abiVersion']==3 and mapping['type']!=mapping['u64']
                reflected=[t for t in document['types'] if t['name']=='type']
                assert all(t['typeValueId']==mapping['type'] for t in reflected)
                if name=='stdlib_native_scalar_reflection':assert reflected
        for fixture in fixtures:
            name=fixture.stem;generated=out/(name+'.c');repeat=out/(name+'.repeat.c')
            run(name+'.cgen',[build['Driver'],fixture,'cgen-default',generated])
            run(name+'.repeat',[build['Driver'],fixture,'cgen-default',repeat]);assert sha(generated)==sha(repeat)
            if name.startswith('stdlib_scalar_conversion_'):
                assert not re.search(r'\b(?:strtoll|strtoull|strtod|xlir_string_to_[iu]64_checked|xlir_xvalue_to_[iu]64_checked)\s*\(',generated.read_text())
                assert 'xl6IntegerValueConvertV1' in generated.read_text()
            if name in compile_only:
                for opt in (0,2):run(name+'.O'+str(opt)+'.compile',[cc,*flags,*san,'-O'+str(opt),'-c',generated,'-o',out/(name+'.O'+str(opt)+'.o')])
                continue
            for opt in (0,2):
                binary=out/(name+'.O'+str(opt)+suffix)
                fixtureHost=variadicErrorHost if name=='stdlib_runtime_variadic_func_errors' else funcErrorHost if name=='stdlib_runtime_callable_func_errors' else defaultErrorHost if name=='stdlib_runtime_callable_defaults_errors' else host
                run(name+'.O'+str(opt)+'.build',[cc,*flags,*san,'-O'+str(opt),'-include',generated,fixtureHost,memoryObjects[opt],*links,'-o',binary])
                assert 'balanced' in run(name+'.O'+str(opt)+'.run',[binary])
                if name in (*runtime[:3],'stdlib_native_scalar_reflection','stdlib_native_scalar_generic','stdlib_native_scalar_fields',
                    'stdlib_scalar_conversion_reflection','stdlib_scalar_conversion_length'):
                    log=run(name+'.O'+str(opt)+'.oom',[binary,'--all']);match=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',log);assert match
                    runs,first=map(int,match.groups());assert runs==first+1
                    recovered=re.search(r'preserved original errors recovered prefixes=(\d+)',log)
                    prefixes.append(dict(Fixture=name,Lane='O'+str(opt),Runs=runs,FirstNoHit=first,Recovered=int(recovered.group(1)) if recovered else 0))
            for mode in ('run','module'):run(name+'.'+mode,[build['Driver'],fixture,mode,out/(name+'.'+mode+'.result')])
            target=out/(name+'.aot');run(name+'.aot.build',[build['Driver'],fixture,'exe',target])
            executable=Path(str(target)+'.exe') if os.name=='nt' else target;run(name+'.aot.run',[executable])
        run(arrayOnly.stem+'.cgen',[build['Driver'],arrayOnly,'cgen-default',out/(arrayOnly.stem+'.c')])
        for name in (*runtime[:2],'stdlib_native_scalar_as_only',arrayOnly.stem,
            'stdlib_scalar_conversion_reflection','stdlib_scalar_conversion_length'):
            generated=out/(name+'.c');prefix=out/(name+'.pruned')
            run(name+'.plan',[artifactBinary,root/'test/cases'/(name+'.xl'),prefix]);plan=json.loads(Path(str(prefix)+'.plan.json').read_text())
            assert not any('regex' in source or 'process' in source for source in plan['Sources'])
            assert not {'XRT_MODULE_FILE','XRT_MODULE_FILE_MAP','XRT_MODULE_FILE_LOCK','XRT_MODULE_FILE_TEMP'}.intersection(plan['NativeModules'])
            if name==arrayOnly.stem:
                assert not any('xl6_dict' in source for source in plan['Sources']),plan['Sources']
            if name=='stdlib_native_scalar_as_only':
                assert not any(Path(source).name in ('xl6_native_scalar_convert.c','xl6_value_native_scalar_convert.c') for source in plan['Sources']),plan['Sources']
                assert 'XRUNTIME_MODULE_RUNTIME_CONVERT' not in plan['NativeModules'],plan['NativeModules']
            standalone=Path(str(prefix)+'.main.c');standalone.write_text('#include "'+generated.name+'"\nint main(void) { if(!xlir_module_init())return 1; xlir_module_fini(); return xrtGetError() ? 2 : 0; }\n')
            binary=Path(str(prefix)+suffix)
            run(name+'.pruned.build',[cc,*flags,'-I'+str(root/'release'),'-I'+str(root/'lib/xruntime'),'-I'+str(root/'lib/xrt'),'-O0',standalone,Path(str(prefix)+'.xrt.c'),
                *[root/source for source in plan['Sources']],*systems,'-o',binary])
            run(name+'.pruned.run',[binary]);symbols=run(name+'.pruned.symbols',[nm,'--defined-only',binary]);assert re.search(r'\bxlir_module_init$',symbols,re.M)
            if name==arrayOnly.stem:assert not re.search(r'\bxl6Dict\w+',symbols)
            if name=='stdlib_native_scalar_as_only':assert not re.search(r'\b(?:xl6NativeScalarConvertV1|xl6ValueConvertNativeScalarV[12])$',symbols,re.M)
        original=(out/'stdlib_runtime_method_scalar.c').read_text(encoding='utf-8')
        changed,count=re.subn(r'if \(signature && !xrtValueTypeIdBind\(result, signature\)\) goto fail;',r'(void)signature;',original)
        assert count==1, 'Monomorphic bound methods must bind their verified identity'
        mutant=out/'lost-default-owner.c';mutant.write_text(changed,encoding='utf-8');binary=out/('lost-default-owner'+suffix)
        run('lost-default-owner.build',[cc,*flags,*san,'-O0','-include',mutant,host,memoryObjects[0],*links,'-o',binary])
        run('lost-default-owner.reject',[binary],True)
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);assert report['InputsUnchanged']
        products.extend(dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.iterdir()) if path.is_file() and (path.suffix in ('.h','.c','.o','.exe') or os.access(path,os.X_OK)))
        report['NativeScalarConversionAcceptance']=True;report['NativeScalarReflectionAcceptance']=True;report['NativeScalarGenericAcceptance']=True;report['LanguageScalarConversionAcceptance']=True;report['Complete']=True
    except Exception as error:report['Error']=repr(error);print(error,file=sys.stderr)
    finally:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
