from pathlib import Path
import json,sys,subprocess,os
R=Path(__file__).resolve().parents[4];A=Path(__file__).resolve().parent;B=A/'full-v2';OLD=A.parent/'lsp-composite-functions-20261102/full-v1';O=A/'native-diagnostic-v3'
sys.path.insert(0,str(R/'ide/tools'))
from build_lsp_lifecycle_profile import sha
O.mkdir(exist_ok=False);normal=json.loads((B/'build-result.json').read_text());old=json.loads((OLD/'build-result.json').read_text())
assert normal['status']=='failed' and len(normal['runs'])==50
# All actual header bytes and private AST consumers are unchanged;only opaque
# check.c and its accounting oracle change. This diagnostic rearchives matching
# objects;it is NOT a complete successful product build or release admission.
for path,digest in normal['inputs'].items():
 if Path(path).suffix.lower() in ['.h','.inc']:assert sha(path)==digest,path
report=dict(status='running',scope='Matching unchanged headers/ABI,patched opaque check.c,all native oracles. Diagnostic only;full product rebuild still required.',commands=[],protected={p:sha(p) for p in normal['inputs']},cases=[])
report['protected'][str(Path(__file__).resolve())]=sha(__file__)
for p in [*B.glob('*.o'),B/'xlang-analysis-core.a',B/'build-result.json']:report['protected'][str(p)]=sha(p)
def save():(O/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8',newline='\n')
def run(name,c):
 log=O/(name+'.log')
 with log.open('wb') as stream:q=subprocess.run(c,cwd=O,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
 report['commands'].append(dict(name=name,command=c,exit=q.returncode,log=str(log),sha256=sha(log)));save()
 if q.returncode:print(name,q.returncode,log.read_text('utf8',errors='replace')[-2000:],flush=True)
 return q.returncode==0
def compile(path):
 c=list(next(c for c in normal['commands'] if '-c' in c and str(path) in c));c[c.index('-o')+1]=str(O/(path.stem+'.o'));assert run(path.stem+'-compile',c);return O/(path.stem+'.o')
try:
 check=compile(R/'ide/src/analysis/check.c');test=compile(R/'ide/tests/analysis_check_materialize_test.c')
 ar=list(next(c for c in normal['commands'] if len(c)>2 and c[1]=='rcs' and c[2].endswith('xlang-analysis-core.a')));ar[2]=str(O/'xlang-analysis-core.a');ar=[str(check) if p==str(B/'check.o') else p for p in ar];assert run('matching-core-archive',ar)
 names=['analysis_native_borrow_test']+[row['name'] for row in old['runs']];assert len(names)==132
 for name in names:
  refname='analysis_semantic_memo_test' if name=='analysis_native_borrow_test' else name
  c=list(next(c for c in old['commands'] if '-o' in c and Path(c[c.index('-o')+1]).name==refname+'.exe'))
  if name=='analysis_native_borrow_test':c=[v.replace('analysis_semantic_memo_test.o','analysis_native_borrow_test.o') for v in c]+['-Wl,--wrap=xlsFrontendSemCheckMemoFinal']
  c=[p.replace(str(OLD),str(B)) for p in c];c=[str(O/'xlang-analysis-core.a') if p==str(B/'xlang-analysis-core.a') else str(test) if p==str(B/'analysis_check_materialize_test.o') else p for p in c]
  exe=O/(name+'.exe');c[c.index('-o')+1]=str(exe)
  linked=run(name+'-link',c);passed=linked and run(name+'-run',[str(exe)]);report['cases'].append(dict(name=name,status='passed' if passed else 'failed'));save();print(name,passed,flush=True)
 for path,digest in report['protected'].items():assert sha(path)==digest,path
 report['status']='passed' if all(x['status']=='passed' for x in report['cases']) else 'failed'
except BaseException as error:report.update(status='failed',error=repr(error));raise
finally:save()
print(report['status'],len(report['cases']),flush=True)
