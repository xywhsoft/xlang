"""Native List persistence bridge: compact stable-key projection and restore.

One resident graph memo, exact private family authentication, checked parser/
seed failures, corrupted-IR refusal, real O0/O2 pruned execution and JIT.
List source factories/casts and unsupported nested recipes are not cut over.
Sanitizers instrument only the selected native projection closure.
"""
import argparse,hashlib,json,os,re,shutil,subprocess,sys
from pathlib import Path
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--test-root',type=Path); p.add_argument('--sanitize',action='store_true')
    a=p.parse_args(); root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve()
    test=a.test_root.resolve() if a.test_root else root; cases=test/'test/cases'
    assert not out.exists() and (not a.sanitize or os.name!='nt')
    manifest=public/'build.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
    if os.name!='nt': assert build['Complete'] and build['InputsUnchanged']
    cc=shutil.which('gcc'); assert cc
    driver=Path(build['Driver']); assert sha(driver).lower()==build['DriverSha256'].lower()
    product=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    fixture=cases/'stdlib_list_persistence_native.xl'; leaf=root/'test/cases/xlir_restore_copy_leaf.xl'
    restore_only=cases/'stdlib_list_persistence_restore_only.xl'
    contract=cases/'xlir_list_persistence_contract.c'; restore=cases/'stdlib_list_persistence_restore_host.c'
    project=cases/'stdlib_list_project_native_host.c'
    sources=['test/support/value_copy_native.c','test/support/module_product_memory.c',
        'src/runtime/xl6_list.c','src/runtime/xl6_list_methods.c','src/runtime/xl6_sequence.c',
        'src/runtime/xl6_array_erased.c','src/runtime/xl6_array_graph.c','src/runtime/xl6_list_graph.c',
        'src/runtime/xl6_value_copy.c','src/runtime/xl6_value_copy_graph.c',
        'src/runtime/xl6_array_reference_admission.c','src/runtime/xl6_array_reference_value.c',
        'src/runtime/xl6_string_value.c','src/runtime/xl6_nullable_i64_value.c','src/api/xl_foreign_error.c',
        'src/runtime/xl6_array_nominal_admission.c','src/runtime/xl6_array_primitive_admission.c',
        'src/runtime/xl6_array_c_scalar_admission.c','src/runtime/xl6_value_admission.c',
        'src/runtime/xl6_nominal_admission.c','src/runtime/xl6_string_type.c',
        'src/runtime/xl6_nullable_string.c','src/runtime/xl6_nullable_i64.c','src/runtime/xl6_ownership_plan.c',
        'src/runtime/xl6_array_future_admission.c','src/runtime/xl6_char_type.c','src/runtime/xl6_char_value.c',
        'src/runtime/xl6_persistence_project.c','src/runtime/xl6_persistence_project_list.c']
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),manifest,driver,product,
        fixture,restore_only,leaf,contract,restore,project,*[root/s for s in sources]}
    files.update((root/'test/cases').glob('*copy*host.c'))
    for folder in ('src','lib','release/include','test/support'): files.update((root/folder).rglob('*.h'))
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path']).lower()==row['Sha256'].lower(),row['Path']; files.add(Path(row['Path']))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(parents=True); rows=[]; prefixes=[]
    report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,
        ClosedPrefixes=prefixes,PrunedExecutions=[],ImplementationNegativeControls=[],
        SelectedProjectionClosureSanitized=a.sanitize,WholeCompilerSanitized=False,
        WholeGeneratedRestoreSanitized=False,ShippingProductAcceptance=False,Full53Complete=False,
        LanguageListFactoriesCastsMigrated=False)
    env=os.environ.copy(); suffix='.exe' if os.name=='nt' else ''
    (out/'files').mkdir(); env['XL6_PERSIST_FILE_ROOT']=str(out/'files')
    if os.name=='nt': shutil.copy2(product,out/'xl.dll')
    else: env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    if a.sanitize: env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
        UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(cases),'-Wno-unused-function']+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
    full=flags+['-DXL_STATIC_EMBED','-I'+str(root/'lib/tcc'),'-I'+str(public/'include'),
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=[r['Path'] for r in build['Objects'] if Path(r['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]+[str(product),*systems]
    if os.name!='nt': links+=['-Wl,-rpath,'+str(product.parent)]
    wraps=['-Wl,--wrap='+s for s in ('xl6ErrorScopeBegin','xl6ListErasedCloneV1','xl6ValueCloneV1',
        'xl6ValueDeepCloneNativeV1','xrtValueGraphNextV1')]
    san=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if a.sanitize else []
    def run(name,command,reject=False):
        command=list(map(str,command)); r=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=360)
        data=r.stdout+r.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=r.returncode,ExpectedRejection=reject))
        print(name+': '+str(r.returncode),flush=True)
        assert (r.returncode!=0 if reject else r.returncode==0),(name,data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    def sweep(name,binary,mode,kind,count):
        text=run(name,[binary,mode]); matches=re.findall('List '+kind+r' closed-batch (?:operation|format)=(\d+) runs=(\d+) first-no-hit=(\d+)',text)
        assert len(matches)==count
        for operation,runs,first in matches:
            runs,first=int(runs),int(first); assert runs==first+1
            points=re.findall('List '+kind+r' (?:operation|format)='+operation+r' point=(\d+) hit=(\d) balanced',text)
            assert points==[(str(i),str(int(i<first))) for i in range(runs)]
            prefixes.append(dict(Lane=name,Mode=mode,Operation=int(operation),Runs=runs,FirstNoHit=first))
    try:
        native={}
        for level in (0,2):
            lane='project.O'+str(level); selected={}
            for source in [project,*[root/s for s in sources]]:
                obj=out/(lane+'.'+source.stem+'.o'); run(lane+'.provider.'+source.stem,
                    [cc,*flags,*san,'-O'+str(level),'-c',source,'-o',obj]); selected[source]=obj
            native[level]=selected; binary=out/(lane+suffix)
            run(lane+'.build',[cc,*san,*selected.values(),*wraps,*systems,'-o',binary])
            run(lane+'.vectors',[binary,'vectors'])
            for mode in ('oom','scopes','providers'): sweep(lane+'.'+mode,binary,mode,'project',3)
        original=root/'src/runtime/xl6_persistence_project_list.c'; body=original.read_text()
        for name,before,after in (
            ('missing-provider','providers,1,NULL,0','providers,0,NULL,0'),
            ('sparse-format','output,true,true','output,true,false')):
            assert body.count(before)==1; changed=out/(name+'.c'); changed.write_text(body.replace(before,after),encoding='utf-8')
            obj=out/(name+'.o'); run(name+'.provider',[cc,*flags,*san,'-O0','-I'+str(root/'src/runtime'),'-c',changed,'-o',obj])
            binary=out/(name+suffix); run(name+'.build',[cc,*san,
                *[obj if s==original else o for s,o in native[0].items()],*wraps,*systems,'-o',binary])
            run(name+'.reject',[binary,'vectors'],True); report['ImplementationNegativeControls'].append(name)
        binary=out/('contract'+suffix); binding=out/'bindings.h'
        run('contract.build',[cc,*full,'-O0',contract,*links,'-o',binary])
        text=run('contract.run',[binary,fixture.as_posix(),leaf.as_posix(),binding,restore_only.as_posix()]); assert 'leaf pruning verified' in text
        plans=[json.loads(line) for line in text.splitlines() if line.startswith('{"Fixture":"List"')]; assert len(plans)==1
        plan=plans[0]; generated=out/'list.c'; repeat=out/'repeat.c'; lowered=out/'list.lowered.json'
        restore_plan=json.loads(next(line for line in text.splitlines() if line.startswith('{"Fixture":"ListRestoreOnly"')))
        run('lowered',[driver,fixture,'ir-lowered',lowered])
        ir=json.loads(lowered.read_text(encoding='utf-8-sig')); assert all(not r['pending'] for r in ir['persistenceRestoreRoots'])
        run('cgen',[driver,fixture,'cgen-default',generated]); run('cgen.repeat',[driver,fixture,'cgen-default',repeat]); assert sha(generated)==sha(repeat)
        text=generated.read_text(encoding='utf-8-sig'); assert 'xl6PersistenceProjectListV1(' in text and 'xl6ListImportProviderV1()' in text
        assert 'xlir_persist_json_assign_' not in text and 'xlir_persist_xlon_assign_' not in text
        for level in (0,2):
            lane='language.O'+str(level); memory=out/(lane+'.memory.o')
            run(lane+'.memory',[cc,*full,'-O'+str(level),'-c',root/'test/support/module_product_memory.c','-o',memory])
            binary=out/(lane+'.module'+suffix)
            run(lane+'.module.build',[cc,*full,'-O'+str(level),'-DXL6_PRODUCT_USER_ENTRY','-include',generated,
                root/'test/cases/xlir_module_product_oom_host.c',memory,*links,'-o',binary]); run(lane+'.module.run',[binary])
            binary=out/(lane+'.restore'+suffix)
            run(lane+'.restore.build',[cc,*full,'-O'+str(level),'-include',generated,'-include',binding,
                restore,memory,*links,'-Wl,--wrap=xl6ErrorScopeBegin','-o',binary]); run(lane+'.restore.vectors',[binary,'vectors'])
            for mode in ('oom','scopes'): sweep(lane+'.restore.'+mode,binary,mode,'restore',4)
            impl=out/(lane+'.pruned.c'); selected=[]
            impl.write_text(''.join('#define '+s+'\n' for s in plan['NativeModules'])+
                '#define XRT_MODULE_MEMORY_DEBUG\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
            for i,source in enumerate([impl,*[root/s for s in plan['Sources']]]):
                obj=out/(lane+'.pruned.'+str(i)+'.o'); run(lane+'.pruned.provider.'+str(i),[cc,*full,'-O'+str(level),'-c',source,'-o',obj]); selected.append(obj)
            binary=out/(lane+'.pruned'+suffix)
            run(lane+'.pruned.build',[cc,*full,'-O'+str(level),'-DXL6_PRODUCT_USER_ENTRY','-include',generated,
                root/'test/cases/xlir_module_product_oom_host.c',memory,*selected,*systems,'-o',binary]); run(lane+'.pruned.run',[binary])
            report['PrunedExecutions'].append(dict(Optimization=level,Sources=plan['Sources'],NativeModules=plan['NativeModules']))
        run('jit',[driver,fixture,'run',out/'jit.result'])
        generated=out/'restore-only.c'; run('restore-only.cgen',[driver,restore_only,'cgen-default',generated])
        for level in (0,2):
            lane='restore-only.O'+str(level); selected=[]; impl=out/(lane+'.c')
            impl.write_text(''.join('#define '+s+'\n' for s in restore_plan['NativeModules'])+
                '#define XRT_MODULE_MEMORY_DEBUG\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
            for i,source in enumerate([impl,*[root/s for s in restore_plan['Sources']],root/'test/support/module_product_memory.c']):
                obj=out/(lane+'.'+str(i)+'.o'); run(lane+'.provider.'+str(i),[cc,*full,'-O'+str(level),'-c',source,'-o',obj]); selected.append(obj)
            binary=out/(lane+suffix); run(lane+'.build',[cc,*full,'-O'+str(level),'-include',generated,
                root/'test/cases/xlir_module_product_oom_host.c',*selected,*systems,'-o',binary]); run(lane+'.run',[binary,'--all'])
            report['PrunedExecutions'].append(dict(Optimization=level,Fixture='restore-only',Sources=restore_plan['Sources'],NativeModules=restore_plan['NativeModules']))
        run('restore-only.jit',[driver,restore_only,'run',out/'restore-only.jit.result'])
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs); assert report['InputsUnchanged']
        report['Complete']=True
    except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
