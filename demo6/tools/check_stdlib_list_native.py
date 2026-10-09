"""Private resident typed List foundation: sparse keys, metadata, cursors,
physical ownership, explicit failures and whole allocation-failure prefixes.
Includes immutable same-owner erasure, one-level sparse wire projection,
explicit admitted imports and exact mixed-family resident nested schemas.
No recursive List graph/persistence conversion claim.
Not shipping-library, source-language List cutover or full53 acceptance.
"""
from __future__ import annotations
import argparse, hashlib, json, os, re, shutil, subprocess, sys, time
from pathlib import Path

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--graph',action='store_true',help='Optional single-memo List/Array/object graph provider and exact format plans')
    parser.add_argument('--restore',action='store_true',help='Mixed-family shared import memo and explicit JSON/XLON leaf restoration')
    args=parser.parse_args(); root=args.root.resolve(); out=args.out.resolve()
    cc,nm=shutil.which('gcc'),shutil.which('nm')
    assert cc and nm and not out.exists() and (not args.sanitize or sys.platform=='linux')
    sources=['test/cases/stdlib_list_native_host.c','test/cases/stdlib_list_schema_native_host.c','test/support/list_native.c',
        'test/support/module_product_memory.c','src/runtime/xl6_list.c','src/runtime/xl6_sequence.c',
        'src/runtime/xl6_array_reference_admission.c','src/runtime/xl6_array_reference_value.c',
        'src/runtime/xl6_string_value.c','src/runtime/xl6_nullable_i64_value.c','src/api/xl_foreign_error.c',
        'src/runtime/xl6_array_nominal_admission.c','src/runtime/xl6_array_primitive_admission.c',
        'src/runtime/xl6_array_c_scalar_admission.c','src/runtime/xl6_value_admission.c',
        'src/runtime/xl6_nominal_admission.c','src/runtime/xl6_string_type.c',
        'src/runtime/xl6_nullable_string.c','src/runtime/xl6_nullable_i64.c',
        'src/runtime/xl6_ownership_plan.c','src/runtime/xl6_array_future_admission.c','src/runtime/xl6_char_type.c',
        'src/runtime/xl6_char_value.c','src/runtime/xl6_list_schema.c','src/runtime/xl6_array_sequence_admission.c',
        'src/runtime/xl6_array.c','src/runtime/xl6_stack.c']
    if args.graph:
        sources.remove('test/support/list_native.c')
        sources+=['test/support/list_graph_native.c','test/cases/stdlib_list_graph_native_host.c',
            'src/runtime/xl6_list_graph.c','src/runtime/xl6_array_graph.c','src/runtime/xl6_array_erased.c',
            'src/runtime/xl6_persistence_project.c']
    if args.restore:
        sources.remove('test/support/list_graph_native.c' if args.graph else 'test/support/list_native.c')
        sources+=['test/support/list_restore_native.c']
        sources+=['test/cases/stdlib_list_restore_native_host.c','src/runtime/xl6_list_import_chain.c',
            'src/runtime/xl6_array_import_chain.c','src/runtime/xl6_array_input.c']
        if not args.graph: sources+=['src/runtime/xl6_array_erased.c']
    files={Path(__file__).resolve(),Path(cc),Path(nm),Path(sys.executable)}
    files.update(root/name for name in sources)
    for folder in ('src/runtime','src/api','release/include','lib/xrt','lib/xruntime','test/support'):
        files.update((root/folder).rglob('*.h'))
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    rows=[]; prefixes=[]; failures=[]
    report={'Complete':False,'InputsUnchanged':False,'Inputs':inputs,'Rows':rows,'Prefixes':prefixes,
        'Failures':failures,'Platform':sys.platform,'Sanitized':args.sanitize,
        'Graph':args.graph,'Restore':args.restore,
        'SelectedNativeClosureInstrumented':args.sanitize,'WholeRuntimeInstrumented':False,
        'ShippingLibraryAcceptance':False,'SourceLanguageListCutover':False,'Full53Acceptance':False,
        'CleanupBoundaryFailureInjection':'GNU ld --wrap; real cleanup delegates except explicitly armed vectors; not actual OS exhaustion',
        'EntryBoundaryFailureInjection':'GNU ld --wrap=xl6ErrorScopeBegin; delegates unless explicitly armed; not actual OS exhaustion',
        'Scope':'Private mixed List/Array recursive import and explicit scalar restoration in the existing type-depth memo; not class-field restore, source carrier, shipping or full53 acceptance' if args.restore else 'Private List/Array/object single XRT graph memo, resident provider/format plans, native deep copy and data projection; not recursive restore, source carrier or shipping/full53 acceptance' if args.graph else __doc__}
    out.mkdir(parents=True)
    env=dict(os.environ)
    if args.sanitize: env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
        UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name,command,timeout=60):
        command=list(map(str,command)); start=time.monotonic()
        try:
            result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=timeout)
            data=result.stdout+result.stderr; code=result.returncode
        except subprocess.TimeoutExpired as error:
            data=(error.stdout or b'')+(error.stderr or b'')+b'\nTIMEOUT\n'; code=None
        (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':code,'Seconds':time.monotonic()-start,'Complete':code==0})
        if code!=0:
            failures.append(name); print(name+': '+data[-2500:].decode(errors='replace'),file=sys.stderr,flush=True)
            return None
        return data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include']
    if args.graph: flags+=['-DXL6_TEST_LIST_GRAPH']
    if args.restore: flags+=['-DXL6_TEST_LIST_RESTORE']
    links=['-Wl,--wrap=xl6ErrorScopeBeginCleanup','-Wl,--wrap=xl6ErrorScopeBegin']+([] if os.name=='nt' else ['-pthread','-lm'])
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    if args.sanitize: flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
    modes=('integers','text','profiles','cursors','rejected','references','futures','phased','reentrant','boundaries',
        'identity','merges','identity-errors','shared-depth','guard-protocol',
        'projection','projection-profiles','imports','codec-errors','projection-entry-errors',
        'schema-profiles','schema-failures','schema-depth','schema-busy','schema-cleanup-errors')
    operations=('create','insert','replace','append','clone','cursor','get','take','cursor-read','cursor-advance','clear','trim',
        'merge-keep','merge-replace','ref-equal','value-get','value-take','value-set','value-append',
        'wire','import-wire','import-native','import-convert','projection-take',
        'schema-create','schema-list-create','schema-deep-create')
    if args.graph:
        modes+=('graph-profiles','graph-mixed','graph-cycles','graph-plans','graph-codec-errors','graph-entry-errors','graph-provider-errors')
        operations+=('graph-deep','graph-data','graph-sequence','graph-project','graph-mixed-deep','graph-mixed-data')
    if args.restore:
        modes+=('restore-profiles','restore-mixed','restore-domains','restore-rejected','restore-depth','restore-entry-errors','restore-provider-errors',
            'restore-format-roundtrip','restore-cleanup-errors')
        operations+=('restore-native','restore-json','restore-xlon','restore-wide','restore-source-take')
    try:
        for level in (0,2):
            lane='O'+str(level); binary=out/('list-'+lane+('.exe' if os.name=='nt' else ''))
            if run(lane+'.build',[cc,*flags,'-O'+str(level),*sources,*links,'-o',binary],180) is None: continue
            symbols=run(lane+'.symbols',[nm,'--defined-only',binary])
            if symbols is None: continue
            names={line.split()[-1].lstrip('_') for line in symbols.splitlines() if line.split()}
            assert {'xl6ListCreateTakeV1','xl6ListCursorAdvanceV1','xrtTypedListClone',
                'xl6ListToWireV1','xl6ListImportTakeV1','xrtTypedListSetValue',
                'xl6SequenceListCreateTakeV1','xl6ListSchemaCopyV1'}<=names
            assert not {'xl6ArrayFromValueV1','xl6ArrayToValueV1'}&names
            if args.graph:
                assert {'xl6ListGraphProviderV1','xl6ValueDeepCloneNativeV1','xl6PersistenceProjectNativeV1',
                    'xrtValueGraphCopyV1','xrtValueGraphNextV1'}<=names
            else: assert not {'xl6ListGraphProviderV1','xl6ValueDeepCloneNativeV1'}&names
            if args.restore: assert {'xl6ListImportProviderV1','xl6CollectionImportChainTakeV1','xl6ArrayImportChainTakeV1'}<=names
            else: assert 'xl6ListImportProviderV1' not in names
            if not args.graph and not args.restore: assert 'xl6ArrayErasedReadV1' not in names
            for mode in modes: run(lane+'.'+mode,[binary,mode])
            for operation in operations:
                for point in range(4097):
                    text=run(lane+'.'+operation+'.'+str(point),[binary,operation,point])
                    if text is None: break
                    match=re.search(r'hit=([01]) complete-prefix balanced',text)
                    assert match,'Missing explicit ownership/fault result'
                    if match[1]=='0':
                        prefixes.append({'Lane':lane,'Operation':operation,'FirstNoHit':point,'Runs':point+1}); break
                else: failures.append(lane+'.'+operation+'.unclosed')
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        report['Complete']=not failures and report['InputsUnchanged'] and len(prefixes)==2*len(operations)
    except Exception as error:
        report['Error']=repr(error); print(error,file=sys.stderr)
    finally:
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f'Native List: {len(rows)} phases, {len(prefixes)} closed prefixes, complete={report["Complete"]}',flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
