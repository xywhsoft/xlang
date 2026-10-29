"""Direct native method envelope: sealed physical fields, closed signed
defaults, ordered projection, independent outcomes and owned-result rollback.
Not all Native codecs, erased Stack dispatch, full53 or shipping acceptance.
"""
import argparse,hashlib,json,os,re,shutil,subprocess,sys
from pathlib import Path
from xlir_public_build_inputs import verify_public_build
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args();root=a.root.resolve();public=a.public_directory.resolve();out=a.out.resolve()
    assert not out.exists();build,pinned,compared=verify_public_build(root,public)
    project=Path(__file__).resolve().parents[1];cc=shutil.which('gcc');assert cc
    names=('stdlib_native_dynamic_surface','stdlib_native_dynamic_inputs','stdlib_native_dynamic_oom',
        'cgen_dynamic_signed_defaults','cgen_dynamic_method_value_domains','cgen_dynamic_method_arguments',
        'cgen_dynamic_method_inheritance','cgen_dynamic_method_bound_lifetime','cgen_generic_dynamic_method')
    fixtures=[(project if name.startswith('stdlib_native_dynamic_') or name=='cgen_dynamic_signed_defaults' else root)/'test/cases'/(name+'.xl') for name in names]
    faultFixture=project/'test/cases/stdlib_native_dynamic_faults.xl'
    contract=project/'test/support/native_dynamic_contract.c';faultHost=project/'test/support/native_dynamic_fault_host.c'
    host=root/'test/cases/xlir_module_product_oom_host.c';memory=root/'test/support/module_product_memory.c'
    native=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    pinned.update((Path(__file__).resolve(),Path(cc),*fixtures,faultFixture,contract,faultHost,host,memory,native))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)];out.mkdir(parents=True)
    rows=[];prefixes=[];products=[];controls=[];env=dict(os.environ);suffix='.exe' if os.name=='nt' else ''
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,
        Products=products,ComparedSources=compared,ImplementationNegativeControls=controls,DirectNativeDynamicAcceptance=False,
        NativeDynamicAcceptance=False,Full53Acceptance=False,ShippingLibraryAcceptance=False,Scope=__doc__)
    if os.name=='nt':shutil.copy2(native,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',
        '-I'+str(root),'-I'+str(root/'release/include'),'-I'+str(public/'include'),
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers','-Wno-pointer-sign']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    links=[row['Path'] for row in build['Objects'] if Path(row['Path']).stem not in
        ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    links += [str(native),*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt'
        else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(native.parent)])]
    def run(name,command,reject=False):
        result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=300)
        data=result.stdout+result.stderr;(out/(name+'.log')).write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=result.returncode,ExpectedRejection=reject))
        print(name+': '+str(result.returncode),flush=True)
        assert result.returncode!=0 if reject else result.returncode==0,(name,data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    try:
        bindings=out/'faults-bindings.h';binary=out/('contract'+suffix)
        run('contract.build',[cc,*flags,'-O0',contract,*links,'-o',binary])
        assert 'rejected before output' in run('contract.run',[binary,faultFixture,bindings])
        memoryObjects={}
        for opt in (0,2):
            memoryObjects[opt]=out/('memory.O'+str(opt)+'.o')
            run('memory.O'+str(opt),[cc,*flags,'-O'+str(opt),'-c',memory,'-o',memoryObjects[opt]])
        for fixture in [*fixtures,faultFixture]:
            name=fixture.stem;generated=out/(name+'.c');repeat=out/(name+'.repeat.c')
            run(name+'.cgen',[build['Driver'],fixture,'cgen-default',generated])
            run(name+'.repeat',[build['Driver'],fixture,'cgen-default',repeat]);assert sha(generated)==sha(repeat)
            for opt in (0,2):
                binary=out/(name+'.O'+str(opt)+suffix);extras=[];selected=host
                if fixture==faultFixture:
                    selected=faultHost;extras=['-include',bindings,*['-Wl,--wrap='+symbol for symbol in
                        ('xrtValueGetHandle','xl6ForeignCallBegin','xl6BufferLengthV1','xl6BufferTextV1','xl6BufferAppendBufferV1')]]
                elif name in ('cgen_dynamic_method_value_domains','cgen_dynamic_method_arguments',
                    'cgen_dynamic_method_inheritance','cgen_dynamic_method_bound_lifetime','cgen_generic_dynamic_method'):
                    extras=['-DXL6_PRODUCT_USER_ENTRY']
                run(name+'.O'+str(opt)+'.build',[cc,*flags,'-O'+str(opt),'-include',generated,*extras,
                    selected,memoryObjects[opt],*links,'-o',binary])
                log=run(name+'.O'+str(opt)+'.run',[binary]);assert 'balanced' in log
                if fixture.stem=='stdlib_native_dynamic_oom':
                    log=run(name+'.O'+str(opt)+'.oom',[binary,'--all'])
                    match=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',log);assert match
                    runs,first=map(int,match.groups());assert runs==first+1
                    prefixes.append(dict(Lane='O'+str(opt),Runs=runs,FirstNoHit=first))
            if fixture==faultFixture:continue
            for mode in ('run','module'):
                run(name+'.'+mode,[build['Driver'],fixture,mode,out/(name+'.'+mode+'.result')])
            target=out/(name+'.aot');run(name+'.aot.build',[build['Driver'],fixture,'exe',target])
            run(name+'.aot.run',[Path(str(target)+'.exe') if os.name=='nt' else target])
        original=(out/(faultFixture.stem+'.c')).read_text(encoding='utf-8')
        assert '!nativeScope.failed && xrtGetError() == NULL' in original
        mutant=out/'lost-null-diagnostic.c'
        mutant.write_text(original.replace('!nativeScope.failed && xrtGetError() == NULL','xrtGetError() == NULL'),encoding='utf-8')
        binary=out/('lost-null-diagnostic'+suffix)
        wraps=['-Wl,--wrap='+symbol for symbol in
            ('xrtValueGetHandle','xl6ForeignCallBegin','xl6BufferLengthV1','xl6BufferTextV1','xl6BufferAppendBufferV1')]
        run('lost-null-diagnostic.build',[cc,*flags,'-O0','-include',mutant,'-include',bindings,
            faultHost,memoryObjects[0],*links,*wraps,'-o',binary])
        assert 'budgetHit' in run('lost-null-diagnostic.reject',[binary],True)
        controls.append('lost-null-diagnostic')
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);assert report['InputsUnchanged']
        products.extend(dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.iterdir())
            if path.is_file() and (path.suffix in ('.h','.c','.o','.exe') or os.access(path,os.X_OK)))
        report['DirectNativeDynamicAcceptance']=True;report['Complete']=True
    except Exception as error:report['Error']=repr(error);print(error,file=sys.stderr)
    finally:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
