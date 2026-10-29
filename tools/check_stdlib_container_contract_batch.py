"""Implicit collection vocabulary and supported container regression contracts.
This is a declared focused batch, not all container families or full53. Source
discovery's unresolved union routes remain separate work.
Strict C O0/O2, real JIT/AOT, independent imports, exact compiled IR vocabulary,
full synchronous module allocation prefixes and unused-producer pruning.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,re,shutil,subprocess,sys,traceback
from xlir_public_build_inputs import verify_public_build
CASES=('xlir_dynamic_set_vocabulary','cgen_dynamic_container_methods','cgen_dynamic_method_arguments',
    'xlir_erased_collection_members','cgen_container_ops','xlir_erased_collection_member_lifetime',
    'xlir_typed_dict_scalar_set','cgen_dynamic_container_reference','xlir_dict_named_route')
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args(); root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    cc,objdump=shutil.which('gcc'),shutil.which('objdump'); assert cc and objdump and not out.exists()
    build,pinned,compared=verify_public_build(root,public)
    driver=Path(build['Driver']); runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    memory=root/'test/support/module_product_memory.c'; host=root/'test/cases/xlir_module_product_oom_host.c'
    contract=root/'test/cases/xlir_implicit_collection_vocabulary_contract.c'
    named=root/'test/cases/xlir_dict_named_route_contract.c'
    named_prune=root/'test/cases/xlir_dict_named_prune.xl'
    class_prune=root/'test/cases/xlir_class_named_prune.xl'
    numeric=root/'test/cases/stdlib_dynamic_numeric_prune.xl'
    sidecar=root/'test/cases/xlir_default_class_layout_support.c'; weak=root/'test/cases/xlir_json_stream_lifetime_host.c'
    files={Path(__file__),Path(sys.executable),Path(cc),Path(objdump),*pinned,memory,host,contract,named,named_prune,class_prune,numeric,sidecar,weak,
        root/'test/support/module_product_memory.h',runtime,*[root/'test/cases'/(n+'.xl') for n in CASES]}
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir(); rows=[]; errors=[]; cases=[]; prefixes=[]; artifacts=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Errors=errors,Cases=cases,
        Prefixes=prefixes,StandaloneImports=artifacts,SourceInputsMatch=True,ProductionFiles=len(compared),
        FocusedValidation=True,SelectedCases=list(CASES),Full53Acceptance=False,ShippingArtifact=False,Scope=__doc__)
    env=dict(os.environ); scratch=out/'files'; scratch.mkdir(); env['XL6_PERSIST_FILE_ROOT']=str(scratch)
    if os.name=='nt':
        shutil.copy2(runtime,out/'xl.dll'); env['PATH']=str(out)+os.pathsep+env.get('PATH','')
    else:
        env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']; env['XL6_TCC_LIB']=str(Path(build['NativeRoot'])/'build/linux/tcc-dist')
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    links=[str(runtime)]+(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else
        ['-Wl,-rpath,'+str(runtime.parent),'-pthread','-ldl','-lm'])
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc'),'-I'+str(public/'include')]
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    strict=flags+['-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
    def run(label,cmd,expected=0):
        result=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=600)
        data=result.stdout+result.stderr; log=out/(label+'.log'); log.write_bytes(data)
        rows.append(dict(Name=label,Command=list(map(str,cmd)),Exit=result.returncode,ExpectedExit=expected,
            Log=str(log),LogSha256=sha(log)))
        assert result.returncode==expected,(label,data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    def execute(name):
        try:
            directory=out/name; directory.mkdir(); source=root/'test/cases'/(name+'.xl')
            extras=[weak] if name.endswith('member_lifetime') else [sidecar] if name=='xlir_typed_dict_scalar_set' else []
            def emit(mode,target): return run(name+'.'+mode,[driver,source,mode,target,*extras])
            typed,lowered=directory/'typed.json',directory/'lowered.json'
            emit('ir-typed',typed); emit('ir-lowered',lowered)
            ir=json.loads(lowered.read_text()); code=directory/'module.c'; emit('cgen-default',code)
            repeat=directory/'repeat.c'; run(name+'.deterministic',[driver,source,'cgen-default',repeat,*extras])
            assert sha(code)==sha(repeat)
            defines=['-DXL6_PRODUCT_USER_ENTRY'] if ir['moduleEntry']['kind']==1 else []
            for opt in (0,2):
                tag=name+'.O'+str(opt); mem=directory/(tag+'.memory.o'); binary=directory/(tag+'.exe')
                run(tag+'.memory',[cc,*strict,'-O'+str(opt),'-c',memory,'-o',mem])
                side_objects=[]
                for i,extra in enumerate(extras):
                    obj=directory/(tag+'.sidecar'+str(i)+'.o')
                    run(tag+'.sidecar'+str(i),[cc,*strict,'-O'+str(opt),'-c',extra,'-o',obj]); side_objects.append(obj)
                run(tag+'.build',[cc,*strict,'-O'+str(opt),*defines,'-include',code,host,mem,*side_objects,*objects,*links,'-o',binary])
                assert 'balanced' in run(tag+'.baseline',[binary])
                message=run(tag+'.prefixes',[binary,'--all']); match=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',message)
                assert match and int(match[1])==int(match[2])+1 and int(match[2])>0
                prefixes.append(dict(Name=name,Optimization=opt,Count=int(match[1]),FirstNoHit=int(match[2])))
            emit('run',directory/'jit')
            target=directory/'standalone'; emit('exe',target)
            executable=Path(str(target)+'.exe') if os.name=='nt' else target
            table=run(name+'.imports',[objdump,'-p',executable])
            imports=re.findall(r'(?:DLL Name:|NEEDED)\s+(\S+)',table); assert imports
            assert not any(i.lower() in ('xl.dll','xlc.dll','libxl.so','libxlc.so','xrt.dll','libxrt.so') or
                'xltcc' in i.lower() for i in imports),imports
            run(name+'.standalone',[executable]); artifacts.append(dict(Path=str(executable),Sha256=sha(executable),Imports=imports))
            return dict(Name=name,Complete=True,GeneratedSha256=sha(code))
        except Exception: return dict(Name=name,Complete=False,Error=traceback.format_exc())
    try:
        for opt in (0,2):
            binary=out/('vocabulary.O'+str(opt)+'.exe')
            run('vocabulary.O'+str(opt)+'.build',[cc,*flags,'-O'+str(opt),contract,*objects,*links,'-o',binary])
            assert 'six corruptions refused and unused Set pruned' in run('vocabulary.O'+str(opt)+'.run',
                [binary,root/'test/cases/xlir_dynamic_set_vocabulary.xl',numeric])
            binary=out/('named.O'+str(opt)+'.exe')
            run('named.O'+str(opt)+'.build',[cc,*flags,'-O'+str(opt),named,*objects,*links,'-o',binary])
            assert 'owned ABI and class/numeric pruning verified' in run('named.O'+str(opt)+'.run',
                [binary,root/'test/cases/xlir_dict_named_route.xl',named_prune,class_prune,numeric])
        with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
            for case in pool.map(execute,CASES):
                cases.append(case); print(case['Name']+': '+str(case['Complete']),flush=True)
                if not case['Complete']: errors.append(case)
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        report['Complete']=not errors and report['InputsUnchanged'] and len(cases)==len(CASES)
    except Exception: errors.append(dict(ContractError=traceback.format_exc()))
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Focused container contracts: '+str(len(rows))+' phases; complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
