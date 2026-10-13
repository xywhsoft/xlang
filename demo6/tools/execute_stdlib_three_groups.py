"""Execute the complete three-group fixture inventory with an exact-source
public compiler. Dedicated-host and platform-specific fixtures remain explicit
coverage requirements, never counted as executed passes. Serializes filesystem
and process fixtures, launches only the controlled subprocess helper, and pins
inputs, logs and products. This is discovery, not full53/release acceptance.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,re,shutil,subprocess,threading,time
from discover_stdlib_three_groups import PREFIX,NEGATIVE
from check_stdlib_slot_map import stage_private_sdk
from xlir_public_build_inputs import verify_public_build

HOSTED={
 'stdlib_concurrent_queue_continuation':'actual Promise completion/continuation host',
 'stdlib_concurrent_queue_ownership':'registered graph capture/retirement host',
 'stdlib_stack_ownership':'registered graph capture/retirement host',
 'stdlib_tree_ownership':'registered graph capture/retirement host',
 'stdlib_stack_clear_error':'native destruction/error injection host',
 'stdlib_stack_clear_error_continuation':'native destruction/error injection host',
}
FIXTURE_C_SUPPORT={'stdlib_buffer_class_surface':'stdlib_buffer_surface_support.c'}
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','public-directory','out'):p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--jobs',type=int,default=2);p.add_argument('--mode',choices=('run','module','both'),default='run')
    p.add_argument('--fixture',action='append');a=p.parse_args()
    root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve();cc=shutil.which('gcc')
    assert cc and not out.exists() and 1<=a.jobs<=8
    build,pinned,_=verify_public_build(root,public);driver=Path(build['Driver'])
    sources=sorted(s for s in (root/'test/cases').glob('stdlib_*.xl') if PREFIX.match(s.name))
    all_count=len(sources)
    if a.fixture:
        wanted=set(a.fixture);sources=[s for s in sources if s.stem in wanted];assert len(sources)==len(wanted)
    manifest,sdk,_=stage_private_sdk(root)
    child_source=root/'test/cases/xlir_process_child.c';expect=root/'test/cases/stdlib_string_support.c'
    pinned.update((Path(__file__).resolve(),Path(cc),child_source,expect,manifest,*sources))
    pinned.update(root/'test/cases'/name for name in FIXTURE_C_SUPPORT.values())
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    pinned.add(runtime)
    pinned.update(Path(row[k]) for row in sdk for k in ('Source','Path'))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(pinned)]
    out.mkdir();env=dict(os.environ);ext='.exe' if os.name=='nt' else ''
    if os.name!='nt':env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    child=out/('process-child'+ext);mutable=threading.Lock();rows=[]
    report=dict(Complete=False,InputsUnchanged=False,AllExecutablePassed=False,Scope=__doc__,Inputs=inputs,Cases=[],
        CandidateCount=all_count,SelectedCount=len(sources),ExplicitSubset=bool(a.fixture),Mode=a.mode,
        Full53Acceptance=False,ShippingArtifact=False,FunctionCoverageAcceptance=False)
    def command(directory,name,argv,timeout=90,environment=None):
        start=time.monotonic()
        try:
            r=subprocess.run(list(map(str,argv)),cwd=root,env=environment or env,capture_output=True,timeout=timeout)
            data,code=r.stdout+r.stderr,r.returncode
        except subprocess.TimeoutExpired as e:data,code=(e.stdout or b'')+(e.stderr or b''),'timeout'
        log=directory/(name+'.log');log.write_bytes(data)
        return dict(Name=name,Command=list(map(str,argv)),Exit=code,Seconds=time.monotonic()-start,Log=str(log),LogSha256=sha(log)),data
    row,data=command(out,'process-child-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-O2',*(['-municode'] if os.name=='nt' else ['-D_GNU_SOURCE']),child_source,'-o',child]);rows.append(row)
    assert row['Exit']==0,data;report['Preparation']=rows;report['Child']=dict(Path=str(child),Sha256=sha(child))
    env['XL6_PROCESS_CHILD']=str(child)
    (root/'build/test').mkdir(parents=True,exist_ok=True)
    def execute(source):
        directory=out/source.stem;directory.mkdir();text=source.read_text(encoding='utf-8-sig')
        record=dict(Name=source.stem,Source=str(source),SourceSha256=sha(source),Rows=[],Executed=False,Passed=False)
        if source.stem in HOSTED:
            record.update(Status='requires-host',Reason=HOSTED[source.stem]);return record
        if source.stem=='stdlib_process_shell_quotes':
            record.update(Status='requires-product',Reason='explicit release/xl.exe check helper; requires separately verified CLI product');return record
        if os.name=='nt' and source.stem=='stdlib_time_dst':
            record.update(Status='requires-platform',Reason='IANA process TZ fixture; Windows explicit transition coverage requires a separate native host');return record
        if os.name!='nt' and source.stem=='stdlib_process':
            record.update(Status='requires-platform',Reason='legacy Windows cmd/findstr/more fixture; portable process fixtures execute separately');return record
        externs=set(re.findall(r'^extern\s+"([^"]+)"',text,re.M))
        local_env=dict(env)
        if source.stem=='stdlib_time_dst':local_env['TZ']='America/New_York'
        if 'XL6_PERSIST_FILE_ROOT' in text:
            files=directory/'files';files.mkdir();local_env['XL6_PERSIST_FILE_ROOT']=str(files)
        sidecars=[expect] if externs&{'expect_i64','expect_str'} else []
        if source.stem in FIXTURE_C_SUPPORT:sidecars.append(root/'test/cases'/FIXTURE_C_SUPPORT[source.stem])
        modes=['cgen-default'] if source.stem in NEGATIVE else (['run','module'] if a.mode=='both' else [a.mode])
        lock=mutable if source.stem.startswith('stdlib_process') or re.search(r'\b(file|dir)\.',text) else None
        if lock:lock.acquire()
        try:
            for mode in modes:
                row,data=command(directory,mode,[driver,source,mode,directory/mode,*sidecars],environment=local_env);record['Rows'].append(row)
                if source.stem in NEGATIVE:
                    record.update(Status='expected-rejection',Passed=row['Exit'] not in (0,'timeout') and NEGATIVE[source.stem].encode() in data)
                    return record
                record['Executed']=True
                if row['Exit']!=0:
                    record.update(Status='failed',FailedStage=mode,Diagnostic=data.decode(errors='replace').strip());return record
            record.update(Status='passed',Passed=True);return record
        finally:
            if lock:lock.release()
    def save():(out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
            for record in pool.map(execute,sources):
                report['Cases'].append(record);save();print(record['Name']+': '+record['Status'],flush=True)
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        report['Complete']=report['InputsUnchanged'] and len(report['Cases'])==len(sources)
        report['AllExecutablePassed']=report['Complete'] and all(c['Passed'] for c in report['Cases'] if c['Status'] not in ('requires-host','requires-platform','requires-product'))
    finally:
        report['PhaseCount']=sum(len(c['Rows']) for c in report['Cases'])+len(rows);save()
    failures=[c['Name'] for c in report['Cases'] if c['Status']=='failed']
    report['Failures']=failures;report['UnexecutedRequirements']=[c['Name'] for c in report['Cases'] if c['Status'] in ('requires-host','requires-platform','requires-product')];save()
    print(json.dumps(dict(Complete=report['Complete'],AllExecutablePassed=report['AllExecutablePassed'],Failures=failures,Unexecuted=report['UnexecutedRequirements'])),flush=True)
    return 0 if report['AllExecutablePassed'] else 1
if __name__=='__main__':raise SystemExit(main())
