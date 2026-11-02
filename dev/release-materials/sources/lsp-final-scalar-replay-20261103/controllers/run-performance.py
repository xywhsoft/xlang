from pathlib import Path
import json,sys,subprocess,hashlib,time
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;B=O/'full-v2';A=Path(json.loads((O/'source-base.json').read_text())['baseline']);P=O/'performance-v1'
sys.path.insert(0,str(R/'ide/tools'))
from build_lsp_lifecycle_profile import verify_build
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
b=json.loads((B/'build-result.json').read_text());verify_build(b,B)
v=json.loads((O/'validation-v1/result.json').read_text());assert v['status']=='passed'
base=json.loads((A/'build-result.json').read_text());capsule=json.loads((O/'source-base.json').read_text());aliases={r['originalPath']:r['archivedPath'] for r in capsule['archivedInputs']}
def verifybase():
 for p,d in base['inputs'].items():assert sha(aliases.get(p,p))==d,p
 for n,d in base['outputs'].items():assert sha(A/n)==d,n
verifybase();P.mkdir()
protected=dict(v['protected']);protected[str(Path(__file__))]=sha(__file__)
record=dict(status='running',accepted=False,scope='One fixed A/A 4-pair and A/B 8-pair run;32/512 functions,single/imported,literal/declaration,14 generations. Original 1.05 gates and5/3GiB floors;no reruns or waivers.',jobs=[],protected=protected)
def save():(P/'result.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
try:
 for n,pairs,program in [('aa',4,A/'xlang-ls.exe'),('ab',8,B/'xlang-ls.exe')]:
  c=[sys.executable,'-X','utf8','-B',str(R/'ide/tools/benchmark_lsp_analysis.py'),'--baseline',str(A/'xlang-ls.exe'),'--candidate',str(program),'--output',str(P/n),'--pairs',str(pairs),'--generations','14']
  if n=='aa':c+=['--calibrate']
  log=P/(n+'.log');row=dict(name=n,command=c,status='running',log=str(log));record['jobs'].append(row);save();start=time.monotonic()
  with log.open('wb') as stream:v=subprocess.run(c,cwd=R,stdout=stream,stderr=subprocess.STDOUT,timeout=1200)
  row.update(exit=v.returncode,seconds=time.monotonic()-start,sha256=sha(log));q=P/n/'result.json';result=json.loads(q.read_text());row.update(status=result['status'],queries=len(result['queries']),processes=len(result['processes']),failedGates=result['failedGates'],result=str(q),resultSha256=sha(q));save();print(n,row['status'],row['failedGates'],flush=True)
  assert result['status'] in ('passed','calibration-failed','gates-failed'),(n,result['status'])
 verify_build(b,B);verifybase()
 for path,digest in protected.items():assert sha(path)==digest,path
 record['accepted']=all(not r['failedGates'] for r in record['jobs']);record['status']='completed' if record['accepted'] else 'performance-not-admitted'
except BaseException as error:record.update(status='failed',error=repr(error));raise
finally:save()
