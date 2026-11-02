from pathlib import Path
import gc,json,sys
ROOT=Path(__file__).resolve().parents[4];A=Path(__file__).resolve().parent;O=A/'formal-v3'
sys.path.insert(0,str(ROOT/'ide/tools'))
from build_lsp_lifecycle_profile import sha

def receipt(path):
 path=Path(path);return dict(path=str(path),sha256=sha(path),bytes=path.stat().st_size)

controller=json.loads((O/'result.json').read_text());assert controller['status']=='complete'
for path,digest in controller['protected'].items():assert sha(path)==digest,path
result=dict(schema=1,phase='M7a2b107-performance-completion',date='2026-11-02',status='completed-gates-failed',accepted=False,scope=controller['scope'],controller=receipt(O/'result.json'),closures=controller['closures'],protected=controller['protected'],previous=controller['previous'],jobs=[])
paths=[('retained-analysis-aa',A/'formal-v2/analysis-aa/result.json')]+[(job['name'],O/job['name']/'result.json') for job in controller['jobs']]
for name,path in paths:
 raw=json.loads(path.read_text());assert raw['status'] in ['gates-failed','calibrated'] and not raw['accepted']
 assert sha(raw['plan'])==raw['planSha256']
 for resource in raw['resources']:
  assert min(resource['availablePhysical'],resource['availableCommit'])>=resource['requiredGiB']*1024**3,resource
 for process in raw['processes']:
  assert process['exit']==0 and not process['cleanupErrors'],process
  for kind in ['stderr','transcript']:assert sha(process[kind])==process[kind+'Sha256'],process[kind]
 failed=[row for row in raw['gates'] if not row['passed']]
 row=dict(name=name,**receipt(path),status=raw['status'],plan=receipt(raw['plan']),queries=len(raw['queries']),processes=len(raw['processes']),queryRequests=raw.get('queryRequests',len(raw['queries'])),checkRetries=raw.get('checkRetries',0),gates=len(raw['gates']),failedGates=failed,resourceSamples=len(raw['resources']),minimumObservedAvailableCommitGiB=min(x['availableCommit'] for x in raw['resources'])/1024**3,minimumObservedAvailablePhysicalGiB=min(x['availablePhysical'] for x in raw['resources'])/1024**3,allProcessesExitedZero=True,cleanupErrors=0,allProcessArtifactHashesVerified=True,summary=raw['summary'])
 if name!='retained-analysis-aa':
  row['candidatePeakObservedPrivateBytes']=max(q['memory']['private'] for q in raw['queries'] if q['variant']=='candidate')
  row['candidatePeakObservedPagefileBytes']=max(q['memory']['peakPagefile'] for q in raw['queries'] if q['variant']=='candidate')
  row['baselinePeakObservedPrivateBytes']=max(q['memory']['private'] for q in raw['queries'] if q['variant']=='baseline')
 if name.endswith('-comparison') and name!='analysis-comparison':
  row['postResponseCpuMs']={variant:[p['afterLastResponseCpuMs'] for p in raw['processes'] if p['variant']==variant] for variant in ['baseline','candidate']}
  row['stopMs']={variant:[p['stopMs'] for p in raw['processes'] if p['variant']==variant] for variant in ['baseline','candidate']}
 result['jobs'].append(row)
 print(json.dumps(dict(name=name,queries=row['queries'],processes=row['processes'],failed=len(failed),gates=row['gates'],minimumCommitGiB=row['minimumObservedAvailableCommitGiB'],failedMetrics=[g['name'] for g in failed])),flush=True)
 del raw;gc.collect()
assert [len(j['failedGates']) for j in result['jobs']]==[3,2,6,26]
result['limitations']=['Original completed A/A has three failed gates;no causal speedup claim or selective rerun.','Original resource-invalid runs and every successful slow sample remain unchanged.','Measurements concern fixed M107 full-v4,not later M108/M109 sources.','Ordinary full AST and final IR work,current SDK/engine/IDE alignment,GUI and remaining full goal are not admitted.']
target=O/'post-summary.json'
with target.open('x',encoding='utf8',newline='\n') as stream:stream.write(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(summary=str(target),bytes=target.stat().st_size,sha256=sha(target))))
