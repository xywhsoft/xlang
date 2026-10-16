"""Build and run the entire three-group fixture inventory as relocated programs.

Uses fresh products and embedded VFS without source/provider fallback. Native
host fixtures and platform conditions remain explicit requirements, not passes.
Successful programs have no xl/xlc imports (and no Linux RPATH/RUNPATH). This
is inventory execution, not branch/function completeness or full53 acceptance.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,importlib,json,os,re,shutil,subprocess,sys,threading,time
import stdlib_fixture_inputs

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--jobs',type=int,default=2)
    parser.add_argument('--fixture',action='append',help='Explicit subset, never complete inventory acceptance')
    stdlib_fixture_inputs.add_arguments(parser)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve();assert not out.exists() and 1<=args.jobs<=4
    windows=os.name=='nt';cc=shutil.which('gcc');inspector=shutil.which('objdump' if windows else 'readelf');assert cc and inspector
    build=json.loads(args.build_report.read_text(encoding='utf-8-sig'));assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    sys.path.insert(0,str(root/'tools'));inventory=importlib.import_module('discover_stdlib_three_groups');execution=importlib.import_module('execute_stdlib_three_groups')
    sources=sorted(p for p in (root/'test/cases').glob('stdlib_*.xl') if inventory.PREFIX.match(p.name));candidate_count=len(sources)
    sources=stdlib_fixture_inputs.select(sources,args.fixture_source)
    if args.fixture:
        selected=set(args.fixture);sources=[p for p in sources if p.stem in selected];assert len(sources)==len(selected)
    products=[root/'release'/name for name in (('xl.exe','xl.dll','xlc.dll') if windows else ('xl','libxl.so','libxlc.so'))]
    recorded={Path(row['Path']).resolve():row['Sha256'] for row in build['Products']}
    assert all(recorded.get(product)==sha(product) for product in products),'Root products do not match the build evidence'
    pinned={Path(__file__).resolve(),Path(stdlib_fixture_inputs.__file__).resolve(),args.build_report.resolve(),Path(sys.executable),Path(cc),Path(inspector),Path(inventory.__file__),Path(execution.__file__),*sources}
    pinned.update(root/'test/cases'/name for name in ('xlir_process_child.c','stdlib_string_support.c',*execution.FIXTURE_C_SUPPORT.values()))
    for row in build['Inputs']+build['Products']:assert sha(row['Path'])==row['Sha256'];pinned.add(Path(row['Path']))
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(pinned)]
    out.mkdir();isolated=out/'isolated';release=isolated/'release';release.mkdir(parents=True);staged=isolated/'inputs';staged.mkdir()
    relocated=out/'relocated';relocated.mkdir();temporary=out/'tmp';temporary.mkdir();(isolated/'build/test').mkdir(parents=True)
    for product in products:shutil.copy2(product,release/product.name)
    for source in sources:shutil.copy2(source,staged/source.name)
    for name in ('stdlib_string_support.c',*execution.FIXTURE_C_SUPPORT.values()):shutil.copy2(root/'test/cases'/name,staged/name)
    assert not any((isolated/name).exists() for name in ('src','lib','res','include'))
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(TMPDIR=str(temporary),TEMP=str(temporary),TMP=str(temporary),XL6_PRODUCT_CLI=str(release/products[0].name))
    rows=[];cases=[];mutable=threading.Lock();row_lock=threading.Lock()
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Cases=cases,CandidateCount=candidate_count,SelectedCount=len(sources),
        ExplicitSubset=bool(args.fixture),FixtureSourceUpdates=[str(p.resolve()) for p in args.fixture_source],Scope=__doc__,AllExecutablePassed=False,RelocatedAotInventoryAcceptance=False,Full53Acceptance=False,FunctionCoverageAcceptance=False)
    def command(name,argv,environment=env,timeout=240):
        argv=list(map(str,argv));started=time.monotonic()
        try:
            result=subprocess.run(argv,cwd=isolated,env=environment,capture_output=True,timeout=timeout);code,data=result.returncode,result.stdout+result.stderr
        except subprocess.TimeoutExpired as error:code,data='timeout',(error.stdout or b'')+(error.stderr or b'')
        log=out/(name+'.log');log.write_bytes(data);row=dict(Name=name,Command=argv,Exit=code,Seconds=time.monotonic()-started,Log=str(log),LogSha256=sha(log))
        with row_lock:rows.append(row)
        return row,data
    def save():
        report['PhaseCount']=len(rows);pending=out/'results.json.pending';pending.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');pending.replace(out/'results.json')
    try:
        child=out/('process-child'+('.exe' if windows else ''))
        row,data=command('child-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-O2',*(['-municode'] if windows else ['-D_GNU_SOURCE']),root/'test/cases/xlir_process_child.c','-o',child])
        assert row['Exit']==0,data[-4000:];env['XL6_PROCESS_CHILD']=str(child)
        def execute(source):
            record=dict(Name=source.stem,Source=str(source),SourceSha256=sha(source),Rows=[],Executed=False,Passed=False)
            if source.stem in execution.HOSTED:record.update(Status='requires-host',Reason=execution.HOSTED[source.stem]);return record
            if source.stem=='stdlib_array_entry_values':
                record.update(Status='requires-host',Reason='Explicit NULL/boxed/native Array entry arguments, including bytes and embedded NUL, require the entry host; OS argv includes argv[0]');return record
            if windows and source.stem=='stdlib_time_dst':record.update(Status='requires-platform',Reason='IANA TZ fixture requires a native Windows transition host');return record
            if not windows and source.stem=='stdlib_process':record.update(Status='requires-platform',Reason='legacy cmd/findstr/more fixture; portable fixtures execute separately');return record
            directory=out/source.stem;directory.mkdir();environment=dict(env);text=source.read_text(encoding='utf-8-sig')
            if source.stem=='stdlib_time_dst':environment['TZ']='America/New_York'
            if 'XL6_PERSIST_FILE_ROOT' in text:
                files=directory/'files';files.mkdir();environment['XL6_PERSIST_FILE_ROOT']=str(files)
            support=[]
            if set(re.findall(r'^extern\s+"([^"]+)"',text,re.M))&{'expect_i64','expect_str'}:support.append(staged/'stdlib_string_support.c')
            if source.stem in execution.FIXTURE_C_SUPPORT:support.append(staged/execution.FIXTURE_C_SUPPORT[source.stem])
            image=directory/('program.exe' if windows else 'program');guard=mutable if source.stem.startswith('stdlib_process') or re.search(r'\b(file|dir)\.',text) else None
            if guard:guard.acquire()
            try:
                row,data=command(source.stem+'-build',[release/products[0].name,'build',staged/source.name,*support,'-o',image],environment);record['Rows'].append(row)
                if source.stem in inventory.NEGATIVE:
                    record.update(Status='expected-rejection',Passed=row['Exit'] not in (0,'timeout') and inventory.NEGATIVE[source.stem].encode() in data and not image.exists());return record
                if row['Exit']!=0:record.update(Status='failed-build',Diagnostic=data.decode(errors='replace')[-6000:]);return record
                moved=relocated/(source.stem+('.exe' if windows else ''));shutil.move(image,moved)
                row,data=command(source.stem+'-imports',[inspector,'-p' if windows else '-d',moved],environment);record['Rows'].append(row)
                imports=data.decode(errors='replace');dependencies=re.findall(r'DLL Name:\s*(\S+)',imports) if windows else re.findall(r'\(NEEDED\).*?\[(.*?)\]',imports)
                if row['Exit']!=0 or any(n.lower() in ('xl.dll','xlc.dll') or 'libxl' in n.lower() for n in dependencies) or (not windows and ('(RPATH)' in imports or '(RUNPATH)' in imports)):
                    record.update(Status='failed-independence',Diagnostic=imports[-6000:]);return record
                record.update(Image=dict(Path=str(moved),Sha256=sha(moved)),Dependencies=dependencies)
                argv=['','你','🙂','a"b','C:\\path with space\\'] if source.stem=='stdlib_array_entry_argv' else []
                row,data=command(source.stem+'-run',[moved,*argv],environment,timeout=180);record['Rows'].append(row);record['Executed']=True
                record.update(Status='passed' if row['Exit']==0 else 'failed-run',Passed=row['Exit']==0)
                if not record['Passed']:record['Diagnostic']=data.decode(errors='replace')[-6000:]
                return record
            finally:
                if guard:guard.release()
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            for case in pool.map(execute,sources):cases.append(case);save();print(case['Name']+': '+case['Status'],flush=True)
        executable=[case for case in cases if case['Status'] not in ('requires-host','requires-platform')]
        report['AllExecutablePassed']=all(case['Passed'] for case in executable)
        report['UnexecutedRequirements']=[dict(Name=case['Name'],Status=case['Status'],Reason=case['Reason']) for case in cases if case['Status'] in ('requires-host','requires-platform')]
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        report['Complete']=report['InputsUnchanged'] and len(cases)==len(sources) and report['AllExecutablePassed']
        report['RelocatedAotInventoryAcceptance']=report['Complete'] and not args.fixture
    finally:save()
    print(json.dumps(dict(Complete=report['Complete'],Cases=len(cases),Failures=[case['Name'] for case in cases if case['Status'] not in ('requires-host','requires-platform') and not case['Passed']])),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
