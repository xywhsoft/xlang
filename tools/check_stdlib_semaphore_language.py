"""Actual language/JIT/strict generated-C/AOT and source-derived IR contracts.
Uses a hash-verified diagnostic compiler override, not a coherent source or
shipping build. Balanced AOT init/main/fini is checked by native allocation
hooks. Does not claim standalone link-pruning or full 53-module acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','schema-root','compiler','out'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); root,schema,compiler,out=[getattr(args,name).resolve()
        for name in ('root','schema_root','compiler','out')]
    assert not out.exists(); windows=os.name=='nt'; cc=shutil.which('gcc'); assert cc
    manifest=compiler/('overlay.json' if windows else 'overlay.linux.json')
    build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and not build['SourceProductAcceptance']
    project=Path(__file__).resolve().parents[1]
    fixtures=[project/'test/cases'/name for name in ('stdlib_semaphore_resident.xl',
        'stdlib_semaphore_errors.xl','stdlib_tree_semaphore_values.xl')]
    # Existing full sync/thread and typed task fixtures must keep working.
    regressions=[root/'test/cases'/name for name in ('cgen_sync.xl','xlir_task_value_types.xl')]
    negatives=[project/'test/cases'/name for name in ('sem_resident_sync_copy.xl',
        'sem_resident_sync_array.xl','sem_resident_sync_nullable_receiver.xl')]
    contract=project/'test/cases/xlir_semaphore_domain_contract.c'
    host=project/'test/cases/stdlib_semaphore_language_host.c'
    oom_fixtures=[project/'test/cases'/('stdlib_semaphore_oom_'+name+'.xl') for name in ('create','default','stack')]
    oom_host=project/'test/cases/stdlib_semaphore_language_oom_host.c'
    oom_memory=project/'test/support/module_product_memory.c'
    files={Path(__file__).resolve(),Path(cc),manifest,Path(build['Driver']),contract,host,*fixtures,*regressions,*negatives,
        *oom_fixtures,oom_host,oom_memory,project/'test/support/module_product_memory.h',
        root/'test/support/module_product_memory.c',root/'test/support/module_product_memory.h'}
    for row in build['Inputs']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    files.update(path for path in (compiler/'include').rglob('*') if path.is_file())
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(); tmp=out/'tmp'; tmp.mkdir(); env=dict(os.environ,TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp))
    rows=[]; report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,
        'SourceProductAcceptance':False,'ShippingLibraryAcceptance':False,'Full53Acceptance':False,
        'StandaloneLinkPruningAcceptance':False,'Inputs':inputs,'Rows':rows}
    def save(): (out/'semaphore.language.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,success=True):
        command=list(map(str,command)); result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode}); save()
        assert (result.returncode==0)==success,(name,data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(schema),
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(compiler/'include')]
    compat=['-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
    objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
        {'xlir_public_source_driver.o','xl6_xlc_shell.o'}]
    if windows:
        shutil.copy2(root/'release/xl.dll',out/'xl.dll'); suffix='.exe'
        libraries=[*objects,root/'release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
    else:
        flags+=['-D_GNU_SOURCE']; suffix=''
        libraries=[*objects,'-L'+str(root/'release'),'-lxlc','-lxl','-Wl,-rpath,'+str(root/'release'),
            '-Wl,--export-dynamic','-pthread','-ldl','-lm']
    try:
        for fixture in negatives:
            generated=out/(fixture.stem+'.c'); generated.write_text('KEEP',encoding='utf-8')
            command=[build['Driver'],fixture,'cgen-default',generated] if windows else \
                [build['Driver'],'cgen',fixture,'--emit',generated]
            run(fixture.stem+'.refused',command,False)
            assert generated.read_bytes()==b'KEEP'
        for level in ('O0','O2'):
            binary=out/('contract.'+level+suffix)
            run('contract.'+level+'.build',[cc,*flags,*compat,'-'+level,contract,*libraries,'-o',binary])
            for fixture in fixtures:
                assert 'native components selected' in run(fixture.stem+'.'+level+'.contract',
                    [binary,fixture,int(fixture.name.startswith('stdlib_tree_'))])
        for fixture in fixtures+regressions+oom_fixtures:
            name=fixture.stem; generated=out/(name+'.c'); repeat=out/(name+'.repeat.c')
            for label,target in (('emit',generated),('repeat',repeat)):
                command=[build['Driver'],fixture,'cgen-default',target] if windows else \
                    [build['Driver'],'cgen',fixture,'--emit',target]
                run(name+'.'+label,command)
            assert generated.read_bytes()==repeat.read_bytes()
            text=generated.read_text(encoding='utf-8'); assert 'xlir_sem_' not in text and 'xl6SemCreateV1(' in text
            # A real constructor's required-owner contract must be reflected
            # by the CFG. The next failure test cannot depend on a diagnostic.
            constructors=re.findall(r'(v\d+) = (?:xl6SemCreate(?:Default)?V1|xl6SequenceSemCreateV1)\([^;]*\);\s*([^\n]+)',text)
            for value,test in constructors:
                assert re.match(r'if \('+value+r' != NULL\)',test),(name,value,test)
            if fixture in fixtures:
                assert 'xlir_sync_handle' not in text and 'xrtRWLockCreate(' not in text and 'xrtCondCreate(' not in text
            if fixture.name.startswith('stdlib_tree_'): assert 'xl6SequenceSemCreateV1(' in text
            command=[build['Driver'],fixture,'run',out/(name+'.jit')] if windows else \
                [build['Driver'],'run',fixture,'--inc-path',compiler/'include']
            run(name+'.jit',command)
            for level in ('O0','O2'):
                binary=out/(name+'.'+level+suffix)
                run(name+'.'+level+'.build',[cc,*flags,*compat,'-Wshadow','-'+level,
                    *(['-DXLIR_SCRIPT_FRAGMENT'] if fixture in regressions else []),
                    '-DXLIR_GENERATED_MODULE="'+generated.as_posix()+'"',oom_host if fixture in oom_fixtures else host,
                    oom_memory if fixture in oom_fixtures else root/'test/support/module_product_memory.c',
                    *libraries,'-o',binary])
                assert 'lifetimes balanced' in run(name+'.'+level+'.run',[binary])
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print('Semaphore language: '+str(len(rows))+' phases; actual JIT/AOT lifetimes balanced',flush=True)
if __name__=='__main__': main()
