"""Batch three-group language fixtures through exact-source Cgen and strict C.

Read-only language compilation; no fixture execution or subprocess/GUI launch.
Every selected failure is recorded, not waived. This discovers regressions;
it does not prove function coverage, native execution or full53/shipping readiness.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,re,shutil,subprocess,time
from xlir_public_build_inputs import verify_public_build

PREFIX=re.compile(r'^stdlib_(time|environment|signal|process|number|text|string|unicode|html|charset|encoding|pattern|regex|queue|wait_queue|concurrent_queue|special_collections|array|list|buffer|ptr_array|slot_map|avl|tree|ordered_map|int_map|dict|map|set|fixed_stack|ptr_fixed_stack|stack|block_stack|ptr_stack|containers|spsc|mpsc|mpmc)(_|\.)')
NEGATIVE={'stdlib_stack_wire_child_refused':'[sem] typed container item type must be copyable'}
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--jobs',type=int,default=4)
    parser.add_argument('--fixture',action='append',help='Explicit subset; reported as subset, never whole discovery')
    args=parser.parse_args();root,public,out=args.root.resolve(),args.public_directory.resolve(),args.out.resolve()
    assert 1<=args.jobs<=8 and not out.exists();cc=shutil.which('gcc');assert cc
    build,pinned,compared=verify_public_build(root,public);driver=Path(build['Driver'])
    candidates=sorted(source for source in (root/'test/cases').glob('stdlib_*.xl') if PREFIX.match(source.name))
    if args.fixture:
        names=set(args.fixture);sources=[source for source in candidates if source.stem in names]
        assert len(sources)==len(names),'Unknown or out-of-scope fixture'
    else:sources=candidates
    assert sources
    pinned.update((Path(__file__).resolve(),Path(cc),*sources))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir();env=dict(os.environ)
    if os.name!='nt':env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    flags=['-std=c11','-Wall','-Wextra','-Werror','-fsyntax-only','-DXL_STATIC_EMBED',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(public/'include'),
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    def command(directory,name,argv):
        argv=list(map(str,argv));started=time.monotonic()
        try:
            result=subprocess.run(argv,cwd=root,env=env,capture_output=True,timeout=120)
            data,code=result.stdout+result.stderr,result.returncode
        except subprocess.TimeoutExpired as error:
            data,code=(error.stdout or b'')+(error.stderr or b''),'timeout'
        log=directory/(name+'.log');log.write_bytes(data)
        return dict(Name=name,Command=argv,Exit=code,Seconds=time.monotonic()-started,Log=str(log),LogSha256=sha(log)),data
    def execute(source):
        directory=out/source.stem;directory.mkdir();generated=directory/'program.c'
        record=dict(Name=source.stem,Source=str(source),SourceSha256=sha(source),Passed=False,Rows=[])
        row,data=command(directory,'cgen',[driver,source,'cgen-default',generated]);record['Rows'].append(row)
        if source.stem in NEGATIVE:
            record['ExpectedRejection']=True
            record['Passed']=row['Exit'] not in (0,'timeout') and NEGATIVE[source.stem].encode() in data and (not generated.exists() or not generated.stat().st_size)
            record['Diagnostic']=data.decode(errors='replace').strip()
            if not record['Passed']:record['FailedStage']='negative-contract'
            return record
        if row['Exit']!=0:
            record.update(FailedStage='cgen',Diagnostic=data.decode(errors='replace').strip());return record
        record['Generated']=dict(Path=str(generated),Sha256=sha(generated))
        row,data=command(directory,'strict-c',[cc,*flags,generated]);record['Rows'].append(row)
        if row['Exit']!=0:record.update(FailedStage='strict-c',Diagnostic=data.decode(errors='replace').strip())
        else:record['Passed']=True
        return record
    report=dict(Complete=False,InputsUnchanged=False,AllPassed=False,Scope=__doc__,Inputs=inputs,
        Cases=[],CandidateCount=len(candidates),SelectedCount=len(sources),ExplicitSubset=bool(args.fixture),
        SourceInputsMatch=True,ProductionFiles=len(compared),ReadOnlyLanguageCompilation=True,
        NativeExecutionAcceptance=False,Full53Acceptance=False,ShippingArtifact=False)
    def save():
        (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            for record in pool.map(execute,sources):
                report['Cases'].append(record);save()
                print(record['Name']+': '+('pass' if record['Passed'] else record['FailedStage']),flush=True)
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        report['Complete']=report['InputsUnchanged'] and len(report['Cases'])==len(sources)
        report['AllPassed']=report['Complete'] and all(case['Passed'] for case in report['Cases'])
    finally:
        report['PhaseCount']=sum(len(case['Rows']) for case in report['Cases']);save()
    failures=[case['Name'] for case in report['Cases'] if not case['Passed']]
    print(json.dumps(dict(Complete=report['Complete'],AllPassed=report['AllPassed'],Selected=len(sources),Failures=failures)),flush=True)
    return 0 if report['AllPassed'] else 1
if __name__=='__main__':raise SystemExit(main())
