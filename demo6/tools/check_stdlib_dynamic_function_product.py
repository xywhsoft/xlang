"""Contextual dynamic function products, exact count and owning source execution.
Not dynamic multi-result methods, nullable callable codecs, full53 or shipping.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys
from xlir_public_build_inputs import verify_public_build
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--methods',action='store_true',help='Also verify dynamic method vectors and native Stack try roles.')
    a=p.parse_args(); root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve()
    assert not out.exists(); build,pinned,compared=verify_public_build(root,public)
    project=Path(__file__).resolve().parents[1]; cc=shutil.which('gcc'); nm=shutil.which('nm'); assert cc and nm
    names=('stdlib_dynamic_function_product','stdlib_dynamic_function_product_errors','stdlib_dynamic_function_product_status',
        'stdlib_char_typed_only','stdlib_stack_dynamic_surface','stdlib_stack_dynamic_batch','xlir_stdlib_dynamic_native.pending')
    if a.methods:names=('stdlib_dynamic_method_products','stdlib_dynamic_method_product_errors','stdlib_dynamic_method_product_status',
        'stdlib_stack_dynamic_try.pending',*names)
    fixtures=[(project if name.startswith(('stdlib_dynamic_function_','stdlib_dynamic_method_')) else root)/'test/cases'/(name+'.xl') for name in names]
    contract=project/'test/support/dynamic_product_contract.c'; faultHost=project/'test/support/dynamic_product_status_host.c'
    methodFaultHost=project/'test/support/dynamic_method_product_status_host.c'
    host=root/'test/cases/xlir_module_product_oom_host.c'; memory=root/'test/support/module_product_memory.c'
    native=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    pinned.update((Path(__file__).resolve(),Path(cc),Path(nm),*fixtures,contract,faultHost,host,memory,native))
    if a.methods:pinned.add(methodFaultHost)
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]; out.mkdir(parents=True)
    rows=[]; prefixes=[]; products=[]; env=dict(os.environ); suffix='.exe' if os.name=='nt' else ''
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,
        Products=products,ComparedSources=compared,DynamicFunctionProductAcceptance=False,DynamicMethodProductAcceptance=False,Full53Acceptance=False,
        ShippingLibraryAcceptance=False,MethodProductsRequested=a.methods,Scope=__doc__)
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
    def run(name,command,reject=False):
        result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=300)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=result.returncode,ExpectedRejection=reject))
        print(name+': '+str(result.returncode),flush=True)
        assert result.returncode!=0 if reject else result.returncode==0,(name,data[-5000:].decode(errors='replace'))
        return data.decode(errors='replace')
    try:
        binary=out/('contract'+suffix)
        run('contract.build',[cc,*flags,'-O0',contract,*links,'-o',binary])
        assert 'rejected before output' in run('contract.run',[binary,fixtures[0]])
        objects={}
        for opt in (0,2):
            objects[opt]=out/('memory.O'+str(opt)+'.o')
            run('memory.O'+str(opt),[cc,*flags,'-O'+str(opt),'-c',memory,'-o',objects[opt]])
        for fixture in fixtures:
            name=fixture.stem; generated=out/(name+'.c'); repeat=out/(name+'.repeat.c')
            run(name+'.cgen',[build['Driver'],fixture,'cgen-default',generated])
            run(name+'.repeat',[build['Driver'],fixture,'cgen-default',repeat]); assert sha(generated)==sha(repeat)
            body=generated.read_text(encoding='utf-8')
            if name.startswith('stdlib_dynamic_function_'): assert 'xl6DynamicFunctionProductTakeV1(' in body and 'xvalue** dpo' in body
            if name.startswith('stdlib_dynamic_method_'):assert 'xl6DynamicDispatchProductTakeV1(' in body and 'xvalue** dpo' in body
            for opt in (0,2):
                binary=out/(name+'.O'+str(opt)+suffix)
                selected=methodFaultHost if name=='stdlib_dynamic_method_product_status' else faultHost if name=='stdlib_dynamic_function_product_status' else host
                extras=['-Wl,--wrap=xl6DynamicDispatchProductTakeV1'] if name=='stdlib_dynamic_method_product_status' else ['-Wl,--wrap=xl6DynamicFunctionProductTakeV1'] if name=='stdlib_dynamic_function_product_status' else []
                run(name+'.O'+str(opt)+'.build',[cc,*flags,'-O'+str(opt),'-include',generated,*extras,
                    selected,objects[opt],*links,'-o',binary])
                assert 'balanced' in run(name+'.O'+str(opt)+'.run',[binary])
                if name in ('stdlib_dynamic_function_product','stdlib_dynamic_method_products','stdlib_stack_dynamic_try.pending'):
                    log=run(name+'.O'+str(opt)+'.oom',[binary,'--all'])
                    match=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',log); assert match
                    runs,first=map(int,match.groups()); assert runs==first+1
                    prefixes.append(dict(Fixture=name,Lane='O'+str(opt),Runs=runs,FirstNoHit=first))
            for mode in ('run','module'):
                run(name+'.'+mode,[build['Driver'],fixture,mode,out/(name+'.'+mode+'.result')])
            target=out/(name+'.aot'); run(name+'.aot.build',[build['Driver'],fixture,'exe',target])
            run(name+'.aot.run',[Path(str(target)+'.exe') if os.name=='nt' else target])
            symbols=run(name+'.aot.symbols',[nm,'--defined-only',Path(str(target)+'.exe') if os.name=='nt' else target])
            if name=='stdlib_char_typed_only':
                assert not re.search(r'\bxl6DynamicFunctionProductTakeV1$',symbols,re.M)
        generated=out/'stdlib_dynamic_function_product_status.c'; original=generated.read_text(encoding='utf-8')
        status=re.findall(r'bool projection_ok_(\d+) = xl6DynamicFunctionProductTakeV1\(',original); assert len(status)==1
        mutant=out/'lost-status.c'; changed,n=re.subn(r'if \(projection_ok_'+status[0]+r'\)', 'if (true)',original)
        assert n==1; mutant.write_text(changed,encoding='utf-8'); binary=out/('lost-status'+suffix)
        run('lost-status.build',[cc,*flags,'-O0','-include',mutant,'-Wl,--wrap=xl6DynamicFunctionProductTakeV1',
            faultHost,objects[0],*links,'-o',binary])
        rejected=run('lost-status.reject',[binary],True)
        assert 'dynamic_product_status_host.c' in rejected and 'assert' in rejected.lower()
        if a.methods:
            original=(out/'stdlib_dynamic_method_product_status.c').read_text(encoding='utf-8')
            status=re.findall(r'bool projection_ok_(\d+) = xl6DynamicDispatchProductTakeV1\(',original); assert len(status)==1
            changed,n=re.subn(r'if \(projection_ok_'+status[0]+r'\)', 'if (true)',original); assert n==1
            mutant=out/'lost-method-status.c'; mutant.write_text(changed,encoding='utf-8'); binary=out/('lost-method-status'+suffix)
            run('lost-method-status.build',[cc,*flags,'-O0','-include',mutant,'-Wl,--wrap=xl6DynamicDispatchProductTakeV1',methodFaultHost,objects[0],*links,'-o',binary])
            rejected=run('lost-method-status.reject',[binary],True)
            assert 'dynamic_method_product_status_host.c' in rejected and 'assert' in rejected.lower()
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs); assert report['InputsUnchanged']
        products.extend(dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.iterdir())
            if path.is_file() and (path.suffix in ('.h','.c','.o','.exe') or os.access(path,os.X_OK)))
        report['DynamicFunctionProductAcceptance']=True; report['DynamicMethodProductAcceptance']=a.methods; report['Complete']=True
    except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
