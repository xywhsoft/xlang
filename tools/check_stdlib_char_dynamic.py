"""Checked dynamic char construction/assertion, status ABI and source execution.
Not all dynamic casts, nullable casts, full53 or shipping acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys
from xlir_public_build_inputs import verify_public_build
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args(); root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve()
    assert not out.exists(); build,pinned,compared=verify_public_build(root,public)
    project=Path(__file__).resolve().parents[1]; cc=shutil.which('gcc'); nm=shutil.which('nm'); assert cc and nm
    names=('stdlib_char_dynamic_checked','stdlib_char_dynamic_status','stdlib_char_typed_only','char_bytes_literals',
        'stdlib_scalar_projection_char','stdlib_nullable_projection_char','xlir_intrinsic_trivial_integer_convert',
        'stdlib_stack_dynamic_surface','xlir_stdlib_dynamic_native.pending')
    fixtures=[(project if name.startswith('stdlib_char_') else root)/'test/cases'/(name+'.xl') for name in names]
    contract=project/'test/support/char_dynamic_contract.c'; faultHost=project/'test/support/char_dynamic_status_host.c'
    host=root/'test/cases/xlir_module_product_oom_host.c'; memory=root/'test/support/module_product_memory.c'
    native=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    pinned.update((Path(__file__).resolve(),Path(cc),Path(nm),*fixtures,contract,faultHost,host,memory,native))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]; out.mkdir(parents=True)
    rows=[]; prefixes=[]; products=[]; env=dict(os.environ); suffix='.exe' if os.name=='nt' else ''
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,
        Products=products,ComparedSources=compared,DynamicCharAcceptance=False,Full53Acceptance=False,
        ShippingLibraryAcceptance=False,Scope=__doc__)
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
            if name.startswith('stdlib_char_dynamic_'): assert 'xl6CharValueCheckedV1(' in body
            for opt in (0,2):
                binary=out/(name+'.O'+str(opt)+suffix)
                selected=faultHost if name=='stdlib_char_dynamic_status' else host
                extras=['-Wl,--wrap=xl6CharValueCheckedV1'] if name=='stdlib_char_dynamic_status' else []
                if name in ('stdlib_scalar_projection_char','stdlib_nullable_projection_char','xlir_intrinsic_trivial_integer_convert'):
                    extras+=['-DXL6_PRODUCT_USER_ENTRY']
                run(name+'.O'+str(opt)+'.build',[cc,*flags,'-O'+str(opt),'-include',generated,*extras,
                    selected,objects[opt],*links,'-o',binary])
                assert 'balanced' in run(name+'.O'+str(opt)+'.run',[binary])
                if name=='stdlib_char_dynamic_checked':
                    log=run(name+'.O'+str(opt)+'.oom',[binary,'--all'])
                    match=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',log); assert match
                    runs,first=map(int,match.groups()); assert runs==first+1
                    prefixes.append(dict(Lane='O'+str(opt),Runs=runs,FirstNoHit=first))
            for mode in ('run','module'):
                run(name+'.'+mode,[build['Driver'],fixture,mode,out/(name+'.'+mode+'.result')])
            target=out/(name+'.aot'); run(name+'.aot.build',[build['Driver'],fixture,'exe',target])
            run(name+'.aot.run',[Path(str(target)+'.exe') if os.name=='nt' else target])
            symbols=run(name+'.aot.symbols',[nm,'--defined-only',Path(str(target)+'.exe') if os.name=='nt' else target])
            if name=='stdlib_char_typed_only':
                assert not re.search(r'\bxl6CharValueCheckedV1$',symbols,re.M)
        generated=out/'stdlib_char_dynamic_status.c'; original=generated.read_text(encoding='utf-8')
        mutant=out/'lost-status.c'; changed,n=re.subn(r'if \(projection_ok_(\d+)\)', 'if (true)',original)
        assert n==1; mutant.write_text(changed,encoding='utf-8'); binary=out/('lost-status'+suffix)
        run('lost-status.build',[cc,*flags,'-O0','-include',mutant,'-Wl,--wrap=xl6CharValueCheckedV1',
            faultHost,objects[0],*links,'-o',binary])
        rejected=run('lost-status.reject',[binary],True)
        assert 'char_dynamic_status_host.c' in rejected and 'assert' in rejected.lower()
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs); assert report['InputsUnchanged']
        products.extend(dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.iterdir())
            if path.is_file() and (path.suffix in ('.h','.c','.o','.exe') or os.access(path,os.X_OK)))
        report['DynamicCharAcceptance']=True; report['Complete']=True
    except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
