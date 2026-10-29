"""Closed dynamic method argument domains: actual source compiler, strict C,
JIT/module/AOT, sealed-IR mutation rejection and positive-program OOM prefixes.
This is not Native method projection or full53/shipping product acceptance.
"""
import argparse,hashlib,json,os,re,shutil,subprocess,sys
from pathlib import Path
from xlir_public_build_inputs import verify_public_build

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();public=a.public_directory.resolve();out=a.out.resolve()
    assert not out.exists();build,pinned,compared=verify_public_build(root,public)
    cc=shutil.which('gcc');assert cc
    project=Path(__file__).resolve().parents[1]
    contract=project/'test/support/dynamic_method_value_domains_contract.c'
    fixtures=[root/'test/cases'/name for name in (
        'cgen_dynamic_method_value_domains.xl','cgen_dynamic_method_value_domains_oom.xl',
        'cgen_dynamic_method_overload.xl','cgen_dynamic_method_arguments.xl',
        'cgen_dynamic_method_inheritance.xl','cgen_dynamic_method_bound_lifetime.xl',
        'cgen_generic_dynamic_method.xl')]
    noDomains=project/'test/cases/cgen_dynamic_method_no_domains.xl'
    fixtures.append(noDomains)
    native=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    host=root/'test/cases/xlir_module_product_oom_host.c';memory=root/'test/support/module_product_memory.c'
    pinned.update((Path(__file__).resolve(),contract,native,host,memory,*fixtures,Path(cc)))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir(parents=True);env=dict(os.environ);rows=[];products=[];prefixes=[];controls=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,
        ClosedPrefixes=prefixes,ImplementationNegativeControls=controls,ComparedSources=compared,
        NativeDynamicAcceptance=False,Full53Acceptance=False,ShippingLibraryAcceptance=False,Scope=__doc__)
    if os.name=='nt':shutil.copy2(native,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    suffix='.exe' if os.name=='nt' else ''
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',
        '-I'+str(root),'-I'+str(root/'release/include'),'-I'+str(public/'include'),
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers','-Wno-pointer-sign']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    links=[r['Path'] for r in build['Objects'] if Path(r['Path']).stem not in
        ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    links+=[str(native),*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt'
        else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(native.parent)])]
    def run(name,command,reject=False):
        result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=300)
        data=result.stdout+result.stderr;(out/(name+'.log')).write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=result.returncode,ExpectedRejection=reject))
        print(name+': '+str(result.returncode),flush=True)
        assert result.returncode!=0 if reject else result.returncode==0,(name,data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    memoryObjects={}
    def compile_program(name,source,opt):
        if opt not in memoryObjects:
            memoryObjects[opt]=out/('memory.O'+str(opt)+'.o')
            run('memory.O'+str(opt)+'.build',[cc,*flags,'-O'+str(opt),'-c',memory,'-o',memoryObjects[opt]])
        binary=out/(name+'.O'+str(opt)+suffix)
        run(name+'.O'+str(opt)+'.build',[cc,*flags,'-O'+str(opt),'-DXL6_PRODUCT_USER_ENTRY',
            '-include',source,host,memoryObjects[opt],*links,'-o',binary]);return binary
    try:
        binary=out/('contract'+suffix)
        run('contract.build',[cc,*flags,'-O0',contract,*links,'-o',binary])
        contractLog=run('contract.run',[binary,fixtures[0]])
        assert 'rejected before output' in contractLog
        admission=re.search(r'bytes admission rule=(\d+)',contractLog);assert admission
        assert 'rejected before output' in run('contract.pruning',[binary,noDomains,'--no-domains'])
        for fixture in fixtures:
            name=fixture.stem;generated=out/(name+'.c');repeat=out/(name+'.repeat.c')
            run(name+'.cgen',[build['Driver'],fixture,'cgen-default',generated])
            run(name+'.repeat',[build['Driver'],fixture,'cgen-default',repeat]);assert sha(generated)==sha(repeat)
            if fixture==noDomains:
                text=generated.read_text(encoding='utf-8')
                assert 'xl6ValueAdmitsV1' not in text and 'xl6ValueAdmitsNominalV1' not in text
            for opt in (0,2):
                binary=compile_program(name,generated,opt)
                assert 'balanced' in run(name+'.O'+str(opt)+'.run',[binary])
                if name.endswith('_oom'):
                    log=run(name+'.O'+str(opt)+'.oom',[binary,'--all'])
                    match=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',log);assert match
                    runs,first=map(int,match.groups());assert runs==first+1
                    prefixes.append(dict(Lane='O'+str(opt),Runs=runs,FirstNoHit=first))
            for mode in ('run','module'):
                run(name+'.'+mode,[build['Driver'],fixture,mode,out/(name+'.'+mode+'.result')])
            target=out/(name+'.aot')
            run(name+'.aot.build',[build['Driver'],fixture,'exe',target])
            run(name+'.aot.run',[Path(str(target)+'.exe') if os.name=='nt' else target])
        original=(out/(fixtures[0].stem+'.c')).read_text(encoding='utf-8')
        for name,pattern in (
            ('bytes-any',r'xl6ValueAdmitsV1\(bound0, '+admission.group(1)+r', false, 0x[0-9A-F]+ULL\)'),
            ('nominal-any',r'xl6ValueAdmitsNominalV1\(bound0, &xlir_nominal_set_\d+, false\)'),
            ('container-any',r'xrtValueTypeId\(bound0\) == 0x[0-9A-F]+ULL')):
            mutant,count=re.subn(pattern,'true',original);assert count>=2
            source=out/(name+'.c');source.write_text(mutant,encoding='utf-8')
            binary=compile_program(name,source,0)
            run(name+'.reject',[binary],True);controls.append(dict(Name=name,Replacements=count))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);assert report['InputsUnchanged']
        products.extend(dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.iterdir())
            if path.is_file() and (path.suffix in ('.c','.o','.exe') or os.access(path,os.X_OK)))
        report['Complete']=True
    except Exception as error:report['Error']=repr(error);print(error,file=sys.stderr)
    finally:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
