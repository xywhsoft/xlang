"""Source-derived independent Queue component/source/native-module pruning,
real public JIT, strict generated C and balanced AOT init/main/fini lifetimes.
Diagnostic compiler overrides are explicit. Linking every compiler provider
does not prove standalone-link minimum size or rebuilt source/shipping products.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for n in ('root','schema-root','compiler','out'): parser.add_argument('--'+n,type=Path,required=True)
    args=parser.parse_args(); root,schema,compiler,out=[getattr(args,n).resolve() for n in ('root','schema_root','compiler','out')]
    assert not out.exists(); cc=shutil.which('gcc'); assert cc
    manifest=compiler/('overlay.json' if os.name=='nt' else 'overlay.linux.json')
    build=json.loads(manifest.read_text(encoding='utf-8-sig')); assert build['Complete'] and build['InputsUnchanged']
    project=Path(__file__).resolve().parents[1]; contract=project/'test/cases/xlir_queue_family_pruning_contract.c'
    negative=project/'test/cases/xlir_resident_resource_domain_contract.c'
    fixtures=[project/'test/cases'/('stdlib_tree_'+f+'_values.xl') for f in ('spsc','mpsc','mpmc')]
    plain=root/'test/cases/stdlib_concurrent_queue_spsc_only.xl'
    files={Path(__file__).resolve(),Path(cc),manifest,contract,negative,plain,*fixtures,Path(build['Driver']),
        root/'test/cases/stdlib_tree_native_language_host.c',root/'test/support/module_product_memory.c',root/'test/support/module_product_memory.h'}
    for row in build['Inputs']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    files.update(p for p in (schema/'src').rglob('*') if p.suffix in ('.c','.h','.inc'))
    files.update(p for p in (compiler/'include').rglob('*') if p.is_file())
    inputs=[{'Path':str(p),'Sha256':sha(p)} for p in sorted(files)]
    out.mkdir(); rows=[]; report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,
        'SourceProductAcceptance':False,'ShippingLibraryAcceptance':False,'Full53Acceptance':False,
        'StandaloneLinkPruningAcceptance':False,'Inputs':inputs,'Rows':rows}
    def save(): (out/'queue.family.contract.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=list(map(str,command)); result=subprocess.run(command,cwd=root,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode}); save()
        assert not result.returncode,(name,data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(schema),'-I.','-Irelease','-Irelease/include',
        '-I'+str(compiler/'include'),'-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).name not in ('xlir_public_source_driver.o','xl6_xlc_shell.o')]
    if os.name=='nt':
        shutil.copy2(root/'release/xl.dll',out/'xl.dll'); suffix='.exe'
        links=[*objects,str(root/'release/xl.dll'),'-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
    else:
        flags+=['-D_GNU_SOURCE']; suffix=''; links=[*objects,'-L'+str(root/'release'),'-lxlc','-lxl',
            '-Wl,-rpath,'+str(root/'release'),'-Wl,--export-dynamic','-pthread','-ldl','-lm']
    try:
        for level in ('O0','O2'):
            checker=out/('contract.'+level+suffix); refusal=out/('refusal.'+level+suffix)
            run(level+'.contract.build',[cc,*flags,'-'+level,contract,*links,'-o',checker])
            run(level+'.refusal.build',[cc,*flags,'-'+level,negative,*links,'-o',refusal])
            assert 'independent components selected' in run(level+'.plain',[checker,plain,0,1])
            for f,fixture in enumerate(fixtures):
                assert 'independent components selected' in run(fixture.stem+'.'+level+'.pruning',[checker,fixture,1<<f,1<<f])
                assert 'output retained' in run(fixture.stem+'.'+level+'.refusal',[refusal,fixture,1<<f])
        for fixture in fixtures:
            generated=out/(fixture.stem+'.c'); repeat=out/(fixture.stem+'.repeat.c')
            for label,target in [('emit',generated),('repeat',repeat)]:
                cmd=[build['Driver'],fixture,'cgen-default',target] if os.name=='nt' else [build['Driver'],'cgen',fixture,'--emit',target]
                run(fixture.stem+'.'+label,cmd)
            assert generated.read_bytes()==repeat.read_bytes()
            cmd=[build['Driver'],fixture,'run',out/(fixture.stem+'.jit')] if os.name=='nt' else [build['Driver'],'run',fixture,'--inc-path',compiler/'include']
            run(fixture.stem+'.jit',cmd)
            for level in ('O0','O2'):
                binary=out/(fixture.stem+'.'+level+suffix)
                run(fixture.stem+'.'+level+'.build',[cc,*flags,'-Wshadow','-'+level,
                    '-DXLIR_GENERATED_MODULE="'+generated.as_posix()+'"','test/cases/stdlib_tree_native_language_host.c',
                    'test/support/module_product_memory.c',*links,'-o',binary])
                assert 'lifetimes balanced' in run(fixture.stem+'.'+level+'.run',[binary])
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print(f'Queue family contract: {len(rows)} phases; exact per-family plans and actual JIT/AOT lifetimes balanced',flush=True)
if __name__=='__main__': main()
