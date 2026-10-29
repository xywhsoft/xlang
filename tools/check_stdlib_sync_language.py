"""Current-source public compiler: resident sync language contracts.
The source compiler/API/native providers are rebuilt (no overrides); the genuine
released TCC core is loaded dynamically. This is not a rebuilt shipping DLL/VFS
or complete 53-module acceptance. Native family selection is checked from IR;
standalone static-XRT/link pruning is not inferred from a shared runtime link.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','out'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); root,compiler,out=[getattr(args,name).resolve() for name in ('root','compiler','out')]
    assert not out.exists(); windows=os.name=='nt'
    project=Path(__file__).resolve().parents[1]; cc=shutil.which('gcc'); assert cc
    manifest=compiler/'build.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
    assert len(build['Objects'])==len(build['Sources'])
    source_names={Path(path).stem for path in build['Sources']}
    assert len(source_names)==len(build['Sources'])
    # Check the actual required implementation, not an incidental TU count:
    # unrelated IDE work can change that count without changing this contract.
    assert {'xl_api','xl_api_symbols','xrt_single_header_impl','xl6_ir','xl6_ir_lower',
        'xl6_ir_cgen','xl6_compile_unit','xl6_tree','xl6_tree_admission','xl6_semaphore',
        'xl6_array_semaphore_admission','xl6_sync','xl6_mutex','xl6_cond','xl6_rwlock',
        'xl6_array_sync_admission','xl6_queue_spsc_admission','xl6_queue_mpsc_admission',
        'xl6_queue_mpmc_admission'}.issubset(source_names)
    fixtures=[project/'test/cases'/name for name in ('stdlib_sync_resident.xl','stdlib_tree_sync_values.xl')]
    oom=[project/'test/cases'/('stdlib_sync_oom_'+name+'.xl') for name in ('mutex','cond','rwlock','policy')]
    held=[project/'test/cases'/('stdlib_sync_held_'+name+'.xl') for name in ('mutex','read','write','primary')]
    regressions=[project/'test/cases'/name for name in ('cgen_sync.xl','stdlib_semaphore_resident.xl',
        'stdlib_semaphore_errors.xl','stdlib_tree_semaphore_values.xl')]
    contract=project/'test/cases/xlir_sync_domain_contract.c'
    host=project/'test/cases/stdlib_semaphore_language_host.c'
    oom_host=project/'test/cases/stdlib_semaphore_language_oom_host.c'
    held_host=project/'test/cases/stdlib_sync_language_recovery_host.c'
    jit_host=project/'test/cases/stdlib_sync_language_jit_host.c'
    held_global=project/'test/cases/stdlib_sync_held_global.xl'
    memory=project/'test/support/module_product_memory.c'
    files={Path(__file__).resolve(),Path(cc),manifest,Path(build['Driver']),contract,host,oom_host,held_host,
        memory,jit_host,held_global,project/'test/support/module_product_memory.h',*fixtures,*oom,*held,*regressions}
    for row in build['Inputs']+build['Objects']+build['Headers']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    files.update(path for path in (compiler/'include').rglob('*') if path.is_file())
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(); tmp=out/'tmp'; tmp.mkdir(); env=dict(os.environ,TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp))
    if not windows: env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    rows=[]; report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,
        'LanguageCgenAcceptance':False,'SourceProductAcceptance':False,'ShippingLibraryAcceptance':False,
        'Full53Acceptance':False,'StandaloneLinkPruningAcceptance':False,'Inputs':inputs,'Rows':rows}
    def save(): (out/'sync.language.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,success=True):
        command=list(map(str,command)); result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode}); save()
        assert (result.returncode==0)==success,(name,data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wshadow','-DXL_STATIC_EMBED',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(compiler/'include')]
    # Existing generated shared declarations have unused static helpers. Keep
    # this legacy allowance explicit; all actual new native sources/hosts were
    # independently accepted without any warning exclusion in sync.native.json.
    compat=['-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
    objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
        {'xlir_public_source_driver.o','xlir_public_source_linux_driver.o'}]
    if windows:
        suffix='.exe'; shutil.copy2(root/'release/xl.dll',out/'xl.dll')
        libraries=[*objects,root/'release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
    else:
        suffix=''; flags+=['-D_GNU_SOURCE']; native=Path(build['NativeRoot'])
        libraries=[*objects,'-L'+str(native/'release'),'-lxl','-Wl,-rpath,'+str(native/'release'),
            '-Wl,--export-dynamic','-pthread','-ldl','-lm']
    try:
        for level in ('O0','O2'):
            binary=out/('contract.'+level+suffix)
            # The existing compiler type-id header has a local named `bytes`,
            # shadowing XRT's legacy typedef. Unlike native/AOT hosts, this
            # compiler-internal probe does not claim the extra shadow warning.
            contract_flags=[flag for flag in flags if flag!='-Wshadow']
            run('contract.'+level+'.build',[cc,*contract_flags,*compat,'-'+level,contract,*libraries,'-o',binary])
            for fixture,mask,recursive in [(fixtures[0],7,0),(fixtures[1],7,1),
                (oom[0],1,0),(oom[1],2,0),(oom[2],4,0),(oom[3],0,1)]:
                assert 'native components selected' in run(fixture.stem+'.'+level+'.contract',
                    [binary,fixture,mask,recursive])
            binary=out/('jit-recovery.'+level+suffix)
            run('jit-recovery.'+level+'.build',[cc,*flags,'-'+level,jit_host,memory,*libraries,'-o',binary])
            for fixture,mode,global_case in [*[(fixture,[1,2,3,1][index],0) for index,fixture in enumerate(held)],
                (held_global,1,1)]:
                assert 'lifetimes balanced' in run(fixture.stem+'.'+level+'.jit-recovery',
                    [binary,root,compiler/'include',out/(fixture.stem+'.'+level+'.temps'),fixture,mode,global_case])
        for fixture in fixtures+oom+held+regressions:
            name=fixture.stem; generated=out/(name+'.c'); repeat=out/(name+'.repeat.c')
            for label,target in (('emit',generated),('repeat',repeat)):
                run(name+'.'+label,[build['Driver'],fixture,'cgen-default',target])
            assert generated.read_bytes()==repeat.read_bytes()
            text=generated.read_text(encoding='utf-8')
            assert 'xlir_sync_handle' not in text and 'xlir_mutex_create(' not in text
            assert 'xrtMutexUnit(' not in text and 'xrtRWLockUnit(' not in text and 'xrtCondUnit(' not in text
            constructors=re.findall(r'(v\d+) = (?:xl6MutexCreateV1|xl6CondCreateV1|xl6RWLockCreateV1|xl6SequenceSyncCreateV1)\([^;]*\);\s*([^\n]+)',text)
            if fixture not in regressions: assert constructors
            for value,test in constructors: assert re.match(r'if \('+value+r' != NULL\)',test),(name,value,test)
            if fixture==fixtures[1] or fixture==oom[3]: assert 'xl6SequenceSyncCreateV1(' in text
            if fixture not in held:
                run(name+'.jit',[build['Driver'],fixture,'run',out/(name+'.jit')])
            for level in ('O0','O2'):
                binary=out/(name+'.'+level+suffix); extra=[]
                if fixture in held:
                    extra=['-DSYNC_UNLOCK_MODE='+str([1,2,3,1][held.index(fixture)])]
                    if fixture==held[3]: extra+=['-DSYNC_PRIMARY']
                if fixture.name=='cgen_sync.xl': extra+=['-DXLIR_SCRIPT_FRAGMENT']
                selected=held_host if fixture in held else oom_host if fixture in oom else host
                run(name+'.'+level+'.build',[cc,*flags,*compat,'-'+level,*extra,
                    '-DXLIR_GENERATED_MODULE="'+generated.as_posix()+'"',selected,memory,*libraries,'-o',binary])
                assert 'lifetimes balanced' in run(name+'.'+level+'.run',[binary])
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True; report['LanguageCgenAcceptance']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print('Sync language: '+str(len(rows))+' phases; actual JIT/AOT lifetimes balanced',flush=True)
if __name__=='__main__': main()
