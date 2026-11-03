from pathlib import Path
import json,hashlib,subprocess,sys,re
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;F=O/'frontend'
sys.path.insert(0,str(R/'ide/tools'))
from native_c_interop_profile import PINS
from native_frontend_inputs import recover
sha=lambda raw:hashlib.sha256(raw).hexdigest()
seed=json.loads((O/'source-base.json').read_text());B=Path(seed['baseline']);old=Path(json.loads((B/'build-result.json').read_text())['frontendRoot'])
changed=['src/sem/xl6_sem.c','src/sem/xl6_sem_c_owned.c','src/ir/xl6_ir_bound_method.c','src/ir/xl6_ir_lower.c']
F.mkdir();rows=[];overrides={};includes=[]
for rel,expected in PINS.items():
 source=R/rel;raw=source.read_bytes()
 if rel in changed:
  git=subprocess.check_output(['git','show',seed['head']+':'+rel],cwd=R)
  assert git.replace(b'\r\n',b'\n')==raw.replace(b'\r\n',b'\n'),rel
  data=raw;operation='exact-current-committed-source';overrides[rel]=sha(data)
  previous=re.findall(rb'^\s*#\s*include[^\r\n]*',(old/rel).read_bytes(),re.M)
  current=re.findall(rb'^\s*#\s*include[^\r\n]*',data,re.M)
  assert [s.strip() for s in previous]==[s.strip() for s in current],rel
  includes.append(dict(path=rel,directIncludesUnchanged=True,count=len(current)))
 else:
  result=recover(raw,rel);assert result is not None,rel;data,operation=result;assert sha(data)==expected,rel
 target=F/rel;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
 rows.append(dict(path=rel,sourcePath=str(source),sourceSha256=sha(raw),preparedSha256=sha(data),operation=operation))
for row in rows:assert sha(Path(row['sourcePath']).read_bytes())==row['sourceSha256'],row['path']
report=dict(status='frozen-current-source',revision=seed['head'],frontend=str(F),files=rows,newPins=overrides,directIncludeReview=includes,scope='Whole previously consumed356-input cohort;four committed source revisions,all other bytes match the prior pins. Actual compiler include closure still requires audit.')
(O/'frontend-freeze.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(files=len(rows),newPins=overrides,includes=includes)))
