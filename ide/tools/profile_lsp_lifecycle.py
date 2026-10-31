"""Compare a private lifecycle image with its unchanged normal LS over stdio.

Timings are inclusive diagnostic samples. They are not formal performance
gates, IDE latency, or evidence that a retirement ordinal belongs to a query.
"""
from pathlib import Path
import argparse
import collections
import io
import json
import math
import shutil
import threading
import time

from build_lsp_lifecycle_profile import read, sha, verify_build
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import initialize, checked
from benchmark_lsp_analysis import make_body, distribution


def frames(text, labels):
    """Reject malformed, truncated, unknown, duplicated or reordered frames."""
    def unique(pairs):
        value={}
        for key,item in pairs:
            if key in value:raise ValueError('duplicate trace key: '+key)
            value[key]=item
        return value
    result=[];sequences={}
    for line in text.splitlines():
        if not line.startswith('XLS_LIFECYCLE '):
            raise ValueError('unexpected diagnostic stderr: '+line[:160])
        value=json.loads(line.removeprefix('XLS_LIFECYCLE '),object_pairs_hook=unique)
        if set(value)!={'group','thread','sequence','cells'}:
            raise ValueError('unknown trace envelope')
        group=value['group'];thread=value['thread'];sequence=value['sequence']
        if group not in ('check','snapshot','retire') or type(thread)!=int or thread<=0 or type(sequence)!=int or sequence<=sequences.get(thread,0):
            raise ValueError('invalid thread,group or sequence')
        sequences[thread]=sequence
        cells=value['cells']
        if not isinstance(cells,dict) or not cells:
            raise ValueError('missing trace cells')
        for label,cell in cells.items():
            if label not in labels or not isinstance(cell,dict) or set(cell)!={'ms','calls'}:
                raise ValueError('unknown trace cell: '+label)
            if type(cell['calls'])!=int or cell['calls']<=0 or type(cell['ms']) not in (int,float) or not math.isfinite(cell['ms']) or cell['ms']<0:
                raise ValueError('invalid trace measurement')
        if {'check':'worker.total','snapshot':'adapter.total','retire':'retire.total'}[group] not in cells:
            raise ValueError('missing completed outer scope')
        result.append(value)
    return result


def review(runs, labels):
    groups=collections.defaultdict(list);details=[]
    for run in runs:
        if run['variant']!='diagnostic':continue
        records=frames(Path(run['stderr']).read_text('utf8'),labels)
        counts=collections.Counter(record['group'] for record in records)
        if counts['check']!=15 or counts['snapshot']!=15 or counts['retire']<14:
            raise ValueError('incomplete diagnostic frame counts: '+str(counts))
        ordinals=collections.Counter()
        for record in records:
            group=record['group'];ordinal=ordinals[group];ordinals[group]+=1
            # Retire ordering is deliberately not assigned to a query phase.
            phase='all-retirements' if group=='retire' else 'cold' if ordinal==0 else 'first' if ordinal==1 else 'exact' if ordinal==14 else 'hot'
            for label,cell in record['cells'].items():
                groups[(run['case'],group,phase,label)].append(cell['ms'])
        details.append(dict(case=run['case'],pair=run['pair'],counts=dict(counts),frames=records))
    summaries=[dict(case=case,group=group,phase=phase,label=label,**distribution(values))
               for (case,group,phase,label),values in sorted(groups.items())]
    return dict(accepted=False,scope='Inclusive diagnostic timings;parent and child overlap;retirement order is not a request ID;small samples do not qualify performance.',summaries=summaries,traces=details)


def verify_profile(build):
    if build['status']!='built':raise ValueError('private profile build is incomplete')
    for path,digest in {**build['inputs'],**build['outputs']}.items():
        if sha(path)!=digest:raise ValueError('private profile input/output changed: '+path)
    root=Path(build['normalBuild']);verify_build(read(root/'build-result.json'),root)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile',type=Path,required=True,help='private passed profile build directory')
    parser.add_argument('--output',type=Path,required=True,help='fresh evidence directory')
    parser.add_argument('--functions',type=int,default=512)
    parser.add_argument('--pairs',type=int,default=3)
    args=parser.parse_args()
    if not 4<=args.functions<=4096 or not 1<=args.pairs<=16:
        parser.error('functions must be 4..4096; pairs must be 1..16')
    profile=args.profile.resolve(strict=True);build=read(profile/'result.json');verify_profile(build)
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    protected={str(profile/'result.json'):sha(profile/'result.json')}
    for name in ('build_lsp_lifecycle_profile.py','profile_lsp_lifecycle.py','benchmark_lsp_analysis.py','check_native_bridge_catalog.py','check_lsp_automatic_diagnostics.py','check_lsp_module_semantics.py','check_language_snapshots.py','check_language_definitions.py'):
        path=Path(__file__).with_name(name).resolve();protected[str(path)]=sha(path)
    report=dict(status='running',accepted=False,scope=__doc__.strip(),functions=args.functions,pairs=args.pairs,queries=0,protected=protected,runs=[])
    def save():(out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8',newline='\n')
    save()
    try:
        answers={}
        for topology in ('single','imported'):
            for edit in ('literal','declaration'):
                case=f'{args.functions}-{topology}-{edit}';folder=out/case;folder.mkdir()
                entry=folder/'main.xl';dep=entry if topology=='single' else folder/'dep.xl'
                for path in dict.fromkeys((entry,dep)):
                    path.write_text('// immutable disk; editor overlays supply source\n',encoding='utf8')
                for pair in range(args.pairs):
                    for variant in ('normal','diagnostic')[::1 if pair%2==0 else -1]:
                        program=build['programs'][variant];exe=Path(program['path'])
                        if sha(exe)!=program['sha256']:raise ValueError('program changed: '+str(exe))
                        client=AutomaticClient(exe,out/'absent/xl.exe',folder)
                        stream=client.process.stderr;errors=[];name=f'{case}-{pair}-{variant}'
                        log=out/(name+'-stderr.log');rows=[]
                        def drain():
                            try:
                                with log.open('wb') as target:shutil.copyfileobj(stream,target)
                            except BaseException as error:errors.append(repr(error))
                        reader=threading.Thread(target=drain,daemon=True);reader.start()
                        client.process.stderr=io.BytesIO()
                        try:
                            initialize(client,entry,[entry] if dep==entry else [entry,dep],folder)
                            if topology=='imported':client.open(entry,'import dep\nfunc main(...) as int\n return dep.compute(3)\nend\n',1)
                            for step in range(15):
                                generation=min(step,13);repeat=step==14
                                body=make_body(args.functions,generation,topology,edit);begin=time.perf_counter()
                                if not repeat:
                                    if generation:client.change(dep,body,generation+1)
                                    else:client.open(dep,body,1)
                                value=checked(client,entry,generation+1 if dep==entry else 1)
                                elapsed=(time.perf_counter()-begin)*1000
                                if not value['ok'] or value['count'] or value['checkId']!=client.request:
                                    raise ValueError('native check/result identity failed: '+str(value))
                                facts={k:v for k,v in value.items() if k not in ('analysis','durationMs','checkId')}
                                key=(case,generation)
                                if key in answers and answers[key]!=facts:
                                    raise ValueError('normal/instrumented result mismatch: '+str(key))
                                answers[key]=facts;rows.append(dict(generation=generation,repeat=repeat,ms=elapsed,result=value))
                                report['queries']+=1
                        finally:
                            client.stop();reader.join(5)
                            (out/(name+'-transcript.json')).write_text(json.dumps(client.transcript,indent=2)+'\n',encoding='utf8',newline='\n')
                            report['runs'].append(dict(case=case,pair=pair,variant=variant,pid=client.process.pid,exit=client.process.returncode,results=rows,stderr=str(log),stderrReaderErrors=errors,stderrReaderAlive=reader.is_alive()));save()
                        if client.process.returncode or reader.is_alive() or errors:
                            raise ValueError('server/trace cleanup failed: '+name)
                        if variant=='normal' and log.read_bytes():raise ValueError('normal oracle wrote unexpected stderr')
                        print(name+' passed',flush=True)
        reviewed=review(report['runs'],set(build['labels']))
        (out/'review.json').write_text(json.dumps(reviewed,indent=2)+'\n',encoding='utf8',newline='\n')
        verify_profile(build)
        for path,digest in protected.items():
            if sha(path)!=digest:raise ValueError('protected input changed: '+path)
        report.update(status='passed',review=str(out/'review.json'),reviewSha256=sha(out/'review.json'))
    except BaseException as error:
        report.update(status='failed',error=repr(error));raise
    finally:save()


if __name__=='__main__':main()
