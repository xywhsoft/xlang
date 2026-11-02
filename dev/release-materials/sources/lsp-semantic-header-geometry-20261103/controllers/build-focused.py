from pathlib import Path
import json,subprocess,hashlib,shutil,sys
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;B=Path(json.loads((O/'source-base.json').read_text())['baseline']);P=O/sys.argv[1];P.mkdir();b=json.loads((B/'build-result.json').read_text());seed=json.loads((O/'source-base.json').read_text());aliases={r['originalPath']:r['archivedPath'] for r in seed['archivedInputs']}
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
for p,d in b['inputs'].items():assert sha(aliases.get(p,p))==d,p
for n,d in b['outputs'].items():assert sha(B/n)==d,n
watch={str(R/p):sha(R/p) for p in ['ide/src/analysis/frontend_sem.inc','ide/src/analysis/frontend_sem.h','ide/src/analysis/frontend_sem_proof.inc','ide/src/analysis/frontend_sem_body.inc','ide/src/analysis/frontend_sem_geometry.inc','ide/src/analysis/check.c','ide/tests/analysis_native_borrow_test.c','ide/tests/analysis_final_environment_test.c']}
record=dict(status='running',scope='Private focused current-geometry delta build against matched M113 headers;not full acceptance',inputs=watch,commands=[],outputs={})
def save():(P/'result.json').write_text(json.dumps(record,indent=2)+'\n')
def run(n,c):
 v=subprocess.run(c,cwd=P,capture_output=True,timeout=180);(P/(n+'.log')).write_bytes(v.stdout+v.stderr);record['commands'].append(dict(name=n,command=c,exit=v.returncode));save();print(n,v.returncode,flush=True);assert not v.returncode,v.stderr[-1800:]
try:
 for n in ['xl6_sem','check']:
  c=list(next(c for c in b['commands'] if '-c' in c and c[-1]==str(B/(n+'.o'))));c[-1]=str(P/(n+'.o'));run(n,c)
 shutil.copyfile(B/'xlang-analysis-core.a',P/'xlang-analysis-core.a');run('archive',[str(Path(b['compiler']).with_name('ar.exe')),'rcs',str(P/'xlang-analysis-core.a'),str(P/'xl6_sem.o'),str(P/'check.o')])
 c=list(next(c for c in b['commands'] if c[-1]==str(B/'xlang-ls.exe')));c=[str(P/'xlang-analysis-core.a') if v==str(B/'xlang-analysis-core.a') else v for v in c];c[-1]=str(P/'xlang-ls.exe');run('link',c)
 for n in sys.argv[2:]:
  c=list(next(c for c in b['commands'] if '-c' in c and c[-1]==str(B/(n+'.o'))));c[-1]=str(P/(n+'.o'));run(n,c)
  c=list(next(c for c in b['commands'] if c[-1]==str(B/(n+'.exe'))));c=[v.replace(str(B/(n+'.o')),str(P/(n+'.o'))).replace(str(B/'xlang-analysis-core.a'),str(P/'xlang-analysis-core.a')) for v in c];c[-1]=str(P/(n+'.exe'));run(n+'-link',c);run(n+'-run',[str(P/(n+'.exe'))])
 for p,d in watch.items():assert sha(p)==d,p
 record['status']='passed';record['outputs']={p.name:sha(p) for p in P.iterdir() if p.is_file() and p.name!='result.json'}
finally:save()
