from pathlib import Path
import json,hashlib
R=Path(__file__).resolve().parents[4];A=Path(__file__).resolve().parent;B=A/'full-v3';OLD=A.parent/'lsp-composite-functions-20261102/full-v1'
def digest(body):return hashlib.sha256(body).hexdigest()
def take(body,start,end):
 first=body.index(start);last=body.index(end,first);return body[first:last]
rows=[]
for name,start,end in [('type','static void xl6SemAnnotateTypeIdentities(','\ntypedef struct XL6_SemPlanLoopFrame'),('plan','static void xl6SemAnnotateExecutionPlans(','\nstatic int xl6SemCheckMainEntry(')]:
 before=take((OLD/'frontend/xl6_sem.c').read_bytes(),start.encode(),end.encode());after=take((B/'frontend/xl6_sem.c').read_bytes(),start.encode(),end.encode());assert before==after,name
 rows.append(dict(name=name,bytes=len(after),sha256=digest(after),exactlyEqualToM111=True))
for name in ['xl6_sem_c_owned.effects.c']:
 before=(OLD/'frontend'/name).read_bytes();after=(B/'frontend'/name).read_bytes();assert before==after,name
 rows.append(dict(name=name,bytes=len(after),sha256=digest(after),exactlyEqualToM111=True))
record=dict(status='passed',scope='Original generated type/plan bodies including unchanged M110 hooks,and complete C-owned effects producer byte-identical to M111. Ordinary native composition changes module-boundary invocation only.',rows=rows)
(A/'native-rule-proof-v1.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8',newline='\n');print(record)
