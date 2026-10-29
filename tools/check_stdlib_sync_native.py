"""Resident sync owner/retirement native diagnostics only.
Actual native refused Unit, dead language shells, owned recovery/retry and
activity/OOM are exercised. New providers/hosts compile without exclusions;
sanitizers do not instrument the old runtime. Native family objects are linked
and audited independently; generated-C/static-XRT pruning, Cgen/source/shipping
and full53 acceptance remain explicitly unproven.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','schema-root','compiler','out'): parser.add_argument('--'+name,type=Path,required=True)
    instrumentation=parser.add_mutually_exclusive_group()
    instrumentation.add_argument('--sanitize',action='store_true')
    instrumentation.add_argument('--race',action='store_true')
    args=parser.parse_args(); root,schema,compiler,out=[getattr(args,name).resolve() for name in ('root','schema_root','compiler','out')]
    assert not out.exists() and (not (args.sanitize or args.race) or os.name!='nt')
    project=Path(__file__).resolve().parents[1]; cc=shutil.which('gcc'); nm=shutil.which('nm'); assert cc and nm
    manifest=compiler/('overlay.json' if os.name=='nt' else 'overlay.linux.json')
    prior=json.loads(manifest.read_text(encoding='utf-8-sig')); assert prior['Complete'] and prior['InputsUnchanged']
    providers=['xl6_sync','xl6_mutex','xl6_cond','xl6_rwlock','xl6_array_sync_admission']
    sources=[project/'src/runtime'/(name+'.c') for name in providers]
    host=project/'test/cases/stdlib_sync_native_host.c'
    family_host=project/'test/cases/stdlib_sync_native_family_host.c'
    inputs={Path(__file__).resolve(),manifest,Path(cc),Path(nm),*sources,host,family_host,project/'src/runtime/xl6_sync.h',
        project/'src/runtime/xl6_sync_internal.h',project/'src/runtime/xl6_mutex_internal.h',
        project/'src/runtime/xl6_sync_admission.h',project/'src/runtime/xl6_sequence.h',
        project/'src/runtime/xl6_array_admission.h',project/'src/runtime/xl6_array.h',
        project/'src/runtime/xl6_ownership_plan.h',project/'release/include/xl_sync_retirement.h',
        project/'release/include/xl.h',project/'src/api/xl_foreign_error.h',
        root/'test/support/module_product_memory.c',root/'test/support/module_product_memory.h'}
    # This native probe needs no compiler, IR, API or unrelated providers.
    # All kernels/value ABI and the neutral error boundary come from the real
    # runtime library. Some older islands override that boundary separately.
    selected=[row for row in prior['Objects'] if Path(row['Path']).name=='xl_foreign_error.o']
    assert len(selected)<=1
    for row in selected:
        assert sha(row['Path'])==row['Sha256'].lower(); inputs.add(Path(row['Path']))
    inputs.update(path for path in (root/'release/include').glob('*.h') if path.is_file())
    inputs.update(path for path in (root/'release').iterdir() if path.is_file() and
        (path.name in ('xl.dll','libxl.so','libxlc.so') or path.name.startswith(('libxl.so.','libxlc.so.'))))
    inputs.update(project/'src/runtime'/name for name in ('xl6_code_lease.h','xl6_code_owner.h'))
    # The source includes the project's real current XRT/xruntime declarations.
    inputs.update(path for folder in ('lib/xrt','lib/xruntime') for path in (project/folder).glob('*.h'))
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(inputs)]
    out.mkdir(); tmp=out/'tmp'; tmp.mkdir(); env=dict(os.environ,TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp))
    rows=[]; report={'Scope':__doc__,'Complete':False,'InputsUnchanged':False,'Inputs':inputs,'Rows':rows,
        'LanguageCgenAcceptance':False,'SourceProductAcceptance':False,'ShippingLibraryAcceptance':False,
        'StandaloneLinkPruningAcceptance':False,'Full53Acceptance':False,'ProvidersInstrumented':args.sanitize or args.race,
        'ProviderInstrumentation':'thread' if args.race else 'address,undefined' if args.sanitize else None,
        'RaceStressOnly':args.race,
        'NativeFamilyLinkPruningAcceptance':False,
        'WholeRuntimeInstrumented':False}
    def save(): (out/'sync.native.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=list(map(str,command)); result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode}); save()
        assert result.returncode==0,(name,data[-6000:].decode(errors='replace')); return data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wshadow','-DXL_STATIC_EMBED',
        '-I'+str(project),'-I'+str(project/'release/include'),'-I'+str(schema),
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
    if args.sanitize: flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
    if args.race: flags+=['-g','-fsanitize=thread','-fno-omit-frame-pointer','-fno-pie','-no-pie']
    objects=[row['Path'] for row in selected]
    if os.name=='nt':
        shutil.copy2(root/'release/xl.dll',out/'xl.dll'); suffix='.exe'
        libraries=[*objects,root/'release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
    else:
        flags+=['-D_GNU_SOURCE']; suffix=''
        libraries=[*objects,'-L'+str(root/'release'),'-lxl','-Wl,-rpath,'+str(root/'release'),'-pthread','-ldl','-lm']
    try:
        for level in ('O0','O2'):
            selected=[]; binary=out/('native.'+level+suffix)
            for name,source in zip(providers,sources):
                obj=out/(name+'.'+level+'.o'); selected.append(obj)
                run(level+'.'+name,[cc,*flags,'-'+level,'-c',source,'-o',obj])
            common_undefined=run(level+'.common.undefined',[nm,'-u',selected[0]])
            assert not any(name in common_undefined for name in ('xrtMutex','xrtCond','xrtRWLock','xl6Mutex','xl6Cond','xl6RWLock'))
            run(level+'.host',[cc,*flags,'-'+level,host,root/'test/support/module_product_memory.c',*selected,*libraries,'-o',binary])
            assert 'OOM balanced' in run(level+'.run',[binary,*(['--race-stress-only'] if args.race else [])])
            for index,family in enumerate(('Mutex','Cond','RWLock'),1):
                single=out/(family+'.'+level+suffix)
                run(level+'.'+family+'.link',[cc,*flags,'-'+level,'-DSYNC_FAMILY='+str(index),family_host,
                    root/'test/support/module_product_memory.c',selected[0],selected[index],*libraries,'-o',single])
                assert 'OOM balanced' in run(level+'.'+family+'.run',[single])
                symbols=run(level+'.'+family+'.symbols',[nm,'--defined-only',single])
                assert 'xl6'+family+'CreateV1' in symbols
                assert not any('xl6'+other+'CreateV1' in symbols for other in ('Mutex','Cond','RWLock') if other!=family)
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
        report['NativeFamilyLinkPruningAcceptance']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print('Sync native: '+str(len(rows))+' phases; real held cleanup and recovery balanced',flush=True)
if __name__=='__main__': main()
