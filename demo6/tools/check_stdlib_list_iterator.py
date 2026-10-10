"""Native List snapshots in the shared language foreach carrier.
Fresh full source compiler on both platforms. Real typed storage, exact keys,
owned projection/trace, checked failure != EOF, native generated-function inputs
and selective artifact plans. Not source factory/cast/restore cutover, shipping
DLL/SO/VFS, packaged AOT or full53 acceptance. Refined keys have a separate gate.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--public-directory',type=Path)
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args(); root=args.root.resolve(); out=args.out.resolve()
    assert not out.exists() and (not args.sanitize or os.name!='nt')
    cc=shutil.which('gcc'); assert cc
    native_sources=['test/support/list_native.c','test/support/module_product_memory.c',
        'src/runtime/xl6_list.c','src/runtime/xl6_list_value_iterator.c','src/runtime/xl6_value_iterator.c',
        'src/runtime/xl6_array_iterator.c','src/runtime/xl6_array_value_iterator.c','src/runtime/xl6_array.c',
        'src/runtime/xl6_array_erased.c','src/runtime/xl6_sequence.c',
        'src/runtime/xl6_array_reference_admission.c','src/runtime/xl6_array_reference_value.c',
        'src/runtime/xl6_string_value.c','src/runtime/xl6_nullable_i64_value.c','src/api/xl_foreign_error.c',
        'src/runtime/xl6_array_nominal_admission.c','src/runtime/xl6_array_primitive_admission.c',
        'src/runtime/xl6_array_c_scalar_admission.c','src/runtime/xl6_value_admission.c',
        'src/runtime/xl6_nominal_admission.c','src/runtime/xl6_string_type.c',
        'src/runtime/xl6_nullable_string.c','src/runtime/xl6_nullable_i64.c',
        'src/runtime/xl6_ownership_plan.c','src/runtime/xl6_array_future_admission.c',
        'src/runtime/xl6_char_type.c','src/runtime/xl6_char_value.c']
    host=root/'test/cases/stdlib_list_iterator_native_host.c'
    fixtures=[root/'test/cases'/('stdlib_list_iterator'+suffix+'.xl') for suffix in
        ('','_array_prune','_continuation','_checked_families')]
    contract=root/'test/cases/xlir_list_iterator_contract.c'
    language_host=root/'test/cases/stdlib_list_iterator_language_host.c'
    module_host=root/'test/cases/xlir_module_product_oom_host.c'
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),host,contract,language_host,module_host,*fixtures,
        root/'test/cases/xlir_artifact_runtime_contract.c'}
    files.update(root/path for path in native_sources)
    for folder in ('src','lib','release/include','test/support'): files.update((root/folder).rglob('*.h'))
    public=None; product=None
    if args.public_directory:
        public_dir=args.public_directory.resolve(); manifest=public_dir/'build.json'
        public=json.loads(manifest.read_text(encoding='utf-8-sig'))
        assert public['SourceXrt'] and not public['RuntimeOnly'] and public['EnableExeCompiler']==1
        if os.name!='nt': assert public['Complete'] and public['InputsUnchanged']
        driver=Path(public['Driver']); assert sha(driver).lower()==public['DriverSha256'].lower()
        product=root/'release/xl.dll' if os.name=='nt' else Path(public['NativeRoot'])/'release/libxl.so'
        files.update((manifest,driver,product))
        for row in public['Inputs']+public['Headers']+public['Objects']:
            assert sha(row['Path']).lower()==row['Sha256'].lower(),row['Path']; files.add(Path(row['Path']))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(parents=True); rows=[]; prefixes=[]; controls=[]
    report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Platform=sys.platform,Inputs=inputs,
        Rows=rows,ClosedPrefixes=prefixes,ImplementationNegativeControls=controls,NativeOnly=public is None,
        SelectedNativeClosureSanitized=args.sanitize,WholeCompilerSanitized=False,
        LanguageListMigrationComplete=False,ShippingProductAcceptance=False,Full53Complete=False,
        NativeFailureInjection='External Begin failures without diagnostics; generated native-input harness injects checked snapshot/advance status, not all OS failures')
    env=dict(os.environ)
    if product and os.name=='nt': shutil.copy2(product,out/'xl.dll')
    if public and os.name!='nt': env['XL6_SOURCE_NATIVE_ROOT']=public['NativeRoot']
    if args.sanitize: env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
        UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(root/'src/runtime')]+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
    sanitizer=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if args.sanitize else []
    native_links=['-Wl,--wrap=xl6ErrorScopeBegin']+([] if os.name=='nt' else ['-pthread','-lm'])
    suffix='.exe' if os.name=='nt' else ''
    def run(name,command,reject=False):
        command=list(map(str,command)); started=time.monotonic()
        result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=240)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        ok=result.returncode!=0 if reject else result.returncode==0
        rows.append(dict(Name=name,Command=command,Exit=result.returncode,Seconds=time.monotonic()-started,
            Complete=ok,ExpectedRejection=reject)); print(name+': '+str(result.returncode),flush=True)
        assert ok,(name,data[-6000:].decode(errors='replace')); return data.decode(errors='replace')
    def closed(name,log):
        matches=re.findall(r'closed-batch(?: function=(\d+))? runs=(\d+) first-no-hit=(\d+)',log)
        assert matches,log
        for function,runs,first in matches:
            assert int(runs)==int(first)+1
            pattern=r'function='+function+r' point=(\d+) hit=(\d) balanced' if function else r'point=(\d+) hit=(\d) balanced'
            points=re.findall(pattern,log)
            assert points==[(str(i),str(int(i<int(first)))) for i in range(int(runs))]
            prefixes.append(dict(Name=name,Function=function,Runs=int(runs),FirstNoHit=int(first)))
    try:
        for level in (0,2):
            lane='native.O'+str(level); binary=out/(lane+suffix)
            run(lane+'.build',[cc,*flags,*sanitizer,'-O'+str(level),host,*[root/p for p in native_sources],*native_links,'-o',binary])
            for mode in ('vectors','rejected'): assert 'balanced' in run(lane+'.'+mode,[binary,mode])
            for operation in ('create','advance'):
                for mode in ('oom','scopes'): closed(lane+'.'+operation+'.'+mode,run(lane+'.'+operation+'.'+mode,[binary,operation,mode]))
        path=root/'src/runtime/xl6_list_value_iterator.c'; body=path.read_text(encoding='utf-8')
        for name,before,after,arguments in (
            ('integer-key-lost','key->Type=XVALUE_KEY_INT; key->Integer=integer;',
             'key->Type=XVALUE_KEY_INDEX; key->Index=(size_t)integer;',['vectors']),
            ('failure-is-eof','return ok;','(void)ok; return true;',['advance','scopes']),
            ('snapshot-skipped','return xl6ValueIteratorCreateResidentV1(source,&ops);',
             'return xl6ValueIteratorRuntimeV2Get()->Create(source);',['vectors'])):
            assert body.count(before)>=1
            mutant=out/('mutant-'+name+'.c'); mutant.write_text(body.replace(before,after,1),encoding='utf-8')
            binary=out/('mutant-'+name+suffix)
            run(name+'.build',[cc,*flags,*sanitizer,'-O0',host,
                *[mutant if root/p==path else root/p for p in native_sources],*native_links,'-o',binary])
            run(name+'.reject',[binary,*arguments],reject=True)
            controls.append(dict(Name=name,Rejected=True,Source=str(mutant),Sha256=sha(mutant)))
        if public:
            objects=[r['Path'] for r in public['Objects'] if Path(r['Path']).name not in
                ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
            full=flags+['-DXL_STATIC_EMBED','-I'+str(root/'lib/tcc'),'-I'+str(public_dir/'include'),
                '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
                '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
            links=[product]+(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else
                ['-Wl,-rpath,'+str(product.parent),'-pthread','-ldl','-lm'])
            binary=out/('contract'+suffix); bindings=out/'bindings.h'
            run('contract.build',[cc,*full,'-O0',contract,*objects,*links,'-o',binary])
            assert 'Array excludes List' in run('contract.selection',[binary,*fixtures[:2],bindings])
            for index,fixture in enumerate(fixtures):
                tag=('list','array','continuation','checked-families')[index]; generated=out/(tag+'.c'); repeat=out/(tag+'.repeat.c')
                run(tag+'.cgen',[driver,fixture,'cgen-default',generated]); run(tag+'.repeat',[driver,fixture,'cgen-default',repeat])
                assert sha(generated)==sha(repeat); text=generated.read_text(encoding='utf-8-sig')
                assert 'xlir_xvalue_iterator_create_checked(' not in text
                if index in (0,2): assert 'xl6ListValueIteratorCreateV1(' in text and 'xl6ListCreateTakeV1(' not in text
                else: assert 'xl6_list_value_iterator.h' not in text and 'xl6ListValueIteratorCreateV1(' not in text
                if index==1: assert 'xl6ArrayValueIteratorCreateCheckedV1(' in text
                if index==3: assert 'xl6SetValueIteratorCreateCheckedV1(' in text and 'xl6DictValueIteratorCreateCheckedV1(' in text
                if index==0: assert 'xl6CollectionValueIteratorCreateV1(' in text
                for level in (0,2):
                    lane=tag+'.O'+str(level); memory=out/(lane+'.memory.o'); executable=out/(lane+suffix)
                    run(lane+'.memory',[cc,*flags,'-O'+str(level),'-c',root/'test/support/module_product_memory.c','-o',memory])
                    run(lane+'.build',[cc,*full,'-O'+str(level),'-include',generated,module_host,memory,*objects,*links,'-o',executable])
                    assert 'balanced' in run(lane+'.run',[executable])
                    if index==0:
                        executable=out/(lane+'.native-input'+suffix)
                        run(lane+'.native-input.build',[cc,*full,'-O'+str(level),'-include',generated,'-include',bindings,
                            language_host,memory,*objects,*links,'-Wl,--wrap=xl6ListIteratorSnapshotErasedV1',
                            '-Wl,--wrap=xl6ListIteratorAdvanceValueV1','-o',executable])
                        for mode in ('oom','scopes'):
                            closed(lane+'.native-input.'+mode,run(lane+'.native-input.'+mode,[executable,*(['scopes'] if mode=='scopes' else [])]))
                run(tag+'.jit',[driver,fixture,'run',out/(tag+'.jit.result')])
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('List iterator gate: '+str(len(rows))+' phases, complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
