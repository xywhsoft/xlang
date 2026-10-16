"""Aggregate fresh product gates and execute the complete three-group fixture
inventory through real compiler DLL/SO run/relocate APIs and embedded VFS.
Independent failures are collected, hosted/platform requirements remain visible.
No source/provider fallback, release promotion, worker-wide OOM or full53 claim.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,importlib, json,os,re,shutil,subprocess,sys,threading,time
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--jobs',type=int,default=2);a=p.parse_args()
    root,out=a.root.resolve(),a.out.resolve();assert not out.exists() and 1<=a.jobs<=4
    cc=shutil.which('gcc');assert cc;windows=os.name=='nt'
    build=json.loads(a.build_report.read_text(encoding='utf-8-sig'));assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    sys.path.insert(0,str(root/'tools'))
    inventory=importlib.import_module('discover_stdlib_three_groups')
    execution=importlib.import_module('execute_stdlib_three_groups')
    # A new test runner/fixture may follow the frozen product build. Only these
    # explicit test inputs come from its own selected task source, never src/lib.
    task=Path(__file__).resolve().parents[1]
    host=task/'test/cases/stdlib_product_fixture_host.c'
    quote=task/'test/cases/stdlib_process_shell_quotes.xl'
    pathFixture=task/'test/cases/stdlib_process_path_parts.xl'
    pathHost=task/'test/cases/stdlib_path_build_product_host.c'
    childSource=task/'test/cases/xlir_process_child.c'
    terminalFixtures={stem:task/'test/cases'/(stem+'.xl') for stem in
        ('stdlib_process_terminal_session','stdlib_process_terminal_event_order')}
    cases=sorted((root/'test/cases').glob('stdlib_*.xl'));cases=[s for s in cases if inventory.PREFIX.match(s.name)]
    cases=[quote if s.stem==quote.stem else s for s in cases]
    cases=[terminalFixtures.get(s.stem,s) for s in cases]
    if not any(s.stem==pathFixture.stem for s in cases):cases.append(pathFixture);cases.sort()
    assert cases
    gates=[('headers','check_stdlib_runtime_header_vfs.py',None,True),
        ('nested','check_stdlib_nested_aot.py','results.json',True),
        ('graphs','check_stdlib_container_graph_shipping.py','results.json',True),
        ('clear','check_stdlib_clear_shipping.py','results.json',True),
        ('array-entry','check_stdlib_array_entry_shipping.py','results.json',True),
        ('thread','check_stdlib_thread_shipping.py','thread.shipping.json',True),
        ('sync','check_stdlib_sync_shipping.py','sync.shipping.json',False),
        # This assertion runner may follow the pinned product build. Its only
        # native code observes callbacks through the embedded SDK; production
        # sources, headers and providers still come exclusively from the build.
        ('aot-graphs',task/'tools/check_stdlib_aot_graph_retirement.py','results.json',True),
        ('terminal',task/'tools/check_stdlib_terminal_shipping.py','results.json',True),
        ('terminal-ledger',task/'tools/check_stdlib_terminal_ledger_shipping.py','results.json',True),
        ('worker-fault',task/'tools/check_stdlib_worker_fault_shipping.py','results.json',True)]
    if not windows:gates.append(('elf','check_stdlib_sync_shipping_elf.py','sync.shipping.elf.json',True))
    products=[root/'release'/n for n in (('xl.exe','xl.dll','xlc.dll') if windows else ('xl','libxl.so','libxlc.so'))]
    recorded={Path(row['Path']).resolve():row['Sha256'] for row in build['Products']}
    assert all(recorded[f]==sha(f) for f in products)
    files={Path(__file__).resolve(),Path(cc),Path(sys.executable),a.build_report.resolve(),host,quote,pathFixture,pathHost,
        Path(inventory.__file__),Path(execution.__file__),root/'test/cases/stdlib_string_support.c',childSource,*cases}
    files.update(root/'tools'/g[1] for g in gates)
    files.update(root/'test/cases'/name for name in execution.FIXTURE_C_SUPPORT.values())
    for row in build['Inputs']+build['Products']:assert sha(row['Path'])==row['Sha256'];files.add(Path(row['Path']))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir();isolated=out/'isolated';(isolated/'release').mkdir(parents=True);(isolated/'inputs').mkdir();tmp=out/'tmp';tmp.mkdir()
    for f in products:
        shutil.copy2(f,isolated/'release'/f.name)
        if windows:shutil.copy2(f,isolated/f.name)
    sources={}
    for f in cases:target=isolated/'inputs'/f.name;shutil.copy2(f,target);sources[f.stem]=target
    sidecar=isolated/'inputs/stdlib_string_support.c';shutil.copy2(root/'test/cases'/sidecar.name,sidecar)
    fixtureSupport={}
    for name,support in execution.FIXTURE_C_SUPPORT.items():
        destination=isolated/'inputs'/support;shutil.copy2(root/'test/cases'/support,destination);fixtureSupport[name]=destination
    assert not any((isolated/f).exists() for f in ('src','lib','res','include'))
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
        ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp),XL6_PRODUCT_CLI=str(isolated/'release'/products[0].name))
    rows=[];records=[];children=[];failures=[];lock=threading.Lock();rowLock=threading.Lock()
    report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,Cases=records,Children=children,Failures=failures,
        CandidateCount=len(cases),ExplicitSubset=False,SourceProductAcceptance=True,ProviderOverrides=False,EmbeddedVfsWithoutFallback=True,
        AllExecutablePassed=False,HostedProductAcceptance=False,AotGraphRetirementAcceptance=False,
        TerminalReadinessAcceptance=False,
        NativeTerminalLedgerAcceptance=False,
        WorkerBodyFaultPrefixAcceptance=False,
        Full53Acceptance=False,FunctionCoverageAcceptance=False,WorkerWideOomAcceptance=False)
    def save():(out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    def run(name,command,directory=isolated,environment=env,timeout=180):
        start=time.monotonic()
        try:r=subprocess.run(list(map(str,command)),cwd=directory,env=environment,capture_output=True,timeout=timeout);code,data=r.returncode,r.stdout+r.stderr
        except subprocess.TimeoutExpired as e:code,data='timeout',(e.stdout or b'')+(e.stderr or b'')
        log=out/(name+'.log');log.write_bytes(data)
        row=dict(Name=name,Command=list(map(str,command)),Exit=code,Log=str(log),LogSha256=sha(log),Seconds=time.monotonic()-start)
        with rowLock:rows.append(row)
        return row,data
    try:
        binary=isolated/('fixture-host'+('.exe' if windows else ''))
        libs=[isolated/'release/xlc.dll'] if windows else ['-L'+str(isolated/'release'),'-lxlc','-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
        row,data=run('fixture-host-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-O2','-DXL_STATIC_EMBED','-I'+str(root/'release/include'),host,*libs,'-o',binary]);assert row['Exit']==0,data[-4000:]
        child=out/('process-child'+('.exe' if windows else ''))
        row,data=run('process-child-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-O2',*(['-municode'] if windows else ['-D_GNU_SOURCE']),childSource,'-o',child]);assert row['Exit']==0,data[-4000:]
        env['XL6_PROCESS_CHILD']=str(child);(isolated/'build/test').mkdir(parents=True)
        def fixture(source):
            record=dict(Name=source.stem,Rows=[],Executed=False,Passed=False);text=source.read_text(encoding='utf-8-sig')
            if source.stem in execution.HOSTED:record.update(Status='requires-host',Reason=execution.HOSTED[source.stem]);return record
            if windows and source.stem=='stdlib_time_dst':record.update(Status='requires-platform',Reason='IANA process TZ requires Windows native transition host');return record
            if not windows and source.stem=='stdlib_process':record.update(Status='requires-platform',Reason='legacy cmd/findstr/more fixture; portable process fixtures run');return record
            directory=out/source.stem;directory.mkdir();local=dict(env)
            if source.stem=='stdlib_time_dst':local['TZ']='America/New_York'
            if 'XL6_PERSIST_FILE_ROOT' in text:files=directory/'files';files.mkdir();local['XL6_PERSIST_FILE_ROOT']=str(files)
            externs=set(re.findall(r'^extern\s+"([^"]+)"',text,re.M));support=[sidecar] if externs&{'expect_i64','expect_str'} else []
            if source.stem in fixtureSupport:support.append(fixtureSupport[source.stem])
            modes=['check'] if source.stem in inventory.NEGATIVE else ['run','module']
            guard=lock if source.stem.startswith('stdlib_process') or re.search(r'\b(file|dir)\.',text) else None
            if guard:guard.acquire()
            try:
                for mode in modes:
                    row,data=run(source.stem+'-'+mode,[binary,isolated/'release'/products[2].name,sources[source.stem],mode,directory/(mode+'-temps'),*support],environment=local)
                    record['Rows'].append(row)
                    if source.stem in inventory.NEGATIVE:
                        record.update(Status='expected-rejection',Passed=row['Exit'] not in (0,'timeout') and inventory.NEGATIVE[source.stem].encode() in data);return record
                    record['Executed']=True
                    if row['Exit']!=0:record.update(Status='failed',Diagnostic=data.decode(errors='replace')[-6000:]);return record
                record.update(Status='passed',Passed=True);return record
            finally:
                if guard:guard.release()
        with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
            for record in pool.map(fixture,cases):records.append(record);save();print(record['Name']+': '+record['Status'],flush=True)
        report['AllExecutablePassed']=all(c['Passed'] for c in records if c['Status'] not in ('requires-host','requires-platform'))
        failures.extend(dict(Name=c['Name'],Error=c.get('Diagnostic','expected rejection mismatch')) for c in records if c['Status'] not in ('requires-host','requires-platform') and not c['Passed'])
        def gate(spec):
            name,script,result,withBuild=spec;destination=out/('gate-'+name)
            command=[sys.executable,root/'tools'/script,'--root',root,'--out',destination]
            if withBuild:command+=['--build-report',a.build_report.resolve()]
            row,data=run('gate-'+name,command,directory=root,timeout=1800)
            path=destination/result if result else destination
            if not path.is_file():return dict(Name=name,Passed=False,Exit=row['Exit'],Diagnostic=data.decode(errors='replace')[-6000:])
            childReport=json.loads(path.read_text(encoding='utf-8-sig'))
            passed=row['Exit']==0 and childReport['Complete'] and childReport['InputsUnchanged'] and not childReport.get('Failures',[]) and not childReport.get('Errors',[])
            return dict(Name=name,Passed=passed,Exit=row['Exit'],Report=str(path),Sha256=sha(path),Diagnostic='' if passed else data.decode(errors='replace')[-6000:])
        with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
            for childReport in pool.map(gate,gates):children.append(childReport);save();print('gate '+childReport['Name']+': '+str(childReport['Passed']),flush=True)
        failures.extend(dict(Name='gate-'+c['Name'],Error=c['Diagnostic']) for c in children if not c['Passed'])
        headers=out/'headers/__xl_runtime';headers.mkdir(parents=True)
        for f in (root/'src/runtime').glob('*.h'):shutil.copy2(f,headers/f.name)
        for library in products[1:]:
            for opt in ('O0','O2'):
                name='path-parts-'+library.stem+'-'+opt;binary=isolated/(name+('.exe' if windows else ''))
                libs=[isolated/'release'/library.name] if windows else ['-L'+str(isolated/'release'),'-l'+library.stem[3:],'-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
                row,data=run(name+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-'+opt,'-DXL_STATIC_EMBED',
                    '-I'+str(out/'headers'),'-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),pathHost,*libs,'-o',binary])
                if row['Exit']==0:
                    row,data=run(name+'-run',[binary])
                    if row['Exit']==0 and b'complete OOM budgets' in data:continue
                failures.append(dict(Name=name,Error=data.decode(errors='replace')[-6000:]))
        cli=isolated/'release'/products[0].name;image=out/('path-parts'+('.exe' if windows else ''))
        row,data=run('path-parts-aot-build',[cli,'build',sources[pathFixture.stem],'-o',image],timeout=600)
        if row['Exit']==0:row,data=run('path-parts-aot-run',[image])
        if row['Exit']!=0:failures.append(dict(Name='path-parts-aot',Error=data.decode(errors='replace')[-6000:]))
        report['HostedProductAcceptance']=all(c['Passed'] for c in children if c['Name'] in ('graphs','clear'))
        report['AotGraphRetirementAcceptance']=any(c['Name']=='aot-graphs' and c['Passed'] for c in children)
        report['TerminalReadinessAcceptance']=any(c['Name']=='terminal' and c['Passed'] for c in children)
        report['NativeTerminalLedgerAcceptance']=any(c['Name']=='terminal-ledger' and c['Passed'] for c in children)
        report['WorkerBodyFaultPrefixAcceptance']=any(c['Name']=='worker-fault' and c['Passed'] for c in children)
        report['UnexecutedRequirements']=[c['Name'] for c in records if c['Status']=='requires-platform']
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        report['Complete']=report['InputsUnchanged'] and report['AllExecutablePassed'] and len(children)==len(gates) and not failures
    finally:report['PhaseCount']=len(rows);save()
    print(json.dumps(dict(Complete=report['Complete'],Failures=failures,Unexecuted=report.get('UnexecutedRequirements',[]))),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
