"""Stack erasure, batch/equals and core dynamic methods through source C/JIT/module/AOT.
Not all dynamic Stack methods, full53 or shipping library acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys
from xlir_public_build_inputs import verify_public_build
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--try-component',action='store_true',help='Also verify native two-result try provider; not dynamic try syntax')
    a=p.parse_args(); root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve()
    assert not out.exists(); build,pinned,compared=verify_public_build(root,public)
    project=Path(__file__).resolve().parents[1]; cc=shutil.which('gcc'); nm=shutil.which('nm'); assert cc and nm
    names=('stdlib_stack_dynamic_surface','stdlib_stack_dynamic_errors','xlir_stdlib_dynamic_native.pending',
        'stdlib_stack_factory_only','stdlib_stack_values','stdlib_native_dynamic_inputs','stdlib_stack_dynamic_batch')
    fixtures=[(project if name.startswith('stdlib_stack_dynamic_') else root)/'test/cases'/(name+'.xl') for name in names]
    contract=project/'test/support/stack_erasure_contract.c'
    tryContract=project/'test/support/stack_try_component_contract.c' if a.try_component else None
    host=root/'test/cases/xlir_module_product_oom_host.c'; memory=root/'test/support/module_product_memory.c'
    native=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    pinned.update((Path(__file__).resolve(),Path(cc),Path(nm),*fixtures,contract,host,memory,native))
    if tryContract: pinned.add(tryContract)
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]; out.mkdir(parents=True)
    rows=[]; prefixes=[]; products=[]; env=dict(os.environ); suffix='.exe' if os.name=='nt' else ''
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,
        Products=products,ComparedSources=compared,StackCoreDynamicAcceptance=False,
        StackFullDynamicAcceptance=False,Full53Acceptance=False,ShippingLibraryAcceptance=False,Scope=__doc__)
    report['NativeTryProviderAcceptance']=False
    report['DynamicTrySyntaxAcceptance']=False
    if os.name=='nt': shutil.copy2(native,out/'xl.dll')
    else: env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',
        '-I'+str(root),'-I'+str(root/'release/include'),'-I'+str(public/'include'),
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers','-Wno-pointer-sign']
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    links=[row['Path'] for row in build['Objects'] if Path(row['Path']).stem not in
        ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    links += [str(native),*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt'
        else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(native.parent)])]
    def run(name,command):
        result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=300)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=result.returncode))
        print(name+': '+str(result.returncode),flush=True)
        assert result.returncode==0,(name,data[-5000:].decode(errors='replace'))
        return data.decode(errors='replace')
    try:
        binary=out/('contract'+suffix)
        run('contract.build',[cc,*flags,'-O0',contract,*links,'-o',binary])
        assert 'rejected before output' in run('contract.run',[binary,fixtures[0]])
        if tryContract:
            for opt in (0,2):
                binary=out/('try-contract.O'+str(opt)+suffix)
                run('try-contract.O'+str(opt)+'.build',[cc,*flags,'-O'+str(opt),tryContract,*links,'-o',binary])
                assert 'provider closure and current-source linkage verified' in run('try-contract.O'+str(opt)+'.run',[binary])
            report['NativeTryProviderAcceptance']=True
        objects={}
        for opt in (0,2):
            objects[opt]=out/('memory.O'+str(opt)+'.o')
            run('memory.O'+str(opt),[cc,*flags,'-O'+str(opt),'-c',memory,'-o',objects[opt]])
        for fixture in fixtures:
            name=fixture.stem; generated=out/(name+'.c'); repeat=out/(name+'.repeat.c')
            run(name+'.cgen',[build['Driver'],fixture,'cgen-default',generated])
            run(name+'.repeat',[build['Driver'],fixture,'cgen-default',repeat]); assert sha(generated)==sha(repeat)
            body=generated.read_text(encoding='utf-8')
            if name in ('stdlib_stack_dynamic_surface','stdlib_stack_dynamic_batch'): assert 'xl6StackEraseTakeV2(' in body
            if name=='stdlib_stack_factory_only': assert 'xl6StackEraseTakeV2(' not in body
            for opt in (0,2):
                binary=out/(name+'.O'+str(opt)+suffix)
                extras=['-DXL6_PRODUCT_USER_ENTRY'] if name in ('stdlib_stack_values','stdlib_stack_factory_only') else []
                run(name+'.O'+str(opt)+'.build',[cc,*flags,'-O'+str(opt),'-include',generated,*extras,
                    host,objects[opt],*links,'-o',binary])
                assert 'balanced' in run(name+'.O'+str(opt)+'.run',[binary])
                if name in ('stdlib_stack_dynamic_surface','stdlib_stack_dynamic_batch'):
                    log=run(name+'.O'+str(opt)+'.oom',[binary,'--all'])
                    match=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',log); assert match
                    runs,first=map(int,match.groups()); assert runs==first+1
                    prefixes.append(dict(Fixture=name,Lane='O'+str(opt),Runs=runs,FirstNoHit=first))
            for mode in ('run','module'):
                run(name+'.'+mode,[build['Driver'],fixture,mode,out/(name+'.'+mode+'.result')])
            target=out/(name+'.aot'); run(name+'.aot.build',[build['Driver'],fixture,'exe',target])
            run(name+'.aot.run',[Path(str(target)+'.exe') if os.name=='nt' else target])
            symbols=run(name+'.aot.symbols',[nm,'--defined-only',Path(str(target)+'.exe') if os.name=='nt' else target])
            if name=='stdlib_stack_factory_only':
                assert not any(re.search(r'\b'+symbol+r'$',symbols,re.M) for symbol in
                    ('xl6StackEraseTakeV1','xl6StackEraseTakeV2','xl6StackErasedReadV1','xl6StackErasedMethodV1','xl6StackErasedTryCallV1','xl6DynamicCollectionCallTakeV1'))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs); assert report['InputsUnchanged']
        products.extend(dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.iterdir())
            if path.is_file() and (path.suffix in ('.h','.c','.o','.exe') or os.access(path,os.X_OK)))
        report['StackCoreDynamicAcceptance']=True; report['Complete']=True
    except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
