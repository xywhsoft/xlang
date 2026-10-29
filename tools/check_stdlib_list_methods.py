"""Checked List method source dispatch and real resident native operations.

Fresh full source driver required on both platforms. Language factories now
produce native List owners; native operations use real owners, never a
test container or an injected public factory. Not complete List carrier,
shipping DLL/SO/VFS, packaged AOT or full53 acceptance.
"""
from __future__ import annotations
import argparse, hashlib, json, os, re, shutil, subprocess, sys, time
from pathlib import Path

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true',help='Instrument the selected real native closure only, not full compiler objects')
    parser.add_argument('--continuation-fixture',type=Path,help='Explicit current test-only source; recorded and hashed, no compiler/product override')
    parser.add_argument('--continuation-only',action='store_true',help='Focused supplement: only that current fixture, no native/catalog acceptance claim')
    args=parser.parse_args(); root=args.root.resolve(); public_dir=args.public_directory.resolve(); out=args.out.resolve()
    assert not out.exists() and (not args.sanitize or os.name!='nt')
    assert not args.continuation_only or args.continuation_fixture
    manifest=public_dir/'build.json'; public=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert public['SourceXrt'] and not public['RuntimeOnly'] and public['EnableExeCompiler']==1
    if os.name!='nt': assert public['Complete'] and public['InputsUnchanged']
    cc=shutil.which('gcc'); assert cc
    driver=Path(public['Driver']); assert sha(driver).lower()==public['DriverSha256'].lower()
    product=root/'release/xl.dll' if os.name=='nt' else Path(public['NativeRoot'])/'release/libxl.so'
    host=root/'test/cases/stdlib_list_methods_native_host.c'
    sources=['test/support/list_native.c','test/support/module_product_memory.c',
        'src/runtime/xl6_list.c','src/runtime/xl6_list_methods.c','src/runtime/xl6_sequence.c',
        'src/runtime/xl6_array_reference_admission.c','src/runtime/xl6_array_reference_value.c',
        'src/runtime/xl6_string_value.c','src/runtime/xl6_nullable_i64_value.c','src/api/xl_foreign_error.c',
        'src/runtime/xl6_array_nominal_admission.c','src/runtime/xl6_array_primitive_admission.c',
        'src/runtime/xl6_array_c_scalar_admission.c','src/runtime/xl6_value_admission.c',
        'src/runtime/xl6_nominal_admission.c','src/runtime/xl6_string_type.c',
        'src/runtime/xl6_nullable_string.c','src/runtime/xl6_nullable_i64.c',
        'src/runtime/xl6_ownership_plan.c','src/runtime/xl6_array_future_admission.c',
        'src/runtime/xl6_char_type.c','src/runtime/xl6_char_value.c']
    fixtures=[root/'test/cases'/('stdlib_list_methods'+suffix+'.xl') for suffix in ('','_array_prune','_dict_prune')]
    if args.continuation_fixture: fixtures.append(args.continuation_fixture.resolve())
    contract=root/'test/cases/xlir_list_methods_contract.c'
    language_host=root/'test/cases/xlir_module_product_oom_host.c'
    memory=root/'test/support/module_product_memory.c'
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),manifest,driver,product,host,contract,language_host,
           root/'test/cases/xlir_artifact_runtime_contract.c',*fixtures}
    files.update(root/path for path in sources)
    for folder in ('src','lib','release/include','test/support'):
        files.update((root/folder).rglob('*.h'))
    for row in public['Inputs']+public['Headers']+public['Objects']:
        assert sha(row['Path']).lower()==row['Sha256'].lower(),row['Path']; files.add(Path(row['Path']))
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(parents=True); rows=[]; sweeps=[]; controls=[]
    report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,'Inputs':inputs,'Rows':rows,
        'Platform':sys.platform,'ClosedPrefixes':sweeps,'ImplementationNegativeControls':controls,
        'SelectedNativeClosureSanitized':args.sanitize,'WholeCompilerSanitized':False,
        'LanguageListMigrationComplete':False,'ShippingProductAcceptance':False,'Full53Complete':False,
        'NativeResidentOptimizations':[] if args.continuation_only else ['O0','O2'],'LinkedSourceCompilerOptimization':'O0',
        'FocusedContinuationOnly':args.continuation_only,
        'ScopeInjection':'GNU ld wrap of external xl6ErrorScopeBegin call boundaries, failure without diagnostic; not every internal BeginCleanup/OS call'}
    environment=os.environ.copy()
    if os.name=='nt': shutil.copy2(product,out/'xl.dll')
    else: environment['XL6_SOURCE_NATIVE_ROOT']=public['NativeRoot']
    if args.sanitize: environment.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
        UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    suffix='.exe' if os.name=='nt' else ''
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include')]+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
    native_links=['-Wl,--wrap=xl6ErrorScopeBegin']+([] if os.name=='nt' else ['-pthread','-lm'])
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
                         Complete=accepted,ExpectedRejection=reject))
        print(name+': '+str(result.returncode),flush=True)
        assert accepted,(name,data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    try:
        for level in (() if args.continuation_only else (0,2)):
            lane='O'+str(level); binary=out/('native-'+lane+suffix)
            run(lane+'.native.build',[cc,*flags,*sanitized,'-O'+str(level),host,*[root/p for p in sources],*native_links,'-o',binary])
            for mode in ('vectors','profiles','rejected'): assert 'balanced' in run(lane+'.'+mode,[binary,mode])
            for operation in ('count','nullable-count','boxed-count','contains','take','remove','clear','trim'):
                for mode in ('oom','scopes'):
                    log=run(lane+'.'+operation+'.'+mode,[binary,operation,mode])
                    match=re.search(r'closed-batch runs=(\d+) first-no-hit=(\d+)',log); assert match
                    runs,first=map(int,match.groups()); assert runs==first+1
                    points=re.findall(r'point=(\d+) hit=(\d) balanced',log)
                    assert points==[(str(i),str(int(i<first))) for i in range(runs)]
                    sweeps.append(dict(Optimization=lane,Operation=operation,Mode=mode,Runs=runs,FirstNoHit=first))
        # Broken real implementation copies must be rejected by the same host.
        body=(root/'src/runtime/xl6_list_methods.c').read_text(encoding='utf-8')
        for name,before,after,arguments in (
            ('missing-is-true','return finish(&scope,success) && found;',
             'return finish(&scope,success);',['vectors']),
            ('native-count-wire','if (kind) return xl6ListErasedCountV1(source,output);',
             'if (kind) { *output=(int64_t)xrtValueCount(source); return true; }',['vectors']),
            ('lost-failed-bit','if (!success || xrtGetError()) xl6ErrorScopeCaptureFailure(scope);',
             '(void)success; if (xrtGetError()) xl6ErrorScopeCaptureFailure(scope);',['count','scopes'])):
            if args.continuation_only: continue
            assert body.count(before)>=1
            changed=out/('mutant-'+name+'.c'); changed.write_text(body.replace(before,after,1),encoding='utf-8')
            binary=out/('mutant-'+name+suffix)
            run(name+'.mutant.build',[cc,*flags,*sanitized,'-O0',host,
                *[changed if p=='src/runtime/xl6_list_methods.c' else root/p for p in sources],*native_links,'-o',binary])
            run(name+'.mutant.reject',[binary,*arguments],reject=True)
            controls.append(dict(Name=name,Rejected=True,Source=str(changed),SourceSha256=sha(changed)))
        binary=out/('contract'+suffix)
        if not args.continuation_only:
            run('contract.build',[cc,*full_flags,'-O0',contract,*objects,*full_links,'-o',binary])
            assert 'Array/Dict exclude List' in run('contract.selection',[binary,*fixtures[:3]])
        for index,fixture in enumerate(fixtures):
            if args.continuation_only and index!=3: continue
            tag=('list','array','dict','continuation')[index]; generated=out/(tag+'.c'); repeat=out/(tag+'.repeat.c')
            run(tag+'.cgen',[driver,fixture,'cgen-default',generated])
            run(tag+'.repeat',[driver,fixture,'cgen-default',repeat]); assert sha(generated)==sha(repeat)
            text=generated.read_text(encoding='utf-8-sig')
            if not index:
                assert all(symbol+'(' in text for symbol in ('xl6ListValueCountV1','xl6ListValueCountIfPresentV1',
                    'xl6ListValueRemoveV1','xl6ListValueContainsV1','xl6ListValueTakeV1','xl6ListValueClearV1','xl6ListValueTrimV1'))
                assert 'xl6ListCreateTakeV1(' in text and 'xrtValueIntMap()' not in text
            elif index==3:
                assert all(symbol+'(' in text for symbol in ('xl6ListValueCountV1','xl6ListValueRemoveV1',
                    'xl6ListValueContainsV1','xl6ListValueTakeV1','xl6ListValueClearV1','xl6ListValueTrimV1'))
                assert 'xl6ListCreateTakeV1(' in text and 'xrtValueIntMap()' not in text
            else: assert 'xl6_list_methods.h' not in text and 'xl6ListValue' not in text
            for level in (0,2):
                lane=tag+'.O'+str(level); memory_obj=out/(lane+'.memory.o'); native=out/(lane+suffix)
                run(lane+'.memory',[cc,*flags,'-O'+str(level),'-c',memory,'-o',memory_obj])
                entry=['-DXL6_PRODUCT_USER_ENTRY'] if not index else []
                run(lane+'.build',[cc,*full_flags,'-O'+str(level),*entry,'-include',generated,language_host,
                    memory_obj,*objects,*full_links,'-o',native])
                assert 'balanced' in run(lane+'.run',[native])
            run(tag+'.jit',[driver,fixture,'run',out/(tag+'.jit.result')])
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error:
        report['Error']=repr(error); print(error,file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('List method gate: '+str(len(rows))+' phases, complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__': raise SystemExit(main())
