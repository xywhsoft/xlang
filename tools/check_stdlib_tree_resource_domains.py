"""Actual generated C/JIT/AOT lifecycle and source-derived negative IR checks.
This diagnostic compiler uses explicit owned overlays against immutable source6;
passing does not mean the source product or shipping DLL contains these fixes.
"""
from __future__ import annotations
import argparse, hashlib, json, os, re, shutil, subprocess, time
from pathlib import Path
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public','overlay','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--fixture',type=Path,action='append',required=True)
    args=parser.parse_args(); root,public,overlay,out=[getattr(args,n).resolve() for n in ('root','public','overlay','out')]
    assert os.name=='nt' and not out.exists(); cc=shutil.which('gcc'); assert cc
    oldManifest=public/'build.json'; newManifest=overlay/'overlay.json'
    old=json.loads(oldManifest.read_text(encoding='utf-8-sig')); new=json.loads(newManifest.read_text(encoding='utf-8-sig'))
    assert old['SourceXrt'] and not old['RuntimeOnly'] and not new['SourceProductAcceptance']
    files={Path(__file__).resolve(),Path(cc),oldManifest,newManifest,Path(new['Driver']),root/'release/xl.dll'}
    for row in old['Inputs']+old['Headers']+old['Objects']+new['Inputs']+new['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    assert sha(new['Driver'])==new['DriverSha256'].lower()
    fixtures=[p.resolve() for p in args.fixture]
    contract=Path(__file__).resolve().parents[1]/'test/cases/xlir_resident_resource_domain_contract.c'
    files.update(fixtures+[contract,root/'test/cases/stdlib_tree_native_language_host.c',
        root/'test/support/module_product_memory.c',root/'test/support/module_product_memory.h',
        root/'test/cases/stdlib_concurrent_queue_spsc_only.xl'])
    replacements={Path(row['Path']).name for row in new['Objects']}
    objects=[row['Path'] for row in new['Objects']]+[row['Path'] for row in old['Objects']
        if Path(row['Path']).name not in replacements|{'xlir_public_source_driver.o'}]
    links=[*objects,str(root/'release/xl.dll'),'-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
    inputs=[{'Path':str(p),'Sha256':sha(p)} for p in sorted(files)]
    out.mkdir(parents=True); shutil.copy2(root/'release/xl.dll',out/'xl.dll')
    rows,proofs=[],[]; report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,
        'Full53Acceptance':False,'SourceProductAcceptance':False,'ShippingLibraryAcceptance':False,
        'WholeRuntimeInstrumented':False,'Inputs':inputs,'Rows':rows,'Proofs':proofs}
    def save(): (out/'resource.domains.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=list(map(str,command)); start=time.monotonic()
        result=subprocess.run(command,cwd=root,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode,'Seconds':time.monotonic()-start}); save()
        if result.returncode: raise RuntimeError(name+': '+data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wshadow','-DXL_STATIC_EMBED',
        '-I'+str(overlay/'overlay'),'-I.','-Irelease/include','-I'+str(overlay/'include'),
        '-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
    try:
        for level in (0,2):
            checker=out/('contract.O'+str(level)+'.exe')
            # Wshadow is required on actual generated C, not on the old
            # compiler headers (their existing C typedef names can overlap).
            run('contract.O'+str(level)+'.build',[cc,*[f for f in flags if f!='-Wshadow'],'-O'+str(level),contract,*links,'-o',checker])
            run('pruning.O'+str(level),[checker,root/'test/cases/stdlib_concurrent_queue_spsc_only.xl',0])
            for fixture in fixtures:
                text=run(fixture.stem+'.contract.O'+str(level),[checker,fixture,7 if 'queue' in fixture.stem else 0])
                assert 'output retained' in text
        for fixture in fixtures:
            case=fixture.stem; generated=out/(case+'.c'); repeat=out/(case+'.repeat.c'); ir=out/(case+'.ir.json')
            for label,target in [('emit',generated),('repeat',repeat)]: run(case+'.'+label,[new['Driver'],fixture,'cgen-default',target])
            assert generated.read_bytes()==repeat.read_bytes()
            text=generated.read_text(encoding='utf-8'); assert 'xlir_avl_' not in text and 'xl6TreeCreateTakeV1(' in text
            run(case+'.ir',[new['Driver'],fixture,'ir-lowered',ir]); data=json.loads(ir.read_text(encoding='utf-8'))
            assert any(row.get('sequenceLayout',{}).get('abiName') in ('tree-reference-v1','queue-reference-v1') for row in data['types'])
            run(case+'.jit',[new['Driver'],fixture,'run',out/(case+'.jit')])
            for level in (0,2):
                binary=out/(case+'.O'+str(level)+'.exe')
                run(case+'.O'+str(level)+'.build',[cc,*flags,'-O'+str(level),
                    '-DXLIR_GENERATED_MODULE="'+str(generated).replace('\\','/')+'"',
                    'test/cases/stdlib_tree_native_language_host.c','test/support/module_product_memory.c',*links,'-o',binary])
                assert 'lifetimes balanced' in run(case+'.O'+str(level)+'.run',[binary])
            proofs.append({'Fixture':str(fixture),'DeterministicC':True,'StrictCFlagsIncludeWshadow':True,
                'ActualJit':True,'AotLevels':[0,2],'InitMainFiniLifetimesBalanced':True})
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print(f'Resident resource domains: {len(rows)} phases, actual JIT/AOT init/main/fini balanced',flush=True)
if __name__=='__main__': main()
