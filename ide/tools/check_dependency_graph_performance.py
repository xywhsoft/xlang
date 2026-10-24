"""Pair actual diagnostic-actor invalidation against a previous native fixture.

Each process checks real sources once,retains64 verified analysis contributions,
then invalidates an unobserved path. Timing includes queue,event wake,reverse
lookup or original guard walk,and the unchanged complete diagnostic merge.
Setup/native checks and final source-proof validation are separate. This is
an actor path measurement,not stdio,IDE frames,RSS or whole-project acceptance.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture',type=Path,required=True)
    parser.add_argument('--baseline',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--helpers',type=int,action='append')
    parser.add_argument('--samples',type=int,default=20)
    args=parser.parse_args();helpers=args.helpers or [64,192]
    if any(not 1<=v<=256 for v in helpers) or len(set(helpers))!=len(helpers) or not 2<=args.samples<=100:
        parser.error('unique helper sizes1..256 and samples2..100 required')
    fixture,baseline=args.fixture.resolve(strict=True),args.baseline.resolve(strict=True)
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected={str(p):sha(p) for p in [fixture,baseline,Path(__file__)]}
    rows=[];setups=[];commands=[]
    report=dict(status='failed',scope=__doc__,samples=rows,setup=setups,commands=commands,protected=protected)
    def quantile(values):
        a=sorted(values);return dict(samples=len(a),p50=a[math.ceil(.5*len(a))-1],p95=a[math.ceil(.95*len(a))-1],p99=a[math.ceil(.99*len(a))-1],maximum=a[-1])
    try:
        for size in helpers:
            source=out/f'sources-{size}'
            for run,variant in enumerate(['before','after','after','before']):
                program=baseline if variant=='before' else fixture
                cmd=[str(program),'--benchmark',str(source),str(size),str(args.samples)]
                cp=subprocess.run(cmd,capture_output=True,timeout=600,cwd=program.parent)
                log=out/f'{size}-{run}-{variant}.log';log.write_bytes(cp.stdout+cp.stderr)
                commands.append(dict(command=cmd,exit=cp.returncode,log=str(log),logSha256=sha(log),fixtureSha256=sha(program)))
                assert cp.returncode==0,(cp.stdout+cp.stderr).decode('utf8','replace')
                values=[json.loads(line) for line in cp.stdout.decode('utf8').splitlines() if line.startswith('{')]
                setup=[v for v in values if 'setupMs' in v];measured=[v for v in values if 'sample' in v]
                assert len(setup)==1 and setup[0]['roots']==64 and setup[0]['sourcesPerRoot']==size+1
                assert len(measured)==args.samples and sorted(v['sample'] for v in measured)==list(range(args.samples))
                assert all(v['roots']==64 and v['documents']==size+1 and v['ms']>=0 and v['indexed']==(variant=='after') for v in measured)
                if variant=='after':
                    assert all(v['lookups']==i+1 for i,v in enumerate(measured))
                    assert all(0<measured[i]['comparisons']-measured[i-1]['comparisons']<=32 for i in range(1,len(measured)))
                files={str(p):sha(p) for p in source.glob('*.xl')}
                assert len(files)==size+1
                if any(p in protected for p in files):assert all(protected[p]==h for p,h in files.items())
                protected.update(files);setups.append(dict(helpers=size,run=run,variant=variant,**setup[0]))
                rows.extend(dict(helpers=size,run=run,variant=variant,**v) for v in measured)
                print(size,run,variant,len(measured),'actual actor samples passed',flush=True)
        assert len(rows)==len(helpers)*4*args.samples and all(sha(p)==h for p,h in protected.items())
        report['summary']={str(size):{variant:quantile([v['ms'] for v in rows if v['helpers']==size and v['variant']==variant]) for variant in ['before','after']} for size in helpers}
        report['status']='passed'
    finally:(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(report['summary'],ensure_ascii=False,indent=2),flush=True)


if __name__=='__main__':main()
