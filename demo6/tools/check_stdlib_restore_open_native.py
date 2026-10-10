"""Private native Open restoration: declared clone, fresh DynamicFields and
atomic slot publication. Rebuilds the actual selected XRT/xruntime closure.
Not language Open cutover, shipping-library or full53 acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess, sys, time

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args(); root=args.root.resolve(); out=args.out.resolve()
    cc,nm=shutil.which('gcc'),shutil.which('nm')
    assert cc and nm and not out.exists() and (not args.sanitize or sys.platform=='linux')
    sources=['test/cases/stdlib_restore_open_native_host.c','test/support/open_restore_native.c',
        'test/support/module_product_memory.c','src/runtime/xl6_list.c','src/runtime/xl6_sequence.c',
        'src/runtime/xl6_array_reference_admission.c','src/runtime/xl6_array_reference_value.c',
        'src/runtime/xl6_string_value.c','src/runtime/xl6_nullable_i64_value.c','src/api/xl_foreign_error.c',
        'src/runtime/xl6_array_nominal_admission.c','src/runtime/xl6_array_primitive_admission.c',
        'src/runtime/xl6_array_c_scalar_admission.c','src/runtime/xl6_value_admission.c',
        'src/runtime/xl6_nominal_admission.c','src/runtime/xl6_string_type.c',
        'src/runtime/xl6_nullable_string.c','src/runtime/xl6_nullable_i64.c',
        'src/runtime/xl6_ownership_plan.c','src/runtime/xl6_array_future_admission.c','src/runtime/xl6_char_type.c',
        'src/runtime/xl6_char_value.c','src/runtime/xl6_list_schema.c','src/runtime/xl6_array_sequence_admission.c',
        'src/runtime/xl6_array.c','src/runtime/xl6_stack.c','src/runtime/xl6_array_graph.c',
        'src/runtime/xl6_array_erased.c','src/runtime/xl6_list_import_chain.c',
        'src/runtime/xl6_array_import_chain.c','src/runtime/xl6_array_input.c',
        'src/runtime/xl6_code_finalizer.c','src/runtime/xl6_code_lease.c',
        'src/runtime/xl6_restore_class.c','src/runtime/xl6_restore_class_closed.c',
        'src/runtime/xl6_dynamic_fields.c','src/runtime/xl6_dynamic_fields_copy.c',
        'src/runtime/xl6_native_handle.c','src/runtime/xl6_restore_open.c']
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),Path(nm)}
    files.update(root/name for name in sources)
    for folder in ('src/runtime','src/api','release/include','lib/xrt','lib/xruntime','test/support'):
        files.update((root/folder).rglob('*.h'))
    # These selected implementation includes are real input too.
    files.update(root/'test/support'/name for name in ('list_native.c','list_restore_native.c'))
    for name in ('cc1','as','ld'):
        selected=subprocess.check_output([cc,'-print-prog-name='+name],text=True).strip()
        files.add((Path(selected) if Path(selected).is_absolute() else Path(shutil.which(selected))).resolve())
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,'Inputs':inputs,
        'Rows':[],'Prefixes':[],'Failures':[],'Products':[], 'Platform':sys.platform,
        'Sanitized':args.sanitize,'SelectedNativeClosureInstrumented':args.sanitize,
        'WholeRuntimeInstrumented':False,'SourceLanguageOpenCutover':False,
        'ShippingLibraryAcceptance':False,'Full53Acceptance':False}
    out.mkdir(parents=True); env=dict(os.environ)
    if args.sanitize: env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
        UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name,command,reject=False):
        command=list(map(str,command)); start=time.monotonic()
        result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; log=out/(name+'.log'); log.write_bytes(data)
        ok=result.returncode!=0 if reject else result.returncode==0
        report['Rows'].append({'Name':name,'Command':command,'Exit':result.returncode,
            'ExpectedRejection':reject,'Seconds':time.monotonic()-start,'Complete':ok,
            'Log':str(log),'LogSha256':sha(log)})
        if not ok:
            report['Failures'].append(name)
            print(name+': '+data[-4000:].decode(errors='replace'),file=sys.stderr,flush=True)
        return data.decode(errors='replace') if ok else None
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include']
    links=[] if os.name=='nt' else ['-pthread','-lm']
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    if args.sanitize: flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
    suffix='.exe' if os.name=='nt' else ''
    try:
        for level in (0,2):
            lane='O'+str(level); product=out/('open-'+lane+suffix)
            if run(lane+'.build',[cc,*flags,'-O'+str(level),*sources,*links,'-o',product]) is None: continue
            symbols=run(lane+'.symbols',[nm,'--defined-only',product])
            assert symbols is not None
            names={line.split()[-1].lstrip('_') for line in symbols.splitlines() if line.split()}
            assert {'xl6RestoreOpenProviderV1','xl6PersistenceRestorePlanReadTakeV1',
                'xl6ClassRuntimeV2Get','xrtDynamicFieldsOwnershipAdapterV1',
                'xl6DynamicFieldsHandlePolicyV1Get','xrtValueObjectFieldsCommitV2'}<=names
            text=run(lane+'.vectors-and-complete-prefixes',[product])
            if text is None: continue
            match=re.search(r'Open: (\d+) complete allocation-failure prefixes balanced',text)
            assert match and int(match[1])>1
            report['Prefixes'].append({'Lane':lane,'Runs':int(match[1]),'FirstNoHit':int(match[1])-1,'Batched':True})
        # Compile actual invalid providers in private files, never edit a pinned
        # input in place. A compile error is NOT a successful negative control.
        implementation=root/'src/runtime/xl6_restore_open.c'
        original=implementation.read_text(encoding='utf-8')
        controls={
            'omit-claimed-key-filter':('if (xl6RestoreOpenClaimsKeyV1(recipe,key)) continue;','if (false) continue;'),
            'omit-extra-insertion':('bool stored=item && xrtDynamicFieldsSetTake(raw,key,&item);','bool stored=item!=NULL;'),
            'write-receiver-before-commit':('/* Hold the first wire store across arbitrary declared clone code. */',
                'if (!xrtValueObjectSetNew((xvalue*)receiver,XRT_STR_LITERAL("id"),xrtValueInt(99))) goto finish;'),
        }
        for name,(old,new) in controls.items():
            assert original.count(old)==1,(name,original.count(old))
            mutant=out/(name+'.c'); mutant.write_text(original.replace(old,new),encoding='utf-8')
            selected=[mutant if source=='src/runtime/xl6_restore_open.c' else source for source in sources]
            product=out/(name+suffix)
            if run('negative-'+name+'.build',[cc,*flags,'-Isrc/runtime','-O0',*selected,*links,'-o',product]) is not None:
                run('negative-'+name+'.run',[product],reject=True)
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        report['Complete']=not report['Failures'] and report['InputsUnchanged'] and len(report['Prefixes'])==2
    except Exception as error:
        report['Error']=repr(error); print(error,file=sys.stderr)
    finally:
        report['Products']=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(out.iterdir())
            if path.is_file() and (path.suffix=='.exe' if os.name=='nt' else path.suffix=='')]
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Native Open: '+str(len(report['Rows']))+' phases, '+str(len(report['Prefixes']))+
        ' closed allocation-prefix groups, complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
