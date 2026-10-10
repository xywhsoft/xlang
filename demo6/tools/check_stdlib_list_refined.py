"""Fresh source acceptance of refined Array/List/Dict key contracts and native
List var-method dispatch. O0/O2 actual generated C, wire JIT, native List input,
closed OOM/diagnosticless provider-failure prefixes and real provider mutation.
Not List source-factory/index/cast/algebra cutover, shipping or full53 acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--native-host',type=Path,help='Explicit test-only host override; hashed with all other gate inputs')
    a=p.parse_args(); root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve()
    assert not out.exists()
    manifest=public/'build.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
    if os.name!='nt': assert build['Complete'] and build['InputsUnchanged']
    driver=Path(build['Driver']); assert sha(driver).lower()==build['DriverSha256'].lower()
    cc=Path(shutil.which('gcc')); product=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    fixture=root/'test/cases/stdlib_list_refined_keys.xl'
    prune=root/'test/cases/stdlib_array_refined_pair_prune.xl'
    contract=root/'test/cases/xlir_list_refined_contract.c'
    native_host=a.native_host.resolve() if a.native_host else root/'test/cases/stdlib_list_refined_native_host.c'
    module_host=root/'test/cases/xlir_module_product_oom_host.c'
    memory=root/'test/support/module_product_memory.c'
    legacy_host=root/'test/cases/stdlib_dynamic_collection_contract.c'
    provider=root/'src/runtime/xl6_dynamic_collection.c'
    files={Path(__file__).resolve(),Path(sys.executable),manifest,cc,driver,product,fixture,prune,contract,native_host,module_host,
        memory,legacy_host,provider,root/'test/cases/xlir_artifact_runtime_contract.c'}
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path']).lower()==row['Sha256'].lower(),row['Path']; files.add(Path(row['Path']))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(); rows=[]; prefixes=[]; controls=[]
    report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,
        ImplementationNegativeControls=controls,NativeListSourceCutover=False,ShippingProductAcceptance=False,
        Full53Complete=False,WholeCompilerSanitized=False)
    env=dict(os.environ); suffix='.exe' if os.name=='nt' else ''
    if os.name=='nt': shutil.copy2(product,out/'xl.dll')
    else: env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),
        '-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc'),'-I'+str(public/'include'),
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    objects=[Path(row['Path']) for row in build['Objects'] if Path(row['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    links=[product]+(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else
        ['-Wl,-rpath,'+str(product.parent),'-pthread','-ldl','-lm'])
    wraps=['-Wl,--wrap='+name for name in ('xl6ListIteratorSnapshotErasedV1','xl6ListIteratorAdvanceValueV1',
        'xl6ListErasedContainsV1','xl6ListErasedReadV1','xl6ListErasedKeyV1','xl6ListErasedClearV1')]
    def run(name,args,reject=False):
        args=list(map(str,args)); started=time.monotonic()
        result=subprocess.run(args,cwd=root,env=env,capture_output=True,timeout=240)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        ok=result.returncode!=0 if reject else result.returncode==0
        rows.append(dict(Name=name,Command=args,Exit=result.returncode,Seconds=time.monotonic()-started,
            Complete=ok,ExpectedRejection=reject)); print(name+': '+str(result.returncode),flush=True)
        assert ok,(name,data[-6000:].decode(errors='replace')); return data.decode(errors='replace')
    def closed(name,log):
        matches=re.findall(r'Refined closed-batch function=(\d+) runs=(\d+) first-no-hit=(\d+)',log)
        assert [f for f,_,_ in matches]==['0','1']
        for function,runs,first in matches:
            assert int(runs)==int(first)+1
            points=re.findall(r'function='+function+r' point=(\d+) hit=(\d) balanced',log)
            assert points==[(str(i),str(int(i<int(first)))) for i in range(int(runs))]
            prefixes.append(dict(Name=name,Function=function,Runs=int(runs),FirstNoHit=int(first)))
    try:
        binary=out/('contract'+suffix); bindings=out/'bindings.h'
        run('contract.build',[cc,*flags,'-O0',contract,*objects,*links,'-o',binary])
        assert '34 malformed IR mutations' in run('contract.proof',[binary,fixture,bindings,prune])
        generated=out/'refined.c'; repeat=out/'refined.repeat.c'
        run('cgen',[driver,fixture,'cgen-default',generated]); run('repeat',[driver,fixture,'cgen-default',repeat])
        assert sha(generated)==sha(repeat)
        text=generated.read_text(encoding='utf-8-sig')
        for symbol,count in [('xl6ListValueIteratorCreateV1',1),('xl6ArrayValueIteratorCreateCheckedV1',1),
            ('xl6DictValueIteratorCreateCheckedV1',2),('xlir_xvalue_iterator_key_i64',1),
            ('xlir_xvalue_iterator_key_ordinal_i64',1),('xlir_xvalue_iterator_key_string_copy',2)]:
            assert len(re.findall(r'=\s*'+symbol+r'\(v\d+\);',text))==count,(symbol,count)
        assert 'xlir_xvalue_iterator_key_dynamic_xvalue(v' not in text
        assert 'xl6DynamicCollectionCallTakeV1(' in text and 'xl6ListCreateTakeV1(' in text
        pruned=out/'array-prune.c'; pruned_repeat=out/'array-prune.repeat.c'
        run('prune.cgen',[driver,prune,'cgen-default',pruned]); run('prune.repeat',[driver,prune,'cgen-default',pruned_repeat])
        assert sha(pruned)==sha(pruned_repeat)
        pruned_text=pruned.read_text(encoding='utf-8-sig')
        assert 'xl6ArrayValueIteratorCreateCheckedV1(' in pruned_text
        assert 'xl6ListValueIteratorCreateV1(' not in pruned_text and 'xl6_list_' not in pruned_text
        for level in (0,2):
            lane='O'+str(level); mem=out/(lane+'.memory.o')
            run(lane+'.memory',[cc,*flags,'-O'+str(level),'-c',memory,'-o',mem])
            binary=out/(lane+'.module'+suffix)
            run(lane+'.module.build',[cc,*flags,'-O'+str(level),'-include',generated,module_host,mem,*objects,*links,'-o',binary])
            assert 'balanced' in run(lane+'.module.run',[binary])
            binary=out/(lane+'.prune'+suffix)
            run(lane+'.prune.build',[cc,*flags,'-O'+str(level),'-include',pruned,module_host,mem,*objects,*links,'-o',binary])
            assert 'balanced' in run(lane+'.prune.run',[binary])
            binary=out/(lane+'.native'+suffix)
            run(lane+'.native.build',[cc,*flags,'-O'+str(level),'-include',generated,'-include',bindings,native_host,
                mem,*objects,*links,*wraps,'-o',binary])
            for mode in ('oom','scopes'):
                closed(lane+'.native.'+mode,run(lane+'.native.'+mode,[binary,*(['scopes'] if mode=='scopes' else [])]))
            assert 'balanced' in run(lane+'.native.rejects',[binary,'rejects'])
            binary=out/(lane+'.legacy'+suffix)
            run(lane+'.legacy.build',[cc,*flags,'-O'+str(level),legacy_host,mem,*objects,*links,'-o',binary])
            assert 'balanced' in run(lane+'.legacy.run',[binary])
        body=provider.read_text(encoding='utf-8'); before='int list=xl6ListErasedProbeV1(source);'
        assert body.count(before)==1
        mutant=out/'mutant-list-family-lost.c'; mutant.write_text(body.replace(before,'int list=0;'),encoding='utf-8')
        obj=out/'mutant-list-family-lost.o'; binary=out/('mutant-list-family-lost'+suffix)
        run('mutant.build',[cc,*flags,'-O0','-c',mutant,'-o',obj])
        replaced=[obj if p.name=='xl6_dynamic_collection.o' else p for p in objects]
        assert obj in replaced and len(replaced)==len(objects)
        run('mutant.link',[cc,*flags,'-O0','-include',generated,'-include',bindings,native_host,out/'O0.memory.o',
            *replaced,*links,*wraps,'-o',binary])
        run('mutant.reject',[binary,'scopes'],reject=True)
        controls.append(dict(Name='NativeListFamilyLost',Rejected=True,Source=str(mutant),Sha256=sha(mutant)))
        run('jit',[driver,fixture,'run',out/'jit.result'])
        run('prune.jit',[driver,prune,'run',out/'prune.jit.result'])
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Refined List gate: '+str(len(rows))+' phases, complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
