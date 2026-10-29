"""Exact union Array/List/Dict domains, not general message/Stack acceptance.
Real original regressions and optional/nested values; strict C O0/O2, JIT/AOT,
full allocation prefixes, corrupted IR, causal wrong-size rejection and pruning.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,re,shutil,subprocess,sys,traceback
from xlir_public_build_inputs import verify_public_build
CASES=('cgen_typed_container_safe_index_objects_functions',
    'cgen_typed_container_index_assign_objects_functions','xlir_union_container_domain')
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args(); root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    cc,objdump=shutil.which('gcc'),shutil.which('objdump'); assert cc and objdump and not out.exists()
    build,pinned,compared=verify_public_build(root,public)
    driver=Path(build['Driver']); runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    memory=root/'test/support/module_product_memory.c'; host=root/'test/cases/xlir_module_product_oom_host.c'
    contract=root/'test/cases/xlir_union_container_contract.c'; sidecar=root/'test/cases/xlir_default_class_layout_support.c'
    numeric=root/'test/cases/stdlib_dynamic_numeric_prune.xl'; admission=root/'src/runtime/xl6_array_union_admission.c'
    files={Path(__file__),Path(sys.executable),Path(cc),Path(objdump),*pinned,memory,host,contract,numeric,sidecar,admission,
        root/'test/support/module_product_memory.h',runtime,*[root/'test/cases'/(n+'.xl') for n in CASES]}
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir(); rows=[]; errors=[]; cases=[]; prefixes=[]; artifacts=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Errors=errors,Cases=cases,
        Prefixes=prefixes,StandaloneImports=artifacts,SourceInputsMatch=True,ProductionFiles=len(compared),
        FocusedValidation=True,SelectedCases=list(CASES),Full53Acceptance=False,ShippingArtifact=False,Scope=__doc__)
    env=dict(os.environ)
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
        if expected=='refused': assert result.returncode!=0 and b'wrongSize' in data,(label,data[-6000:])
        else: assert result.returncode==expected,(label,data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    def execute(name):
        try:
            directory=out/name; directory.mkdir(); source=root/'test/cases'/(name+'.xl')
            extras=[] if name=='xlir_union_container_domain' else [sidecar]
            def emit(mode,target): return run(name+'.'+mode,[driver,source,mode,target,*extras])
            typed,lowered=directory/'typed.json',directory/'lowered.json'
            emit('ir-typed',typed); emit('ir-lowered',lowered)
            ir=json.loads(lowered.read_text()); code=directory/'module.c'; emit('cgen-default',code)
            repeat=directory/'repeat.c'; run(name+'.deterministic',[driver,source,'cgen-default',repeat,*extras])
            assert sha(code)==sha(repeat) and 'xl6SequenceUnionCreateV1(' in code.read_text()
            defines=['-DXL6_PRODUCT_USER_ENTRY'] if ir['moduleEntry']['kind']==1 else []
            for opt in (0,2):
                tag=name+'.O'+str(opt); mem=directory/(tag+'.memory.o'); binary=directory/(tag+'.exe')
                run(tag+'.memory',[cc,*strict,'-O'+str(opt),'-c',memory,'-o',mem]); side_objects=[]
                for i,extra in enumerate(extras):
                    obj=directory/(tag+'.sidecar'+str(i)+'.o')
                    run(tag+'.sidecar'+str(i),[cc,*strict,'-O'+str(opt),'-c',extra,'-o',obj]); side_objects.append(obj)
                run(tag+'.build',[cc,*strict,'-O'+str(opt),*defines,'-include',code,host,mem,*side_objects,*objects,*links,'-o',binary])
                assert 'balanced' in run(tag+'.baseline',[binary])
                message=run(tag+'.prefixes',[binary,'--all']); match=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',message)
                assert match and int(match[1])==int(match[2])+1 and int(match[2])>0
                prefixes.append(dict(Name=name,Optimization=opt,Count=int(match[1]),FirstNoHit=int(match[2])))
            emit('run',directory/'jit'); target=directory/'standalone'; emit('exe',target)
            executable=Path(str(target)+'.exe') if os.name=='nt' else target
            table=run(name+'.imports',[objdump,'-p',executable])
            imports=re.findall(r'(?:DLL Name:|NEEDED)\s+(\S+)',table); assert imports
            assert not any(i.lower() in ('xl.dll','xlc.dll','libxl.so','libxlc.so','xrt.dll','libxrt.so') or 'xltcc' in i.lower() for i in imports),imports
            run(name+'.standalone',[executable]); artifacts.append(dict(Path=str(executable),Sha256=sha(executable),Imports=imports))
            return dict(Name=name,Complete=True,GeneratedSha256=sha(code))
        except Exception: return dict(Name=name,Complete=False,Error=traceback.format_exc())
    try:
        for opt in (0,2):
            mem=out/('native.O'+str(opt)+'.memory.o'); binary=out/('contract.O'+str(opt)+'.exe')
            run('native.O'+str(opt)+'.memory',[cc,*flags,'-O'+str(opt),'-c',memory,'-o',mem])
            run('contract.O'+str(opt)+'.build',[cc,*flags,'-O'+str(opt),contract,mem,*objects,*links,'-o',binary])
            assert 'exact owned recipe verified' in run('contract.O'+str(opt)+'.ir',[binary,root/'test/cases/xlir_union_container_domain.xl',numeric])
            assert 'balanced' in run('contract.O'+str(opt)+'.native',[binary,'--native'])
        # Controlled bad implementation: only remove the exact byte-size check.
        # Replace one producer object and demand the original wrong-size assert.
        old=admission.read_text(); token='bytes.Size==data->size'; assert old.count(token)==1
        mutant=out/'bad-size.c'; mutant.write_text(old.replace(token,'bytes.Size>0'),encoding='utf-8')
        bad_object=out/'bad-size.o'; run('bad-size.build',[cc,*flags,'-I'+str(root/'src/runtime'),'-O2','-c',mutant,'-o',bad_object])
        selected=[r for r in build['Objects'] if Path(r['Path']).name=='xl6_array_union_admission.o']; assert len(selected)==1
        bad=out/'bad-size.exe'; bad_objects=[r for r in objects if r!=selected[0]['Path']]
        run('bad-size.link',[cc,*flags,'-O2',contract,out/'native.O2.memory.o',bad_object,*bad_objects,*links,'-o',bad])
        run('bad-size.refused',[bad,'--native'],expected='refused')
        rejected=out/'union-set-key-rejected.xl'
        rejected.write_text('union Unkeyed\n int number\n c_double real\nend\n'
            'Unkeyed item\nitem.number = 42\nset<Unkeyed> keys = set<Unkeyed>{item}\n',encoding='utf-8')
        message=run('union-set-key.refused',[driver,rejected,'ir-lowered',out/'bad-set.json'],expected=1)
        assert 'Set' in message and 'recipe' in message,message
        with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
            for case in pool.map(execute,CASES):
                cases.append(case); print(case['Name']+': '+str(case['Complete']),flush=True)
                if not case['Complete']: errors.append(case)
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        report['Complete']=not errors and report['InputsUnchanged'] and len(cases)==len(CASES)
    except Exception: errors.append(dict(ContractError=traceback.format_exc()))
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Union container contracts: '+str(len(rows))+' phases; complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
