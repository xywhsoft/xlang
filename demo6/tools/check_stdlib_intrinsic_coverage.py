"""Static public-member binding census using the genuine Linux compiler SO.
Keep registrations/classes and add native intrinsics, exact source identities,
receiver/result types, arities and lowered function reachability. A source
binding or reachable function is not proof that a runtime branch executed.
No compiler objects or replacement providers; no full53/runtime acceptance.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,re,shutil,subprocess,sys
import check_stdlib_resolved_coverage as legacy
import stdlib_fixture_inputs


def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def native_contract(root):
    rows=set()
    def add(owner, names, argc=0,kind='call'):
        for name in names.split():rows.add((owner,name,argc,kind))
    add('array','push append remove removeValue take reserve resize contains indexOf',1)
    add('array','insert swap remove',2);add('array','pop clear capacity trim reverse')
    add('list','remove take trimTo contains',1);add('list','clear trim')
    add('set','add remove contains reserve isDisjoint',1);add('set','clear capacity trim')
    add('dict','remove contains take reserve',1);add('dict','clear capacity trim')
    for owner in ('array','list','set','dict'):add(owner,'length',kind='member')
    add('Stack','create pop peek tryPop tryPeek clone trim clear count capacity')
    add('Stack','push reserve equals peekAt tryPeekAt pushBatch popBatch peekBatch',1)
    add('Stack','peekBatchAt',2)
    add('OrderedMap','create clear count keys values trim clone')
    add('OrderedMap','contains remove trimTo equals merge',1);add('OrderedMap','set merge',2)
    for match in re.finditer(r'^XL6_TREE_QUERY_OP\(\w+,\s*"([^"]+)",\s*(\d)',
                            (root/'src/abi/xl6_tree_query_ops.inc').read_text(),re.M):
        add('OrderedMap',match[1],int(match[2]))
    add('SlotMap','create')
    for match in re.finditer(r'^XL6_SLOT_MAP_OP\(\w+,\s*"([^"]+)",\s*(\d)',
                            (root/'src/abi/xl6_slot_map_ops.inc').read_text(),re.M):
        add('SlotMap',match[1],int(match[2]))
    for owner in ('SpscQueue','MpscQueue','MpmcQueue','WaitQueue'):
        add(owner,'close count capacity isClosed isDrained tryPop')
        add(owner,'create tryPush',1)
        if owner!='WaitQueue':add(owner,'reset');add(owner,'pushBatch popBatch',1)
        else:add(owner,'pop');add(owner,'popTimeout',1)
    return rows


def write_names(root,path):
    text=(root/'src/ir/xl6_ir.h').read_text()
    tables=(('XL6_IrIntrinsicKind','XL6_IR_INTRINSIC_','coverage_intrinsic_name'),
            ('XL6_IrRuntimeResourceOp','XL6_IR_RUNTIME_RESOURCE_OP_','coverage_resource_name'),
            ('XL6_IrTypedCollectionOp','XL6_IR_TYPED_COLLECTION_','coverage_typed_name'),
            ('XL6_IrDynamicCollectionOp','XL6_IR_DYNAMIC_COLLECTION_','coverage_dynamic_name'))
    parts=['/* Test-only enum labels from the pinned production headers. */\n']
    for enum,prefix,function in tables:
        body=re.search(r'typedef enum '+enum+r'\s*\{(.*?)\}\s*'+enum+r'\s*;',text,re.S)
        assert body,enum
        body=re.sub(r'/\*.*?\*/','',body[1],flags=re.S)
        names=set(n for n in re.findall(r'\b'+prefix+r'[A-Z0-9_]+\b',body) if not n.endswith('_'))
        if enum=='XL6_IrRuntimeResourceOp':
            for macro,stem,filename in (('XL6_SLOT_MAP_OP','SLOT_MAP_','xl6_slot_map_ops.inc'),
                                       ('XL6_TREE_QUERY_OP','TREE_QUERY_','xl6_tree_query_ops.inc'),
                                       ('XL6_CHANNEL_RECEIVE_OP','CHANNEL_RECEIVE_','xl6_channel_receive_ops.inc')):
                names.update(prefix+stem+m for m in re.findall(r'^'+macro+r'\((\w+)',
                             (root/'src/abi'/filename).read_text(),re.M))
        assert names
        parts.append('static const char* '+function+'(unsigned int value) {\n')
        parts.extend('    if(value=='+n+')return "'+n[len(prefix):]+'";\n' for n in sorted(names))
        parts.append('    return "UNRECOGNIZED";\n}\n')
    path.write_text(''.join(parts))


def native_binding(row):
    if not row['SourceCall'] or not row['SourceReceiverMatches']:return None
    if row.get('Admission')==3:return None  # Logical template, never a physical binding.
    owner=row['ReceiverFamily'];name=row['Spelling']
    if not row['SourceMember']:
        if row['ResourceOp'].endswith('_CREATE'):owner=row['ResultFamily'];name='create'
        else:return None
    if owner=='Queue':owner='MpmcQueue'
    if owner not in {'array','list','dict','set','Stack','OrderedMap','SlotMap','SpscQueue','MpscQueue','MpmcQueue','WaitQueue'}:return None
    return owner,name,row['SourceArgumentCount'],row['SourceKind']


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','build-report','out'):p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--jobs',type=int,default=3);p.add_argument('--fixture',action='append')
    stdlib_fixture_inputs.add_arguments(p)
    a=p.parse_args();assert os.name!='nt','This census requires the real SO frontend exports, not Windows source substitutes'
    root,out=a.root.resolve(),a.out.resolve();assert not out.exists() and 1<=a.jobs<=4
    cc=shutil.which('gcc');assert cc;task=Path(__file__).resolve().parents[1]
    build=json.loads(a.build_report.read_text());assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    library=root/'release/libxlc.so';recorded={Path(r['Path']).resolve():r['Sha256'] for r in build['Products']}
    assert recorded[library]==sha(library)
    host=task/'test/cases/stdlib_resolved_coverage.c';helper=task/'test/cases/stdlib_intrinsic_coverage.h'
    control=task/'test/cases/stdlib_intrinsic_coverage_control.xl'
    old_control=task/'test/cases/stdlib_resolved_coverage_control.xl';seed=root/'test/cases/stdlib_class_catalog.xl'
    sources=sorted(s for s in (root/'test/cases').glob('stdlib_*.xl') if legacy.PREFIX.match(s.name) or s.stem=='stdlib_crt_algorithms')
    sources=stdlib_fixture_inputs.select(sources,a.fixture_source)
    population=len(sources)
    if a.fixture:
        wanted=set(a.fixture);sources=[s for s in sources if s.stem in wanted];assert len(sources)==len(wanted)
    files={Path(__file__).resolve(),Path(stdlib_fixture_inputs.__file__).resolve(),Path(legacy.__file__).resolve(),Path(sys.executable),Path(cc),a.build_report.resolve(),
           host,helper,control,old_control,seed,*sources}
    for r in build['Inputs']+build['Products']:assert sha(r['Path'])==r['Sha256'];files.add(Path(r['Path']))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir();rows=[];cases=[];failures=[];expected=native_contract(root)
    report=dict(Complete=False,DiscoveryComplete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,
        Rows=rows,Cases=cases,Failures=failures,CandidateCount=population,SelectedCount=len(sources),ExplicitSubset=bool(a.fixture),
        FixtureSourceUpdates=[str(p.resolve()) for p in a.fixture_source],
        CompilerSourceObjects=False,UsesActualCompilerSO=True,RuntimeExecutionAcceptance=False,
        Full53Acceptance=False,NativeBindings=[],MissingNativeBindings=[],UnexpectedNativeBindings=[])
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
         ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    def run(tag,args):
        r=subprocess.run(list(map(str,args)),cwd=root,env=env,capture_output=True,timeout=180)
        log=out/(tag+'.json');log.write_bytes(r.stdout);error=out/(tag+'.stderr');error.write_bytes(r.stderr)
        row=dict(Name=tag,Command=list(map(str,args)),Exit=r.returncode,Log=str(log),LogSha256=sha(log),
                 Stderr=str(error),StderrSha256=sha(error))
        return row,r.stdout,r.stderr
    def checked(tag,args):
        row,data,error=run(tag,args);rows.append(row);assert row['Exit']==0,(tag,error[-5000:]);return json.loads(data)
    bindings={};reachable={};static={};class_observed={}
    try:
        names=out/'enum_names.h';write_names(root,names);binary=out/'resolved'
        row,data,error=run('build',[cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-D_GNU_SOURCE',
            '-I'+str(root),'-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc'),'-I'+str(task/'test/cases'),
            '-DXL6_COVERAGE_ENUM_NAMES_HEADER="'+str(names)+'"',host,'-L'+str(library.parent),'-lxlc',
            '-Wl,-rpath,'+str(library.parent),'-pthread','-ldl','-lm','-o',binary])
        rows.append(row);assert row['Exit']==0,error[-5000:]
        catalog=checked('catalog',[binary,'--catalog']);classes=checked('class-catalog',[binary,'--class-catalog',seed])
        # The shared observer still serves the legacy resolved-call gate.
        # Compile that real SO client too; opt-in native observation may add
        # fields, never silently change the existing call/catalog semantics.
        legacy_binary=out/'legacy-resolved'
        row,data,error=run('legacy-build',[cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-D_GNU_SOURCE',
            '-I'+str(root),'-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc'),host,
            '-L'+str(library.parent),'-lxlc','-Wl,-rpath,'+str(library.parent),'-pthread','-ldl','-lm','-o',legacy_binary])
        rows.append(row);assert row['Exit']==0,error[-5000:]
        assert checked('legacy-catalog',[legacy_binary,'--catalog'])==catalog
        expected_static={}
        for item in catalog['Registrations']:
            vectors=legacy.declarations(item['Ast'],item['Internal']) if item['Ast'] else [item['ParameterTypes']]
            assert vectors
            for vector in vectors:expected_static.setdefault(legacy.key(item['Internal'],vector),[]).append(item)
        members=legacy.class_members(classes)
        control_data=checked('native-control',[binary,control])
        hits={native_binding(r) for r in control_data['Intrinsics']};hits.discard(None)
        visible={native_binding(r) for r in control_data['Intrinsics'] if r['InReachableFunction']};visible.discard(None)
        assert hits=={('Stack','create',0,'call'),('Stack','reserve',1,'call'),('Stack','peek',0,'call')},hits
        assert visible==hits-{('Stack','peek',0,'call')},visible
        old=checked('overload-control',[binary,old_control])
        assert checked('legacy-overload-control',[legacy_binary,old_control])['Calls']==old['Calls']
        report['LegacyObserverCompatibility']=True
        observed={legacy.key(c['Internal'],c['ParameterTypes']) for c in old['Calls'] if c['SourceCall'] and legacy.key(c['Internal'],c['ParameterTypes']) in expected_static}
        assert observed=={legacy.key('__xl_std_encoding_is_utf8',('string','int'))},observed
        def scan(source):
            row,data,error=run(source.stem,[binary,source]);record=dict(Name=source.stem,Row=row)
            if source.stem in legacy.NEGATIVE:
                record['ExpectedRejection']=row['Exit']!=0 and legacy.NEGATIVE[source.stem].removeprefix('[sem] ').encode() in data+error
            elif row['Exit']!=0:record['Error']=error.decode(errors='replace')[-5000:]
            else:record.update(json.loads(data))
            return record
        with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
            for case in pool.map(scan,sources):
                cases.append(case);rows.append(case['Row']);name=case['Name']
                if case.get('Error') or case.get('ExpectedRejection') is False:failures.append(dict(Name=name,Error=case.get('Error','negative mismatch')))
                for call in case.get('Calls',[]):
                    identity=legacy.key(call['Internal'],call['ParameterTypes'])
                    if call['SourceCall']:
                        if identity in expected_static:static.setdefault(identity,set()).add(name)
                        elif call['Internal'] in ('__xl_std_array_sort','__xl_std_array_sort_by','__xl_std_array_binary_search'):
                            for template in expected_static:
                                if template[0]==identity[0] and legacy.generic_match(template[1],identity[1]):static.setdefault(template,set()).add(name)
                        if call['Owner']:class_observed.setdefault(legacy.class_call(call),set()).add(name)
                for entry in case.get('Intrinsics',[])+case.get('Properties',[]):
                    identity=native_binding(entry)
                    if identity:
                        if 'Admission' in entry:
                            assert entry['Admission']==1 and 'UNRECOGNIZED' not in (entry['Kind'],entry['ResourceOp'],entry['TypedOp'],entry['DynamicOp']), (name,entry)
                        else:assert entry['VerifiedLoweredModule']
                        bindings.setdefault(identity,set()).add(name)
                        if entry['InReachableFunction']:reachable.setdefault(identity,set()).add(name)
                print(name+': '+('rejected' if case.get('ExpectedRejection') else 'error' if case.get('Error') else 'scanned'),flush=True)
        class_missing=[]
        for member in members:
            if member['Category']!='callable':continue
            identity=(member['Owner'],member['Name'],tuple(member['ParameterTypes']));hits=set(class_observed.get(identity,()))
            if member['Owner']=='FixedStack':
                for concrete,fixtures in class_observed.items():
                    if concrete[:2]==identity[:2] and legacy.generic_match(identity[2],concrete[2]):hits.update(fixtures)
            member['Fixtures']=sorted(hits)
            if not hits:class_missing.append(member)
        def native_row(identity):
            return dict(Owner=identity[0],Name=identity[1],SourceArgumentCount=identity[2],SourceKind=identity[3],
                        Fixtures=sorted(bindings.get(identity,())),ReachableFunctionFixtures=sorted(reachable.get(identity,())))
        report.update(NativeExpectedCount=len(expected),NativeBoundCount=len(expected&bindings.keys()),
            NativeBindings=[native_row(k) for k in sorted(expected)],MissingNativeBindings=[native_row(k) for k in sorted(expected-bindings.keys())],
            UnexpectedNativeBindings=[native_row(k) for k in sorted(bindings.keys()-expected)],
            StaticRegistrationCount=len(catalog['Registrations']),StaticExpectedCount=len(expected_static),StaticBoundCount=len(static),
            MissingStaticBindings=[dict(Internal=k[0],ParameterTypes=list(k[1])) for k in expected_static if k not in static],
            ClassMembers=members,MissingClassBindings=class_missing,SourceIdentityControl=True,UnreachableControl=True)
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        report['DiscoveryComplete']=report['InputsUnchanged'] and len(cases)==len(sources) and not failures
        report['Complete']=report['DiscoveryComplete'] and not a.fixture and not any(report[k] for k in
            ('MissingNativeBindings','UnexpectedNativeBindings','MissingStaticBindings','MissingClassBindings'))
    except Exception as error:failures.append(dict(Name='gate',Error=repr(error)))
    report['Products']=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(out.rglob('*')) if f.is_file()]
    (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k in ('Complete','DiscoveryComplete','Failures','NativeExpectedCount','NativeBoundCount')} |
        dict(MissingNative=len(report['MissingNativeBindings']),UnexpectedNative=len(report['UnexpectedNativeBindings']))),flush=True)
    return 0 if report['Complete'] else 1


if __name__=='__main__':raise SystemExit(main())
