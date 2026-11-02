from pathlib import Path
import json,hashlib,subprocess,shutil,sys
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;P=O/sys.argv[1];P.mkdir();seed=json.loads((O/'source-base.json').read_text());B=Path(seed['baseline']);b=json.loads((B/'build-result.json').read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
aliases={r['originalPath']:r['archivedPath'] for r in seed['archivedInputs']}
def verifybase():
 for p,d in b['inputs'].items():assert sha(aliases.get(p,p))==d,p
 for n,d in b['outputs'].items():assert sha(B/n)==d,n
verifybase();protected={p:sha(p) for p in b['inputs']};protected[str(Path(__file__))]=sha(__file__)
record=dict(status='running',accepted=False,scope='Matched private original-cohort focused compile;all private layouts unchanged;not a complete normal build.',commands=[],runs=[],protected=protected)
def save():(P/'result.json').write_text(json.dumps(record,indent=2)+'\n')
def run(name,c):
 v=subprocess.run(c,cwd=P,capture_output=True,timeout=240);log=P/(name+'.log');log.write_bytes(v.stdout+v.stderr);record['commands'].append(dict(name=name,command=c,exit=v.returncode,log=str(log),sha256=sha(log)));save();assert not v.returncode,(name,v.stderr[-1800:]);return v
try:
 c=list(next(c for c in b['commands'] if '-c' in c and Path(c[c.index('-c')+1]).name=='xl6_sem.c'));c[-1]=str(P/'xl6_sem.o');run('sem-compile',c)
 shutil.copyfile(B/'xlang-analysis-core.a',P/'core.a');run('archive',[str(Path(b['compiler']).with_name('ar.exe')),'rcs',str(P/'core.a'),str(P/'xl6_sem.o')])
 for name in ['analysis_final_environment_test','analysis_index_canonical_test','analysis_final_annotations_test','analysis_native_borrow_test','analysis_semantic_final_input_test']:
  c=list(next(c for c in b['commands'] if '-c' in c and Path(c[c.index('-c')+1]).stem==name));c[-1]=str(P/(name+'.o'));run(name+'-compile',c)
  c=list(next(c for c in b['commands'] if c[-1]==str(B/(name+'.exe'))));c[1]=str(P/(name+'.o'));c=[str(P/'core.a') if x==str(B/'xlang-analysis-core.a') else x for x in c];c[-1]=str(P/(name+'.exe'));run(name+'-link',c)
  v=run(name+'-run',[str(P/(name+'.exe'))]);record['runs'].append(name);save();print(v.stdout.decode('utf8').splitlines()[-1],flush=True)
 c=list(next(c for c in b['commands'] if c[-1]==str(B/'xlang-ls.exe')));c=[str(P/'core.a') if x==str(B/'xlang-analysis-core.a') else x for x in c];c[-1]=str(P/'xlang-ls.exe');run('server-link',c)
 verifybase()
 for p,d in protected.items():assert sha(p)==d,p
 record['status']='passed'
except BaseException as e:record.update(status='failed',error=repr(e));raise
finally:save()
