"""Diagnostic Linux actual CLI JIT, generated C/AOT, and actual-IR refusal.
Owned object overrides against Linux5 are explicit, not rebuilt shipping proof.
The strict API source gate is not certified by this mixed-library CLI; its
missing-prototype negative control must be repeated in the final source build.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, time
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','schema-root','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--fixture',type=Path,action='append',required=True)
    args=parser.parse_args(); assert os.name!='nt'
    root,compiler,schema,out=[getattr(args,n.replace('-','_')).resolve() for n in ('root','compiler','schema-root','out')]
    assert not out.exists(); cc=shutil.which('gcc'); assert cc
    manifest=compiler/'overlay.linux.json'; build=json.loads(manifest.read_text())
    assert build['Complete'] and build['InputsUnchanged'] and not build['SourceProductAcceptance']
    files={Path(__file__).resolve(),Path(cc),manifest,Path(build['Driver'])}
    assert sha(build['Driver'])==build['DriverSha256']
    for row in build['Inputs']+build['Objects']:
        assert sha(row['Path'])==row['Sha256']; files.add(Path(row['Path']))
    fixtures=[p.resolve() for p in args.fixture]
    contract=Path(__file__).resolve().parents[1]/'test/cases/xlir_resident_resource_domain_contract.c'
    files.update(fixtures+[contract,root/'test/cases/stdlib_tree_native_language_host.c',
        root/'test/support/module_product_memory.c',root/'test/support/module_product_memory.h'])
    files.update(root/'release'/n for n in ('libxl.so','libxlc.so'))
    inputs=[{'Path':str(p),'Sha256':sha(p)} for p in sorted(files)]
    out.mkdir(parents=True); rows=[]
    report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,'SourceProductAcceptance':False,
        'Full53Acceptance':False,'ShippingLibraryAcceptance':False,'StrictApiNegativeControlAcceptance':False,
        'Inputs':inputs,'Rows':rows}
    def save(): (out/'resource.domains.linux.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=list(map(str,command)); start=time.monotonic()
        result=subprocess.run(command,cwd=root,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode,'Seconds':time.monotonic()-start}); save()
        if result.returncode: raise RuntimeError(name+': '+data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in ('xl6_xlc_shell.o','xl6_console.o')]
    links=[*objects,'-L'+str(root/'release'),'-lxlc','-lxl','-Wl,-rpath,'+str(root/'release'),
        '-Wl,--export-dynamic','-pthread','-ldl','-lm']
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wshadow','-D_GNU_SOURCE','-DXL_STATIC_EMBED',
        '-I'+str(schema),'-I.','-Irelease/include','-I'+str(compiler/'include'),
        '-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
    try:
        for level in (0,2):
            checker=out/('contract.O'+str(level))
            run('contract.O'+str(level)+'.build',[cc,*[f for f in flags if f!='-Wshadow'],'-O'+str(level),contract,*links,'-o',checker])
            for fixture in fixtures:
                assert 'output retained' in run(fixture.stem+'.contract.O'+str(level),[checker,fixture,7 if 'queue' in fixture.stem else 0])
        for fixture in fixtures:
            generated=out/(fixture.stem+'.c'); repeat=out/(fixture.stem+'.repeat.c')
            for label,target in [('emit',generated),('repeat',repeat)]:
                run(fixture.stem+'.'+label,[build['Driver'],'cgen',fixture,'--emit',target])
            assert generated.read_bytes()==repeat.read_bytes()
            assert 'xlir_avl_' not in generated.read_text() and 'extern xvalue* xl6SequenceTreeCreateTakeV1(' in generated.read_text()
            run(fixture.stem+'.jit',[build['Driver'],'run',fixture,'--inc-path',compiler/'include'])
            for level in (0,2):
                binary=out/(fixture.stem+'.O'+str(level))
                run(fixture.stem+'.O'+str(level)+'.build',[cc,*flags,'-O'+str(level),
                    '-DXLIR_GENERATED_MODULE="'+str(generated)+'"','test/cases/stdlib_tree_native_language_host.c',
                    'test/support/module_product_memory.c',*links,'-o',binary])
                assert 'lifetimes balanced' in run(fixture.stem+'.O'+str(level)+'.run',[binary])
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print(f'Linux diagnostic resource domains: {len(rows)} phases; actual JIT/AOT init/main/fini balanced',flush=True)
if __name__=='__main__': main()
