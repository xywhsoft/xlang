"""Verify real pure postpass reuse over stdio; not complete final-IR incrementality.

Uses open overlays, two edit shapes and two module topologies. The optional
reference image checks all standard result facts in the same source directory.
"""
from pathlib import Path
import argparse
import hashlib
import json

from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import initialize,checked
from benchmark_lsp_analysis import make_body


def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--reference',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();server=args.server.resolve(strict=True);reference=args.reference.resolve(strict=True) if args.reference else None
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    protected={str(server):sha(server),str(Path(__file__).resolve()):sha(__file__)}
    if reference:protected[str(reference)]=sha(reference)
    for name in ('check_lsp_automatic_diagnostics.py','check_lsp_module_semantics.py','benchmark_lsp_analysis.py','check_language_snapshots.py','check_language_definitions.py'):
        path=Path(__file__).with_name(name).resolve();protected[str(path)]=sha(path)
    report=dict(status='running',accepted=False,scope=__doc__.strip(),protected=protected,queries=0,processes=[],cases=[])
    def save():(out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8',newline='\n')
    try:
        for count in (32,128):
            for topology in ('single','imported'):
                for edit in ('literal','declaration'):
                    case=f'{count}-{topology}-{edit}';folder=out/case;folder.mkdir();entry=folder/'main.xl';dep=entry if topology=='single' else folder/'dep.xl'
                    for path in dict.fromkeys((entry,dep)):path.write_text('// open overlays are authoritative\n',encoding='utf8')
                    clients=[];values=[]
                    try:
                        for variant,program in [('candidate',server)]+([('reference',reference)] if reference else []):
                            client=AutomaticClient(program,out/'absent/xl.exe',folder);clients.append((variant,client))
                            initialize(client,entry,[entry] if dep==entry else [entry,dep],folder)
                            if topology=='imported':client.open(entry,'import dep\nfunc main(...) as int\n return dep.compute(3)\nend\n',1)
                        exact=None
                        for step in range(5):
                            generation=min(step,3);body=make_body(count,generation,topology,edit);answers=[]
                            for variant,client in clients:
                                if step<4:
                                    if generation:client.change(dep,body,generation+1)
                                    else:client.open(dep,body,1)
                                value=checked(client,entry,generation+1 if dep==entry else 1);report['queries']+=1
                                assert value['ok'] and not value['count'] and value['checkId']==client.request,value
                                answers.append({k:v for k,v in value.items() if k not in ('analysis','durationMs','checkId')})
                                if variant=='candidate':
                                    raw=value['analysis']['irFragments'];work=value['analysis']['irDerived']
                                    assert work['contexts']==2 and 0<work['bytes']<=raw['bytes']<=32*1024*1024,(case,step,raw,work)
                                    if not step:assert not work['reused'] and not work['nodesSkipped'] and work['captured']>0,(case,step,work)
                                    elif step<4:
                                        assert work['reused']>0 and work['nodesSkipped']>=raw['nodesReused'] and work['rowsApplied']>0,(case,step,raw,work)
                                    else:assert work==exact,(case,step,work,exact)
                                    if step==3:exact=work.copy()
                                    values.append(dict(step=step,version=generation+1,raw=raw,derived=work))
                            if reference:assert answers[0]==answers[1],(case,step,answers)
                    finally:
                        for variant,client in clients:
                            stderr=client.stop();report['processes'].append(dict(case=case,variant=variant,pid=client.process.pid,exit=client.process.returncode,stderr=stderr))
                            (out/(case+'-'+variant+'-transcript.json')).write_text(json.dumps(client.transcript,indent=2)+'\n',encoding='utf8',newline='\n')
                        save()
                    assert all(not row['exit'] and not row['stderr'] for row in report['processes'] if row['case']==case),case
                    report['cases'].append(dict(name=case,passed=True,generations=values));save();print(case+' passed',flush=True)
        for path,digest in protected.items():assert sha(path)==digest,path
        report['status']='passed'
    except BaseException as error:report.update(status='failed',error=repr(error));raise
    finally:save()


if __name__=='__main__':main()
