"""Verify @owned through the public compiler, native code, JIT and AOT.

Use --driver for a Windows public source driver or --cli for a normal xl CLI.
Each run owns a fresh evidence directory; compiled artifacts are never reused.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, subprocess, sys

ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--directory',type=Path,required=True)
    compiler=p.add_mutually_exclusive_group(required=True)
    compiler.add_argument('--driver',type=Path)
    compiler.add_argument('--cli',type=Path)
    p.add_argument('--runtime-build',type=Path,
        help='Verified source runtime receipt for the independent C consumer gate')
    args=p.parse_args();work=args.directory.resolve();work.mkdir(parents=True,exist_ok=False)
    binary=(args.driver or args.cli).resolve();checks=[]
    env=os.environ.copy();env['XLANG_PACKAGE_PATH']=os.pathsep.join([str(ROOT/'test/fixtures'),str(ROOT/'release/extlibs')])
    def run(name,argv,expected=True,diagnostic=None):
        if args.cli and argv[0]==binary:
            argv=[*argv,'--inc-path',ROOT/'release/include']
        result=subprocess.run([str(s) for s in argv],cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=300)
        log=result.stdout.decode('utf-8',errors='replace');(work/(name+'.log')).write_text(log,encoding='utf-8')
        passed=(result.returncode==0)==expected and (diagnostic is None or diagnostic in log)
        checks.append({'name':name,'passed':passed,'exitCode':result.returncode})
        if not passed:raise RuntimeError(name+'\n'+log[-5000:])
        print(name+': PASS',flush=True)
    cases=[
        ('extern_owned_string','test/cases/extern_owned_string.xl',['test/cases/extern_owned_string_host.c'],['owned_text','owned_empty','owned_null','owned_optional']),
        ('extern_owned_string_package','test/cases/extern_owned_string_package.xl',[],['owned_package_text','owned_package_optional']),
        ('extern_owned_string_export','test/cases/extern_owned_string_export.xl',['test/cases/extern_owned_string_host.c'],['owned_text']),
        ('extern_owned_string_forward','test/cases/extern_owned_string_forward.xl',['test/cases/extern_owned_string_host.c'],['owned_optional']),
        ('example','release/examples/extern_owned_string.xl',['release/examples/extern_owned_string_native.c'],['example_owned_text']),
        ('cstring-regression','test/cases/xlir_c_owned_basic.xl',['test/cases/xlir_c_owned_native.c'],[]),
        ('cstring-package-regression','test/cases/xlir_c_owned_import_exports.xl',[],[]),
        ('ordinary-string-export-regression','test/cases/sem_export_string_return.xl',[],[]),
        ('C-variadic-no-raise-text','test/cases/extern_c_variadic_text.xl',['test/cases/extern_c_variadic_text_host.c'],['variadic_text']),
        ('C-variadic-function-value','test/cases/extern_c_variadic_value.xl',['test/cases/extern_c_variadic_text_host.c'],[]),
        ('borrowed-string-failure','test/cases/extern_borrowed_string_failure.xl',['test/cases/extern_borrowed_string_failure_host.c'],[]),
        ('static-method-function-value','test/cases/extern_owned_static_callable.xl',
            ['test/cases/extern_owned_string_host.c','test/cases/extern_c_variadic_text_host.c','test/cases/extern_owned_static_callable_host.c'],[]),
        ('task-C-scalar-parameters','test/cases/extern_owned_task_scalars.xl',
            ['test/cases/extern_owned_task_scalars_host.c'],['owned_task_text','owned_task_scalars']),
        ('task-extern-defaults','test/cases/extern_owned_task_defaults.xl',
            ['test/cases/extern_owned_task_scalars_host.c','test/cases/extern_owned_task_composition_host.c'],['owned_task_text']),
        ('task-generic-composition','test/cases/extern_owned_task_generic.xl',
            ['test/cases/extern_owned_task_scalars_host.c'],['owned_task_text']),
        ('task-nullable-flow','test/cases/extern_owned_task_nullable.xl',
            ['test/cases/extern_owned_task_scalars_host.c'],['owned_task_text']),
        ('task-all-C-scalars','test/cases/extern_owned_task_all_scalars.xl',
            ['test/cases/extern_owned_task_scalars_host.c','test/cases/extern_owned_task_composition_host.c'],[]),
        ('task-nullable-capture-all','test/cases/extern_owned_task_optional.xl',
            ['test/cases/extern_owned_task_scalars_host.c'],[]),
        ('task-nullable-C-scalars','test/cases/extern_owned_task_optional_scalars.xl',
            ['test/cases/extern_owned_task_scalars_host.c','test/cases/extern_owned_task_composition_host.c'],[]),
        ('task-generic-continuations','test/cases/extern_owned_task_continuations.xl',
            ['test/cases/extern_owned_task_scalars_host.c'],[]),
        ('task-generic-function-values','test/cases/extern_owned_task_generic_callable.xl',
            ['test/cases/extern_owned_task_scalars_host.c'],[]),
        ('task-nullable-callbacks','test/cases/extern_owned_task_nullable_callbacks.xl',
            ['test/cases/extern_owned_task_scalars_host.c'],[]),
        ('task-dependent-defaults','test/cases/extern_owned_task_dependent_defaults.xl',
            ['test/cases/extern_owned_task_scalars_host.c'],[]),
        ('task-error-arrays','test/cases/extern_owned_task_error_arrays.xl',
            ['test/cases/extern_owned_task_scalars_host.c','test/cases/extern_owned_string_host.c'],[]),
        ('default-capture-oom','test/cases/extern_owned_default_capture_oom.xl',
            ['test/cases/extern_owned_string_host.c'],[]),
        ('error-safe-index','test/cases/extern_owned_error_safe_index.xl',
            ['test/cases/extern_owned_task_scalars_host.c','test/cases/extern_owned_string_host.c'],[]),
        ('error-transports','test/cases/extern_owned_error_transports.xl',
            ['test/cases/extern_owned_task_scalars_host.c','test/cases/extern_owned_string_host.c'],[]),
        ('error-channel-receive','test/cases/extern_owned_error_channel_receive.xl',[],[]),
        ('method-receiver-defaults','test/cases/extern_owned_method_receiver_defaults.xl',
            ['test/cases/extern_owned_string_host.c'],[]),
        ('foreign-prefix-defaults','test/cases/extern_owned_foreign_prefix_defaults.xl',
            ['test/cases/extern_owned_task_scalars_host.c','test/cases/extern_owned_task_composition_host.c'],[]),
        ('bound-method-defaults','test/cases/extern_owned_bound_method_defaults.xl',
            ['test/cases/extern_owned_string_host.c'],[]),
        ('native-bound-defaults','test/cases/extern_owned_native_bound_defaults.xl',
            ['test/cases/extern_owned_string_host.c','test/cases/extern_owned_native_bound_defaults_host.c'],[]),
        ('nullable-generic-channel-arguments','test/cases/extern_owned_nullable_channel_arguments.xl',
            ['test/cases/extern_owned_task_scalars_host.c'],[]),
        ('error-array-extract','test/cases/extern_owned_error_array_extract.xl',
            ['test/cases/extern_owned_task_scalars_host.c','test/cases/extern_owned_string_host.c'],[]),
        ('bound-generic-constraints','test/cases/extern_owned_bound_generic_constraints.xl',[],[]),
        ('bound-constraint-casts','test/cases/extern_owned_bound_constraint_casts.xl',[],[]),
        ('bound-nonpublic','test/cases/extern_owned_bound_nonpublic.xl',
            ['test/cases/extern_owned_string_host.c','test/cases/extern_owned_typed_bound_native_host.c'],[]),
        ('bound-c-owners','test/cases/extern_owned_bound_c_owners.xl',
            ['test/cases/extern_owned_bound_c_owners_host.c','test/cases/extern_owned_string_host.c'],[]),
        ('bound-nested-generics','test/cases/extern_owned_bound_nested_generics.xl',
            ['test/cases/extern_owned_bound_c_owners_host.c','test/cases/extern_owned_string_host.c'],[]),
        ('bound-callback-arguments','test/cases/extern_owned_bound_callback_arguments.xl',
            ['test/cases/extern_owned_bound_c_owners_host.c','test/cases/extern_owned_string_host.c'],[]),
        ('bound-nullable-generic-arguments','test/cases/extern_owned_bound_nullable_generic_args.xl',
            ['test/cases/extern_owned_bound_c_owners_host.c','test/cases/extern_owned_string_host.c'],[]),
        ('bound-nullable-casts','test/cases/extern_owned_bound_nullable_casts.xl',
            ['test/cases/extern_owned_bound_c_owners_host.c','test/cases/extern_owned_string_host.c'],[]),
        ('bound-variadic-owners','test/cases/extern_owned_bound_variadic_owners.xl',
            ['test/cases/extern_owned_bound_c_owners_host.c','test/cases/extern_owned_string_host.c'],[]),
        ('generic-nullable-order','test/cases/extern_owned_generic_nullable_order.xl',[],[]),
        ('bound-base-dispatch','test/cases/extern_owned_bound_base_dispatch.xl',[],[]),
        ('bound-async-generic','test/cases/extern_owned_bound_async_generic.xl',
            ['test/cases/extern_owned_string_host.c'],[]),
        ('bound-overload-rank','test/cases/extern_owned_bound_overload_rank.xl',[],[]),
        ('bound-nullable-callable','test/cases/extern_owned_bound_nullable_callable.xl',
            ['test/cases/extern_owned_string_host.c','test/cases/extern_owned_typed_bound_native_host.c'],[]),
        ('typed-bound-methods','test/cases/extern_owned_typed_bound_methods.xl',[],[]),
        ('typed-bound-native','test/cases/extern_owned_typed_bound_native.xl',
            ['test/cases/extern_owned_string_host.c','test/cases/extern_owned_typed_bound_native_host.c'],['typed_bound_text','typed_bound_roundtrip']),
    ]
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    inputs=[ROOT/source for _,source,_,_ in cases]+[ROOT/path for _,_,paths,_ in cases for path in paths]
    inputs+=[Path(__file__),*sorted((ROOT/'test/fixtures/extern_owned_string').rglob('*'))]
    inputs+=[*sorted((ROOT/'test/cases').glob('bad_extern_owned_*.xl')),
        ROOT/'test/cases/sem_c_callback_string_return_invalid.xl']
    snapshot={str(f.relative_to(ROOT)):sha(f) for f in inputs if f.is_file()}
    for case,relative,sidecars,symbols in cases:
        source=ROOT/relative;prefix=work/case
        run(case+'-jit',[binary,'run',source,*sidecars] if args.cli else [binary,source,'run',str(prefix)+'-jit',*sidecars])
        run(case+'-aot-build',[binary,'build',source,*sidecars,'-o',prefix] if args.cli else [binary,source,'exe',prefix,*sidecars])
        run(case+'-aot',[prefix.with_suffix('.exe' if os.name=='nt' else '')])
        emit=Path(str(prefix)+'-emit')
        run(case+'-emit',[binary,'cgen',source,*sidecars,'--emit',str(emit)+'.c','--emit-header',str(emit)+'.h']
            if args.cli else [binary,source,'emit',emit,*sidecars])
        generated=Path(str(prefix)+'-emit.c').read_text(encoding='utf-8')
        for symbol in symbols:
            assert 'extern char* '+symbol+'(' in generated,symbol+' lost its exact C prototype'
            assert 'xlir_string_from_cstr('+symbol+'(' not in generated,symbol+' was copied'
            assert '.Data = '+symbol+'(' in generated,symbol+' was not adopted at its call site'
        if symbols:assert 'xl6CStringTakeV1' not in generated and 'XL6_COwnerV1' not in generated
        if case=='task-C-scalar-parameters':
            # Values alone can survive a widen/narrow round trip. Also require
            # the actual task frame to retain every exact C representation.
            frames=re.findall(r'typedef struct xlir_task_spawn_frame_(\d+) \{(.*?)\} xlir_task_spawn_frame_\1;',generated,re.S)
            native=next(body for _,body in frames if ' p19;' in body)
            types=['char','signed char','unsigned char','short','unsigned short',
                'int','unsigned int','long','unsigned long','long long','unsigned long long',
                'size_t','intptr_t','ptrdiff_t','intptr_t','uintptr_t','bool','float','double','wchar_t']
            for index,ctype in enumerate(types):
                assert '    '+ctype+' p'+str(index)+';' in native,(index,ctype,'task frame type changed')
        if case=='extern_owned_string':
            assert re.search(r'const char\* borrowed_raw_\d+ = owned_borrowed\(',generated),'borrowed C prototype changed'
            assert 'xlir_string_from_cstr(borrowed_raw_' in generated,'borrowed default changed'
        if case=='typed-bound-native':
            assert re.search(r'#define xlir_closure_invoke_(\d+) xlir_fn_\1',generated),'bound entry gained another forwarding thunk'
            assert 'xlir_bound_output' in generated,'bound result lost its typed output ABI'
        if case=='task-all-C-scalars':
            # Aggregation already produces an xvalue array. Its scalar boxer
            # must use the native profile and identity, never signed i64.
            boxers=re.findall(r'static xvalue\* xlir_future_payload_box_t\d+\(const void\* payload\)\s*\{(.*?)\n\}',generated,re.S)
            assert len(boxers)==20,len(boxers)
            assert all('xl6ValueBoxNativeScalarV1(payload,' in body for body in boxers)
            assert all('xrtValueInt(' not in body for body in boxers)
        if case=='extern_owned_string_export':
            # Reentrant C sidecar integration. The compiler injects an ABI
            # header here; independent-header coverage is a separate gate.
            consumer=work/'export-consumer.c'
            consumer.write_text('#include "'+Path(str(emit)+'.h').as_posix()+'"\n'+
                'extern bool owned_original(const char*);\n'
                'bool owned_export_consumer(void) {\n'
                ' xl6_foreign_error_scope scope={0}; if(!xl6ForeignCallBegin(&scope)) return false;\n'
                ' xlir_string value=owned_export_text(); xl6_foreign_error_result outcome=xl6ForeignCallEnd(&scope);\n'
                ' bool ok=outcome.ok && value.Data && value.Size==10 && owned_original(value.Data);\n'
                ' xrtFree((void*)value.Data); xrtErrorFree(outcome.error);\n'
                ' if(!xl6ForeignCallBegin(&scope)) return false;\n'
                ' value=owned_export_optional(); outcome=xl6ForeignCallEnd(&scope);\n'
                ' ok=ok && outcome.ok && !value.Data && !value.Size; xrtErrorFree(outcome.error);\n'
                ' if(!xl6ForeignCallBegin(&scope)) return false;\n'
                ' value=owned_export_failed(); outcome=xl6ForeignCallEnd(&scope);\n'
                ' ok=ok && !outcome.ok && outcome.error && !value.Data && !value.Size; xrtErrorFree(outcome.error);\n'
                ' if(!xl6ForeignCallBegin(&scope)) return false;\n'
                ' value=owned_export_failed_without_diagnostic(); outcome=xl6ForeignCallEnd(&scope);\n'
                ' ok=ok && !outcome.ok && !value.Data && !value.Size; xrtErrorFree(outcome.error); return ok; }\n',encoding='utf-8')
            entry=work/'export-entry.xl'
            program=source.read_text(encoding='utf-8')
            assert '    return 0\n' in program
            entry.write_text(program.replace('    return 0\n','    assert consumer()\n    return 0\n',1)+
                '\nextern "owned_export_consumer" @no_raise func consumer() as bool\n',encoding='utf-8')
            extra=[*sidecars,consumer]
            run('export-C-consumer-jit',[binary,'run',entry,*extra] if args.cli else [binary,entry,'run',work/'consumer-jit',*extra])
            run('export-C-consumer-aot-build',[binary,'build',entry,*extra,'-o',work/'consumer-aot']
                if args.cli else [binary,entry,'exe',work/'consumer-aot',*extra])
            run('export-C-consumer-aot',[(work/'consumer-aot').with_suffix('.exe' if os.name=='nt' else '')])
    negatives={
        'test/cases/bad_extern_owned_bound_constraint.xl':'typed bound method violates generic constraints',
        'test/cases/bad_extern_owned_bound_nested_constraint.xl':'typed bound method violates generic constraints',
        'test/cases/bad_extern_owned_bound_generic_argument.xl':'no matching callable class member',
        'test/cases/bad_extern_owned_bound_nullable_generic_args.xl':'no matching callable class member',
        'test/cases/bad_extern_owned_bound_nullable_cast_private.xl':'unknown class/union field',
        'test/cases/bad_extern_owned_bound_nullable_cast_constraint.xl':'typed bound method violates generic constraints',
        'test/cases/bad_extern_owned_bound_variadic_pack.xl':'typed bound method violates generic constraints',
        'test/cases/bad_extern_owned_bound_variadic_constraint.xl':'typed bound method violates generic constraints',
        'test/cases/bad_extern_owned_bound_private.xl':'unknown class/union field',
        'test/cases/bad_extern_owned_bound_c_owner_kind.xl':'exact signature',
        'test/cases/bad_extern_owned_bound_c_owner_dynamic.xl':'no exact target identity and result ownership ABI',
        'test/cases/bad_extern_owned_bound_protected.xl':'unknown class/union field',
        'test/cases/bad_extern_owned_bound_constraint_cast.xl':'typed bound method violates generic constraints',
        'test/cases/bad_extern_owned_bound_async_constraint_cast.xl':'typed bound method violates generic constraints',
        'test/cases/bad_extern_owned_bound_constraint_nullable_cast.xl':'typed bound method violates generic constraints',
        'test/cases/bad_extern_owned_bound_async_constraint.xl':'typed bound method violates generic constraints',
        'test/cases/bad_extern_owned_nullable_derived_invariant.xl':None,
        'test/cases/bad_extern_owned_bound_ambiguous.xl':'typed bound method requires one exact overload',
        'test/cases/bad_extern_owned_integer.xl':'@owned requires',
        'test/cases/bad_extern_owned_product.xl':'@owned requires',
        'test/cases/bad_extern_owned_constructor.xl':'@owned requires',
        'test/cases/bad_extern_owned_no_raise.xl':'@owned @no_raise requires',
        'test/cases/bad_extern_owned_conflict.xl':'incompatible ABI',
        'test/cases/bad_extern_owned_member_conflict.xl':'conflicting string ownership',
        'test/cases/bad_extern_owned_registered.xl':'bridge',
        'test/cases/bad_extern_owned_unknown.xl':'unknown extern effect contract',
        'test/cases/bad_extern_owned_variadic_tail.xl':'native variadic function value tail',
        'test/cases/bad_extern_owned_variadic_signature.xl':'native variadic func requires',
        'test/cases/bad_extern_owned_static_private.xl':'explicit func signature',
        'test/cases/bad_extern_owned_static_ambiguous.xl':'explicit func signature',
        'test/cases/bad_extern_owned_static_instance.xl':'explicit func signature',
        'test/cases/bad_extern_owned_task_variadic.xl':'task.spawn argument type does not match func signature',
        'test/cases/bad_extern_owned_task_nullable.xl':'task.spawn first argument must be a func value',
        'test/cases/bad_extern_owned_task_nullable_invalidated.xl':'task.spawn first argument must be a func value',
        'test/cases/bad_extern_owned_nullable_invariant.xl':'泛型函数类型推断冲突',
    }
    for relative,message in negatives.items():
        source=ROOT/relative;name=source.stem.removeprefix('bad_extern_owned_')
        run('reject-'+name,[binary,'cgen',source,'--emit',work/('reject-'+name+'.c')] if args.cli else [binary,source,'emit',work/('reject-'+name)],False,message)
    run('reject-raw-C-callback-string',[binary,'check',ROOT/'test/cases/sem_c_callback_string_return_invalid.xl']
        if args.cli else [binary,ROOT/'test/cases/sem_c_callback_string_return_invalid.xl','emit',work/'reject-callback'],False)
    receipt=args.runtime_build or (binary.parent/'build.json' if args.driver else None)
    if receipt:
        run('independent-C-public-header',[sys.executable,ROOT/'tools/check_extern_owned_string_public.py',
            '--cli' if args.cli else '--driver',binary,'--runtime-build',receipt,
            '--directory',work/'independent-C'])
    else:
        print('independent C gate not run: source runtime receipt required via --runtime-build',flush=True)
    assert all(sha(ROOT/f)==digest for f,digest in snapshot.items()),'Test inputs changed'
    (work/'result.json').write_text(json.dumps({'complete':True,'platform':sys.platform,'compiler':str(binary),
        'compilerSha256':sha(binary),'independentCConsumer':bool(receipt),'checks':checks,'inputs':snapshot},indent=2),encoding='utf-8')
    print('extern owned string checks complete',flush=True)

if __name__=='__main__':main()
