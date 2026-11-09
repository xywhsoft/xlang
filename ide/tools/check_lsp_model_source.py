"""Check real imported-declaration model reuse without an eager whole-query retry.

This is a deterministic work-count/wire-fact regression,not a latency gate.
The native fixture independently compares every AST,index and IR fact.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
from benchmark_lsp_analysis import make_body,sha
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import initialize,checked


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--baseline',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();output=args.output.resolve();output.mkdir(parents=True,exist_ok=False)
    programs={name:path.resolve(strict=True) for name,path in [('baseline',args.baseline),('candidate',args.server)]}
    protected={str(path):sha(path) for path in [*programs.values(),Path(__file__)]}
    entry=output/'main.xl';dep=output/'dep.xl'
    for path in [entry,dep]:path.write_text('// overlay is authoritative\n',encoding='utf8',newline='\n')
    facts={};runs=[];report=dict(status='failed',accepted=False,scope=__doc__,protected=protected,runs=runs)
    try:
        for mode,program in programs.items():
            c=AutomaticClient(program,output/'absent/xl.exe',output);row=dict(mode=mode,queries=[]);runs.append(row)
            try:
                initialize(c,entry,[entry,dep],output)
                c.open(entry,'import dep\nfunc main(...) as int\n return dep.compute(3)\nend\n',1)
                for step in range(5):
                    generation=min(step,3);repeat=step==4
                    if not repeat:
                        body=make_body(512,generation,'imported','declaration')
                        if generation:c.change(dep,body,generation+1)
                        else:c.open(dep,body,1)
                    value=checked(c,entry,1)
                    assert value['ok'] and not value['count'] and value['semanticDefinitions'],value
                    assert value['analysis']['workerThreadId']!=value['analysis']['captureThreadId'],value
                    answer={key:value for key,value in value.items() if key not in ('analysis','durationMs')}
                    if step in facts:assert facts[step]==answer,(mode,step)
                    facts[step]=answer
                    row['queries'].append(dict(generation=generation,repeat=repeat,result=value))
            finally:
                try:row['stderr']=c.stop()
                finally:
                    if c.process.poll() is None:c.process.kill();c.process.wait(timeout=5)
                    row['pid']=c.process.pid;row['exit']=c.process.returncode
                    (output/(mode+'-transcript.json')).write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2)+'\n',encoding='utf8',newline='\n')
                assert row['exit']==0 and not row['stderr'],row
        baseline,candidate=runs
        nodes=lambda row,step:row['queries'][step]['result']['analysis']['lastBuildMaterialization']['nodes']
        # The failed prototype made two complete copies on its first imported
        # declaration edit. Keep the predeclared comparison to the old path.
        assert nodes(candidate,1)<=1.2*nodes(baseline,1),(nodes(baseline,1),nodes(candidate,1))
        for step in [2,3]:assert nodes(candidate,step)<=.5*nodes(baseline,step),(step,nodes(baseline,step),nodes(candidate,step))
        assert candidate['queries'][4]['result']['analysis']['builds']==candidate['queries'][3]['result']['analysis']['builds']
        report.update(status='passed',summary=dict(firstMaterializedNodes={r['mode']:nodes(r,1) for r in runs},hotMaterializedNodes={r['mode']:[nodes(r,step) for step in [2,3]] for r in runs},processes=len(runs),queries=sum(len(r['queries']) for r in runs)))
    finally:
        for path,h in protected.items():assert sha(path)==h,path
        (output/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8',newline='\n')
    print('PASS imported model-source regression:10 complete wire results;first copy bound,hot work reduction,exact-cache reuse')

if __name__=='__main__':main()
