"""DynamicFields native List/Array/wire graph and typed restore isolation gate.

Fresh complete source compiler, strict C O0/O2, JIT and closed OOM/scope/provider
prefixes. Optional Linux sanitizers cover the selected native closure only.
This is not List factory/cast/persistence cutover or full53 shipping acceptance.
"""
import argparse,hashlib,json,os,re,shutil,subprocess,sys
from pathlib import Path

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true'); a=p.parse_args()
    root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve(); assert not out.exists()
    manifest=public/'build.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
    cc=shutil.which('gcc'); assert cc and (not a.sanitize or os.name!='nt')
    native=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    source=['test/support/dynamic_fields_list_native.c','test/support/module_product_memory.c',
        'src/runtime/xl6_set.c','src/runtime/xl6_set_key_policy.c',
        'src/runtime/xl6_dict.c',
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
        'src/runtime/xl6_dynamic_fields.c','src/runtime/xl6_dynamic_fields_copy.c']
    host=root/'test/cases/stdlib_dynamic_fields_list_host.c'
    contract=root/'test/cases/xlir_restore_copy_provider_contract.c'
    language_host=root/'test/cases/stdlib_restore_copy_list_host.c'
    fixtures=[root/'test/cases'/('xlir_restore_copy_'+name+'.xl') for name in ('leaf','open')]
    files={Path(__file__).resolve(),Path(cc),Path(sys.executable),manifest,Path(build['Driver']),native,host,contract,language_host,*fixtures,
        root/'test/cases/stdlib_value_copy_native_host.c',root/'test/cases/xlir_artifact_runtime_contract.c',
        root/'test/cases/xlir_module_product_oom_host.c',root/'test/support/list_native.c',root/'test/support/value_copy_native.c',
        *[root/s for s in source]}
    for folder in ('src','lib','release/include','test/support'): files.update((root/folder).rglob('*.h'))
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(parents=True); rows=[]; prefixes=[]; objects=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,
        Scope=__doc__,Objects=objects,SelectedNativeClosureSanitized=a.sanitize,WholeCompilerSanitized=False,
        Full53Complete=False,LanguageListMigrationComplete=False,ImplementationNegativeControls=[])
    env=dict(os.environ); suffix='.exe' if os.name=='nt' else ''
    if os.name=='nt': shutil.copy2(native,out/'xl.dll')
    else: env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    if a.sanitize: env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-Wno-unused-function']+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
    san=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if a.sanitize else []
    wraps=['-Wl,--wrap='+name for name in ('xl6ErrorScopeBegin','xl6ListErasedCloneV1','xl6ValueCloneV1',
        'xl6ValueDeepCloneNativeV1','xrtValueGraphNextV1','xrtDynamicFieldsMergeRefV1')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    full=flags+['-DXL_STATIC_EMBED','-I'+str(root/'lib/tcc'),'-I'+str(public/'include'),
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    links=[r['Path'] for r in build['Objects'] if Path(r['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    links += [str(native),*systems]+([] if os.name=='nt' else ['-Wl,-rpath,'+str(native.parent)])
    def run(name,command,reject=False):
        result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=300)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=result.returncode,ExpectedRejection=reject))
        print(name+': '+str(result.returncode),flush=True)
        assert (result.returncode!=0 if reject else result.returncode==0),(name,data[-5000:].decode(errors='replace'))
        return data.decode(errors='replace')
    try:
        compiled={}
        for opt in (0,2):
            lane='O'+str(opt); selected={}
            for item in [host,*[root/s for s in source]]:
                obj=out/(lane+'.'+item.stem+'.o')
                run(lane+'.provider.'+item.stem,[cc,*flags,*san,'-O'+str(opt),'-c',item,'-o',obj]); selected[item]=obj
                objects.append(dict(Path=str(obj),Source=str(item),Sha256=sha(obj)))
            compiled[opt]=selected; binary=out/(lane+suffix)
            run(lane+'.build',[cc,*san,*selected.values(),*wraps,*systems,'-o',binary])
            run(lane+'.vectors',[binary,'vectors'])
            for mode in ('oom','scopes','providers'):
                log=run(lane+'.'+mode,[binary,mode]); matches=re.findall(
                    r'DynamicFields List closed-batch operation=(\d+) runs=(\d+) first-no-hit=(\d+)',log)
                assert len(matches)==7
                for operation,runs,first in matches:
                    assert int(runs)==int(first)+1
                    points=re.findall(r'DynamicFields List operation='+operation+r' point=(\d+) hit=(\d) balanced',log)
                    assert points==[(str(i),str(int(i<int(first)))) for i in range(int(runs))]
                    prefixes.append(dict(Lane=lane,Mode=mode,Operation=int(operation),Runs=int(runs),FirstNoHit=int(first)))
        original=root/'src/runtime/xl6_dynamic_fields_copy.c'; body=original.read_text(encoding='utf-8')
        for name,before,after in (
            ('missing-list-provider','xl6ValueCloneGraphV1','xl6ValueDeepCloneV1'),
            ('second-per-field-clone','xrtDynamicFieldsMergeRefV1','xrtDynamicFieldsMerge')):
            assert before in body; mutant=out/(name+'.c'); mutant.write_text(body.replace(before,after),encoding='utf-8')
            obj=out/(name+'.o'); run(name+'.provider',[cc,*flags,*san,'-O0','-I'+str(root/'src/runtime'),'-include',
                root/'src/runtime/xl6_array_graph.h','-c',mutant,'-o',obj])
            binary=out/(name+suffix); run(name+'.build',[cc,*san,
                *[obj if s==original else o for s,o in compiled[0].items()],*wraps,*systems,'-o',binary])
            run(name+'.reject',[binary,'vectors'],True); report['ImplementationNegativeControls'].append(name)
        binary=out/('contract'+suffix)
        run('contract.build',[cc,*full,'-O0',contract,*links,'-o',binary])
        binding=out/'restore-copy-bindings.h'
        assert 'provider plans verified' in run('contract.run',[binary,*fixtures,binding])
        for fixture in fixtures:
            name=fixture.stem; generated=out/(name+'.c'); repeat=out/(name+'.repeat.c')
            run(name+'.cgen',[build['Driver'],fixture,'cgen-default',generated])
            run(name+'.repeat',[build['Driver'],fixture,'cgen-default',repeat]); assert sha(generated)==sha(repeat)
            for opt in (0,2):
                binary=out/(name+'.O'+str(opt)+suffix)
                memory=out/(name+'.memory.O'+str(opt)+'.o')
                run(name+'.memory.O'+str(opt),[cc,*full,'-O'+str(opt),'-c',
                    root/'test/support/module_product_memory.c','-o',memory])
                run(name+'.O'+str(opt)+'.build',[cc,*full,'-O'+str(opt),'-include',generated,
                    root/'test/cases/xlir_module_product_oom_host.c',memory,*links,'-o',binary])
                assert 'balanced' in run(name+'.O'+str(opt)+'.run',[binary])
                if fixture==fixtures[1]:
                    genuine=out/(name+'.native.O'+str(opt)+suffix)
                    run(name+'.native.O'+str(opt)+'.build',[cc,*full,'-O'+str(opt),'-include',generated,'-include',binding,
                        language_host,memory,*links,*wraps,'-o',genuine])
                    run(name+'.native.O'+str(opt)+'.vectors',[genuine,'vectors'])
                    for mode in ('oom','providers'):
                        log=run(name+'.native.O'+str(opt)+'.'+mode,[genuine,mode])
                        match=re.search(r'Restore List closed-batch runs=(\d+) first-no-hit=(\d+)',log); assert match
                        runs,first=map(int,match.groups()); assert runs==first+1
                        points=re.findall(r'Restore List point=(\d+) hit=(\d) balanced',log)
                        assert points==[(str(i),str(int(i<first))) for i in range(runs)]
                        prefixes.append(dict(Lane='restore.O'+str(opt),Mode=mode,Runs=runs,FirstNoHit=first))
            run(name+'.jit',[build['Driver'],fixture,'run',out/(name+'.jit.result')])
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs); assert report['InputsUnchanged']
        report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.iterdir()) if path.is_file() and
            (path.suffix in ('.c','.o','.exe') or os.access(path,os.X_OK))]
        report['Complete']=True
    except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1

if __name__=='__main__': raise SystemExit(main())
