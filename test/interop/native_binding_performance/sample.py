"""Alternate matched CPU clients; keep raw intervals and distributions.
Process startup and output occur outside each measured interval. This sampler
requires timing binaries and refuses instrumented counter preparations.
"""
from pathlib import Path
import argparse,json,hashlib,subprocess,itertools,statistics,time,os
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
p=argparse.ArgumentParser();p.add_argument('--preparation',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--samples',type=int,default=12);a=p.parse_args()
d=json.loads(a.preparation.read_text());assert d['complete'] and d['passed'] and not d['counterBinary'];assert a.samples>=6
assert all(sha(path)==value for path,value in d['inputs'].items())
out=a.out.resolve();out.mkdir(exist_ok=False)
programs=d['programs'];inputs={str(a.preparation.resolve()):sha(a.preparation),str(Path(__file__).resolve()):sha(__file__)}
for mode,row in programs.items():assert sha(row['path'])==row['sha256'];inputs[row['path']]=row['sha256']
counts={'caps':500000,'draw':500000,'target':50000} if d['fixture']=='warm' else {'shape':40960000}
expected={'caps':3500000,'draw':5000000} if d['fixture']=='warm' else {'shape':101357502464}
report=dict(complete=False,passed=False,scope=__doc__,cpu=os.environ.get('PROCESSOR_IDENTIFIER'),optimization='O2',fixture=d['fixture'],warmupRounds=2,sampleRounds=a.samples,inputs=inputs,rows=[],summary={},shippingDelivery=False)
orders=list(itertools.permutations(('raw','safe','language')))
def save():(out/'timing.json').write_text(json.dumps(report,indent=2)+'\n')
import re
pattern=re.compile(r'(caps|draw|target|shape)\s+(\d+)\s+(\d+)(?:\s+(\d+))?')
for round in range(a.samples+2):
 for mode in orders[round%len(orders)]:
  started=time.monotonic();r=subprocess.run([programs[mode]['path']],capture_output=True,timeout=120)
  log=out/(str(round)+'-'+mode+'.log');log.write_bytes(r.stdout+r.stderr);assert not r.returncode,log
  rows=[]
  for match in pattern.finditer(r.stdout.decode()):
   name,ticks,hz,checksum=match.groups();assert name in counts
   assert int(hz)>0 and int(ticks)>0
   if name in expected:assert int(checksum)==expected[name],(mode,name,checksum)
   rows.append(dict(operation=name,ticks=int(ticks),frequency=int(hz),checksum=int(checksum) if checksum else None,nsPerOperation=int(ticks)/int(hz)*1e9/counts[name]))
  assert {r['operation'] for r in rows}==set(counts)
  report['rows'].append(dict(round=round,mode=mode,warmup=round<2,wallSeconds=time.monotonic()-started,logSha256=sha(log),measurements=rows));save()
for name in counts:
 summaries={}
 for mode in programs:
  values=[m['nsPerOperation'] for row in report['rows'] if row['mode']==mode and not row['warmup'] for m in row['measurements'] if m['operation']==name]
  assert len(values)==a.samples
  values.sort();summaries[mode]=dict(samples=len(values),minimum=values[0],median=statistics.median(values),maximum=values[-1],p25=statistics.quantiles(values,n=4,method='inclusive')[0],p75=statistics.quantiles(values,n=4,method='inclusive')[2])
 summaries['languageOverSafe']=summaries['language']['median']/summaries['safe']['median'];summaries['languageOverRaw']=summaries['language']['median']/summaries['raw']['median'];report['summary'][name]=summaries
report['inputsUnchanged']=all(sha(p)==h for p,h in inputs.items()) and all(sha(path)==value for path,value in d['inputs'].items());assert report['inputsUnchanged'];report.update(complete=True,passed=True);save();print(json.dumps(report['summary'],indent=2))
