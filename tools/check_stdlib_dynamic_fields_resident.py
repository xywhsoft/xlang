"""Current-source DynamicFields resident ABI, graph and language regression.
Strict generated C O0/O2, physical native contracts, genuine TCC JIT and AOT.
Compiler and native inputs are immutable; no shipping/full53 claim.
"""
from pathlib import Path
from compiler_response import command_with_response
import argparse, hashlib, json, os, re, shutil, subprocess, sys
from xlir_public_build_inputs import verify_public_build

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--native-contract',type=Path)
    args=parser.parse_args(); root=args.root.resolve(); public=args.public_directory.resolve(); out=args.out.resolve()
    assert not out.exists()
    manifest=public/'build.json'; build,pinned,compared=verify_public_build(root,public)
    assert build['SourceXrt'] and not build['RuntimeOnly']
    if os.name!='nt': assert build['Complete'] and build['InputsUnchanged']
    files={Path(__file__).resolve(),Path(sys.executable),manifest,Path(build['Driver'])}
    files.update(pinned)
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(),row['Path']; files.add(Path(row['Path']))
    cc=shutil.which('gcc'); assert cc
    native=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    files.update((Path(cc),native))
    cases=('stdlib_dynamic_fields_resident','xruntime_dynamic_fields','xlir_native_dynamic_fields_values',
           'xlir_native_dynamic_fields_auto_destroy','xlir_native_dynamic_fields_explicit_destroy',
           'cgen_object_var_field_lifetime','stdlib_dynamic_fields_cycle')
    contracts=('stdlib_dynamic_fields_resident_host','xlir_dynamic_fields_contract_host',
               'stdlib_persistence_project_native_host','stdlib_object_fields_native_host')
    host=root/'test/cases/xlir_module_product_oom_host.c'; memory=root/'test/support/module_product_memory.c'
    files.update((host,memory,root/'test/support/module_product_memory.h'))
    plan_source=root/'test/cases/xlir_object_fields_public_plan.c'
    absent=root/'test/cases/stdlib_thread_artifact_absent.xl'
    objdump=shutil.which('objdump'); assert objdump
    files.update((plan_source,absent,Path(objdump)))
    files.update(root/'test/cases'/(name+'.xl') for name in cases)
    files.update(root/'test/cases'/(name+'.c') for name in contracts)
    if args.native_contract: files.add(args.native_contract.resolve())
    files.update((root/'test/cases/xlir_dynamic_fields_contract.inc',
        root/'test/cases/stdlib_array_graph_native_host.c',root/'test/cases/stdlib_sequence_reference_fixture.inc'))
    # Pin the transitive native fixture include population as well, rather
    # than trusting just each contract's top-level test source.
    files.update(path for path in (root/'test/cases').rglob('*')
        if path.is_file() and path.suffix in ('.c','.h','.inc'))
    files={path for path in files if path.is_file()}
    files.add(Path(__file__).with_name('compiler_response.py'))
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    assert sha(build['Driver'])==build['DriverSha256'].lower()
    out.mkdir(parents=True); env=dict(os.environ)
    if os.name=='nt': shutil.copy2(native,out/'xl.dll')
    else:
        env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
        env['XL6_TCC_LIB']=str(Path(build['NativeRoot'])/'build/linux/tcc-dist')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),
        '-I'+str(root/'lib/tcc'),'-I'+str(public/'include'),
        '-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable',
        '-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers']
    links=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    links+=[str(native)]
    if os.name=='nt': links+=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
    else: flags+=['-D_GNU_SOURCE']; links+=['-Wl,-rpath,'+str(native.parent),'-pthread','-ldl','-lm']
    rows=[]; errors=[]; products=[]; report={'Complete':False,'InputsUnchanged':False,
        'Inputs':inputs,'Rows':rows,'Errors':errors,'Products':products,'Scope':__doc__,
        'ShippingLibraryAcceptance':False,'Full53Acceptance':False,'Aot':False,'Sanitized':False,
        'StandaloneImports':[]}
    report['SourceInputsMatch']=True; report['ProductionFiles']=len(compared)
    def run(name,command,rejected=False):
        result=subprocess.run(command_with_response(command,cc,out/(name+'.rsp')),cwd=root,env=env,capture_output=True,timeout=360)
        (out/(name+'.log')).write_bytes(result.stdout+result.stderr)
        rows.append({'Name':name,'Command':list(map(str,command)),'Exit':result.returncode,
            'ExpectedRejection':rejected})
        print(name+': '+str(result.returncode),flush=True)
        assert (result.returncode!=0 if rejected else result.returncode==0),\
            (result.stdout+result.stderr)[-5000:].decode(errors='replace')
        return result.stdout.decode(errors='replace')
    try:
        # Negative implementations must compile successfully and then fail the
        # same physical graph/ledger contract. A compiler error is not evidence.
        body=(root/'src/runtime/xl6_dynamic_fields.c').read_text(encoding='utf-8')
        mutations=(('leaf_trace',
            'return xrtDynamicFieldsOwnershipAdapterV1(child) && visit(child, context);',
            'return xrtDynamicFieldsOwnershipAdapterV1(child) != NULL;'),
            ('lost_drop','xrtDynamicFieldsUnref(payload);','(void)payload;'),
            ('nul_prefix','return xrtDynamicFieldsSetRef(payload, name, value);',
             'name.Size = strlen(name.Data); return xrtDynamicFieldsSetRef(payload, name, value);'))
        contract=args.native_contract.resolve() if args.native_contract else root/'test/cases/stdlib_dynamic_fields_resident_host.c'
        for name,old,new in mutations:
            assert body.count(old)==1
            source=out/('mutant_'+name+'.c'); source.write_text('#include <string.h>\n'+body.replace(old,new),encoding='utf-8')
            binary=out/('mutant_'+name+('.exe' if os.name=='nt' else ''))
            mutant_links=[path for path in links if Path(path).name!='xl6_dynamic_fields.o']
            run(name+'.mutant.compile',[cc,*flags,'-O0','-I'+str(root/'src/runtime'),
                contract,memory,source,*mutant_links,'-o',binary])
            run(name+'.mutant.reject',[binary],rejected=True)
            products.extend({'Path':str(path),'Sha256':sha(path)} for path in (source,binary))
        report['ImplementationNegativeControls']=len(mutations)
        plan=out/('object-fields-plan'+('.exe' if os.name=='nt' else ''))
        run('object-fields-plan.compile',[cc,*flags,'-O2',plan_source,*links,'-o',plan])
        run('object-fields-plan.present',[plan,root/'test/cases/cgen_object_var_field_lifetime.xl','required'])
        run('object-fields-plan.absent',[plan,absent,'absent'])
        products.append({'Path':str(plan),'Sha256':sha(plan)})
        for name in contracts:
            try:
                for opt in ('O0','O2'):
                    binary=out/(name+'.'+opt+('.exe' if os.name=='nt' else ''))
                    extra=['-DXL6_PROJECT_OPEN=1','-DXL6_FACTORY_PROFILE=0'] if name=='stdlib_persistence_project_native_host' else []
                    contract=args.native_contract.resolve() if args.native_contract and name=='stdlib_dynamic_fields_resident_host' else root/'test/cases'/(name+'.c')
                    support=[memory] if name in ('stdlib_dynamic_fields_resident_host','stdlib_object_fields_native_host') else []
                    run(name+'.'+opt+'.compile',[cc,*flags,'-'+opt,*extra,contract,*support,*links,'-o',binary])
                    run(name+'.'+opt+'.run',[binary]); products.append({'Path':str(binary),'Sha256':sha(binary)})
            except Exception as error: errors.append(name+': '+repr(error))
        for name in cases:
            try:
                source=root/'test/cases'/(name+'.xl'); generated=out/(name+'.c'); ir=out/(name+'.json')
                run(name+'.ir',[build['Driver'],source,'ir-lowered',ir])
                run(name+'.c',[build['Driver'],source,'cgen-default',generated])
                repeat=out/(name+'.repeat.c'); run(name+'.repeat',[build['Driver'],source,'cgen-default',repeat])
                assert generated.read_bytes()==repeat.read_bytes()
                text=generated.read_text(encoding='utf-8-sig')
                assert 'static void* xl6_xrt_dynamic_fields_create(' not in text
                assert 'xlir_native_handle_ops_s' not in text and 'xl6DynamicFieldsHandlePolicyV1Get' in text
                if name!='stdlib_dynamic_fields_cycle':
                    for opt in ('O0','O2'):
                        binary=out/(name+'.'+opt+('.exe' if os.name=='nt' else ''))
                        mem=out/(name+'.'+opt+'.memory.o')
                        run(name+'.'+opt+'.memory',[cc,*flags,'-'+opt,'-c',memory,'-o',mem])
                        run(name+'.'+opt+'.compile',[cc,*flags,'-'+opt,'-include',generated,host,mem,*links,'-o',binary])
                        assert 'balanced' in run(name+'.'+opt+'.run',[binary])
                        products.append({'Path':str(binary),'Sha256':sha(binary)})
                # Cycles require the real host ownership driver, not the
                # unrestricted C harness's module-init/fini-only contract.
                run(name+'.jit',[build['Driver'],source,'run',out/(name+'.jit')])
                if name!='stdlib_dynamic_fields_cycle':
                    run(name+'.module',[build['Driver'],source,'module',out/(name+'.module')])
                    binary=out/(name+'.product'+('.exe' if os.name=='nt' else ''))
                    run(name+'.aot.build',[build['Driver'],source,'exe',binary.with_suffix('') if os.name=='nt' else binary])
                    run(name+'.aot.run',[binary])
                    imports=run(name+'.imports',[objdump,'-p',binary])
                    assert not re.search(r'DLL Name:\s*x(?:l|lc)\.dll|NEEDED\s+libx(?:l|lc)\.so',imports,re.I)
                    report['StandaloneImports'].append(name)
                    products.append({'Path':str(binary),'Sha256':sha(binary)})
                products.extend({'Path':str(path),'Sha256':sha(path)} for path in (generated,repeat,ir))
            except Exception as error: errors.append(name+': '+repr(error))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        report['Aot']=len(report['StandaloneImports'])==6
        report['Complete']=not errors and report['InputsUnchanged']
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    if errors: print('\n'.join(errors),file=sys.stderr)
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
