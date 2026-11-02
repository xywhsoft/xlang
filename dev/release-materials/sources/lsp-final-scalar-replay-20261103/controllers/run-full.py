from pathlib import Path
import json,hashlib,subprocess,sys,time
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;P=O/'full-v1';seed=json.loads((O/'source-base.json').read_text());A=Path(seed['baseline']);a=json.loads((A/'build-result.json').read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
aliases={r['originalPath']:r['archivedPath'] for r in seed['archivedInputs']}
def verifybase():
 for p,d in a['inputs'].items():assert sha(aliases.get(p,p))==d,p
 for p,d in a['outputs'].items():assert sha(A/p)==d,p
verifybase();assert not P.exists()
c=[sys.executable,'-X','utf8','-B',str(R/'ide/tools/build_lsp_core.py'),'--frontend-root',a['frontendRoot'],'--cc',a['compiler'],'--output',str(P),'--configuration','Release','--server']
record=dict(status='running',command=c,protected=seed['protected'],baseline=str(A));path=O/'full-run-v1.json'
def save():path.write_text(json.dumps(record,indent=2)+'\n')
save();begin=time.monotonic()
try:
 with (O/'full-run-v1.log').open('wb') as stream:v=subprocess.run(c,cwd=R,stdout=stream,stderr=subprocess.STDOUT,timeout=2400)
 record.update(exit=v.returncode,seconds=time.monotonic()-begin);save();assert not v.returncode,(O/'full-run-v1.log').read_text(errors='replace')[-2400:]
 b=json.loads((P/'build-result.json').read_text());sys.path.insert(0,str(R/'ide/tools'));from build_lsp_lifecycle_profile import verify_build
 verify_build(b,P);verifybase()
 for p,d in seed['protected'].items():assert sha(p)==d,p
 record['status']='passed';print('full build passed:',sum('-c' in c for c in b['commands']),'translation units,',len(b['runs']),'native programs,',len(b['privateAstConsumers']),'private AST consumers')
except BaseException as e:record.update(status='failed',error=repr(e));raise
finally:save()
