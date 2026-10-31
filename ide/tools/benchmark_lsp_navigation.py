"""Pair full diagnostic -> first hover -> repeated hover chains over stdio.

Retains every sample,standard result,CPU/cycle/memory observation and shutdown
cost. First-hover regressions are evaluated explicitly;shorter diagnostic
responses cannot hide work transferred to the next interaction. This does not
replace the original 64 analysis gates,GUI or whole-service acceptance.
"""
from pathlib import Path
import argparse,json,os,sys,time
from benchmark_lsp_analysis import Counters,distribution,make_body,sha
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import initialize,checked
from check_language_definitions import position


METRICS=('checkMs','firstHoverMs','repeatHoverMs','chainMs','cpuMs','cycles')


def summarize(rows,cases):
    return {case:{phase:{variant:{metric:distribution([row[metric] for row in rows if row['case']==case and row['phase']==phase and row['variant']==variant]) for metric in METRICS} for variant in ['baseline','candidate']} for phase in ['cold','first','hot','exact']} for case in cases}


def evaluate(rows,cases):
    summary=summarize(rows,cases);gates=[]
    def gate(name,actual,limit,**details):gates.append(dict(name=name,actual=actual,limit=limit,passed=actual<=limit,**details))
    for case,phases in summary.items():
        for phase,data in phases.items():
            for metric in ['checkMs','firstHoverMs','repeatHoverMs','chainMs']:
                old,new=[data[v][metric] for v in ['baseline','candidate']]
                gate(case+'/'+phase+'/'+metric+'/P50',new['p50'],max(old['p50']*1.05,old['p50']+.5),baseline=old['p50'])
                if phase=='hot':gate(case+'/hot/'+metric+'/P95',new['p95'],max(old['p95']*1.05,old['p95']+.5),baseline=old['p95'])
            if phase=='hot':
                old,new=[data[v]['cpuMs']['p50'] for v in ['baseline','candidate']];gate(case+'/hot/chainCpuP50',new,old*1.05,baseline=old)
        for metric in ['firstHoverMs','repeatHoverMs']:
            value=phases['hot']['candidate'][metric]['p95'];gate(case+'/hot/'+metric+'/cachedAbsoluteP95',value,10)
        old,new=[max(row['memory']['private'] for row in rows if row['case']==case and row['variant']==v) for v in ['baseline','candidate']];gate(case+'/privateExtra',new-old,192*1024*1024,baseline=old,candidate=new)
    return summary,gates


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--candidate',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--pairs',type=int,default=8);p.add_argument('--generations',type=int,default=14);p.add_argument('--functions',default='32,512');p.add_argument('--initial-available-gib',type=float,default=5);p.add_argument('--minimum-available-gib',type=float,default=3);p.add_argument('--functional-only',action='store_true');a=p.parse_args()
    counts=[int(n) for n in a.functions.split(',')];assert counts and len(counts)==len(set(counts)) and all(n>0 for n in counts) and a.pairs>0 and a.generations>=3
    servers={v:path.resolve(strict=True) for v,path in [('baseline',a.baseline),('candidate',a.candidate)]};out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);cases={f'{count}-{topology}-{edit}':(count,topology,edit) for count in counts for topology in ['single','imported'] for edit in ['literal','declaration']}
    protected={str(x):sha(x) for x in [*servers.values(),Path(__file__).resolve(),*[Path(__file__).with_name(name) for name in ['benchmark_lsp_analysis.py','check_lsp_automatic_diagnostics.py','check_lsp_module_semantics.py','check_language_snapshots.py','check_language_definitions.py']]]};prepared={}
    for case,(count,topology,edit) in cases.items():
        folder=out/case;folder.mkdir();entry=folder/'main.xl';dep=entry if topology=='single' else folder/'dep.xl';entry.write_text('// overlays only\n',encoding='utf8');protected[str(entry)]=sha(entry);prepared[case]=(folder,entry,dep)
    plan=dict(accepted=False,servers={v:dict(path=str(path),sha256=sha(path)) for v,path in servers.items()},pairs=a.pairs,generations=list(range(a.generations))+[a.generations-1],cases=cases,expectedChains=len(cases)*a.pairs*2*(a.generations+1),expectedProcesses=len(cases)*a.pairs*2,requestOrder=['check','firstHover','repeatHover'],order='AB/BA by pair and generation;body/position generation outside timing',functionalOnly=a.functional_only,initialAvailableGiB=a.initial_available_gib,duringAvailableGiB=a.minimum_available_gib,thresholds='Unchanged5percent/.5ms tolerance for all wire metrics;hot CPU5percent;first/repeated cached hover hotP95<=10ms;privateExtra<=192MiB. New chain gates supplement,never replace original analysis gates.',limits='No filtering of slow successful rows. First/exact/cold have only pairs samples per side;not final tail proof. CPU is quantized. Per-chain and process-shutdown work reported separately. No own concurrent builds,QA or bulk hashing during formal requests;host not exclusive.')
    plan_path=out/'plan.json';plan_path.write_text(json.dumps(plan,indent=2)+'\n',encoding='utf8',newline='\n');record=dict(status='running',accepted=False,scope=__doc__,plan=str(plan_path),planSha256=sha(plan_path),protected=protected,resources=[],queries=[],processes=[]);counter=Counters();answers={}
    def save():(out/'result.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf8',newline='\n')
    removed={key:os.environ.pop(key,None) for key in ['XLS_CHECK_PHASE_TRACE','XLS_FUNCTION_STAGE_VALIDATE']}
    try:
        counter.qualify(record['resources'],a.initial_available_gib,phase='initial');save()
        for case,(count,topology,edit) in cases.items():
            folder,entry,dep=prepared[case]
            for pair in range(a.pairs):
                clients={};last_cpu={}
                try:
                    for variant in ['baseline','candidate'][::1 if pair%2==0 else -1]:
                        counter.qualify(record['resources'],a.minimum_available_gib,phase='process',case=case,pair=pair,variant=variant);c=AutomaticClient(servers[variant],out/'absent/xl.exe',folder);clients[variant]=c;initialize(c,entry,[entry] if topology=='single' else [entry,dep],folder)
                        if topology=='imported':c.open(entry,'import dep\nfunc main(...) as int\n return dep.compute(3)\nend\n',1)
                    for step,generation in enumerate(plan['generations']):
                        repeat=step==a.generations;phase='exact' if repeat else 'cold' if not generation else 'first' if generation==1 else 'hot';body=make_body(count,generation,topology,edit);point=position(body,body.rindex('identity(value)')+len('identity('));params=dict(textDocument=dict(uri=dep.as_uri()),version=generation+1,position=point)
                        for variant in ['baseline','candidate'][::1 if (pair+step)%2==0 else -1]:
                            c=clients[variant];counter.qualify(record['resources'],a.minimum_available_gib,phase='check',case=case,pair=pair,variant=variant,generation=generation,repeat=repeat);cpu,cycles=counter.cpu(c);began=time.perf_counter_ns()
                            if not repeat:
                                if generation:c.change(dep,body,generation+1)
                                else:c.open(dep,body,1)
                            value=checked(c,entry,generation+1 if topology=='single' else 1);after_check=time.perf_counter_ns();assert value['ok'] and not value['count'] and value['semanticDefinitions'] and value['checkId']==c.request;params['checkId']=value['checkId']
                            counter.qualify(record['resources'],a.minimum_available_gib,phase='first-hover',case=case,pair=pair,variant=variant,generation=generation);first_start=time.perf_counter_ns();first=c.call('textDocument/hover',params);first_end=time.perf_counter_ns()
                            counter.qualify(record['resources'],a.minimum_available_gib,phase='repeat-hover',case=case,pair=pair,variant=variant,generation=generation);repeat_start=time.perf_counter_ns();again=c.call('textDocument/hover',params);finished=time.perf_counter_ns();after_cpu,after_cycles=counter.cpu(c);last_cpu[variant]=after_cpu;assert first and first==again
                            facts={key:v for key,v in value.items() if key not in ['analysis','durationMs','checkId']};key=(case,generation,repeat)
                            if key in answers:assert answers[key]==(facts,first),(key,variant,'standard result mismatch')
                            else:answers[key]=(facts,first)
                            record['queries'].append(dict(case=case,pair=pair,variant=variant,generation=generation,phase=phase,checkMs=(after_check-began)/1e6,firstHoverMs=(first_end-first_start)/1e6,repeatHoverMs=(finished-repeat_start)/1e6,chainMs=(finished-began)/1e6,cpuMs=after_cpu-cpu,cycles=after_cycles-cycles,memory=counter.memory(c),result=value,hover=first))
                finally:
                    active_error=sys.exc_info()[1];failures=[]
                    for variant,c in clients.items():
                        errors=[];started=time.perf_counter();stderr=''
                        try:stderr=c.stop()
                        except BaseException as exc:
                            errors.append(repr(exc))
                            if c.process.poll() is None:
                                try:c.process.kill();c.process.wait(timeout=5)
                                except BaseException as kill_error:errors.append(repr(kill_error))
                        elapsed=(time.perf_counter()-started)*1000;total_cpu=None
                        try:total_cpu,_=counter.cpu(c)
                        except BaseException as exc:errors.append(repr(exc))
                        sp=folder/f'{pair}-{variant}-stderr.log';tp=folder/f'{pair}-{variant}-transcript.json'
                        try:sp.write_text(stderr,encoding='utf8');tp.write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf8')
                        except BaseException as exc:errors.append(repr(exc))
                        requests=sum(m.get('send',{}).get('method') in ['xlang/check','textDocument/hover'] for m in c.transcript)
                        record['processes'].append(dict(case=case,pair=pair,variant=variant,pid=c.process.pid,exit=c.process.returncode,cleanupErrors=errors,stderr=str(sp),transcript=str(tp),queryRequests=requests,stopMs=elapsed,totalProcessCpuMs=total_cpu,afterLastResponseCpuMs=None if total_cpu is None or variant not in last_cpu else total_cpu-last_cpu[variant]));failures+=errors
                        if c.process.returncode or stderr:failures.append((variant,c.process.returncode,stderr[-1000:]))
                    save()
                    if failures and active_error is None:raise RuntimeError('Process cleanup/output failure: '+repr(failures))
                print(case,pair,'pair complete',flush=True)
        assert len(record['queries'])==plan['expectedChains'] and len(record['processes'])==plan['expectedProcesses']
        for row in record['processes']:row.update(stderrSha256=sha(row['stderr']),transcriptSha256=sha(row['transcript']))
        assert sha(plan_path)==record['planSha256'] and all(sha(path)==digest for path,digest in protected.items())
        if a.functional_only:record['summary']=summarize(record['queries'],cases);record['gates']=[];record['status']='functional-only-passed'
        else:record['summary'],record['gates']=evaluate(record['queries'],cases);record['status']='gates-failed' if any(not g['passed'] for g in record['gates']) else 'passed'
        record['queryRequests']=sum(row['queryRequests'] for row in record['processes']);record['checkRetries']=record['queryRequests']-3*len(record['queries']);assert record['checkRetries']>=0
        print(json.dumps(dict(status=record['status'],chains=len(record['queries']),requests=record['queryRequests'],checkRetries=record['checkRetries'],processes=len(record['processes']),failedGates=sum(not g['passed'] for g in record['gates']))),flush=True)
    except BaseException as exc:record.update(status='failed',error=repr(exc));raise
    finally:
        for key,value in removed.items():
            if value is not None:os.environ[key]=value
        save()
    if record['status']=='gates-failed':raise SystemExit(2)


if __name__=='__main__':main()
