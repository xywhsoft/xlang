from pathlib import Path
import json,hashlib,subprocess,shutil,sys
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;seed=json.loads((O/'source-base.json').read_text());A=Path(seed['baseline']);B=O/sys.argv[1];B.mkdir();normal=json.loads((A/'build-result.json').read_text());F=Path(normal['frontendRoot']);sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();aliases={r['originalPath']:r['archivedPath'] for r in seed['archivedInputs']}
for p,d in normal['inputs'].items():assert sha(aliases.get(p,p))==d,p
for p,d in normal['outputs'].items():assert sha(A/p)==d,p
sys.path.insert(0,str(R/'ide/tools'));from native_index_overlay import index
_,generated,overlay=index(F,B);(B/'index-overlay.json').write_text(json.dumps(overlay,indent=2))
watch={str(p):sha(p) for p in (R/'ide/src/analysis').rglob('*') if p.is_file() and p.suffix in ('.h','.c','.inc')};watch[str(R/'ide/tools/native_index_overlay.py')]=sha(R/'ide/tools/native_index_overlay.py')
for p in [R/'ide/tools/build_lsp_core.py',R/'ide/tests/analysis_index_canonical_test.c']:watch[str(p)]=sha(p)
record=dict(status='running',scope='Matched private current canonical delta;not full native/performance/GUI admission.',inputs=watch,commands=[],outputs={})
def save():(B/'result.json').write_text(json.dumps(record,indent=2)+'\n')
def run(n,c):
 p=subprocess.run(c,cwd=B,capture_output=True,timeout=240);(B/(n+'.log')).write_bytes(p.stdout+p.stderr);record['commands'].append(dict(name=n,command=c,exit=p.returncode));save();print(n,p.returncode,flush=True);assert not p.returncode,p.stderr[-1800:]
try:
 for name in ['xl6_sem','xl6_sem_index','check']:
  c=list(next(c for c in normal['commands'] if '-c' in c and c[-1]==str(A/(name+'.o'))));c[-1]=str(B/(name+'.o'))
  if name=='xl6_sem_index':c[c.index('-c')+1]=str(generated)
  run(name,c)
 shutil.copyfile(A/'xlang-analysis-core.a',B/'xlang-analysis-core.a');run('archive',[str(Path(normal['compiler']).with_name('ar.exe')),'rcs',str(B/'xlang-analysis-core.a'),*[str(B/(name+'.o')) for name in ['xl6_sem','xl6_sem_index','check']]])
 c=list(next(c for c in normal['commands'] if c[-1]==str(A/'xlang-ls.exe')));c=[str(B/'xlang-analysis-core.a') if v==str(A/'xlang-analysis-core.a') else v for v in c];c[-1]=str(B/'xlang-ls.exe');run('server-link',c)
 for name in sys.argv[2:]:
  reference='analysis_semantic_memo_test' if name=='analysis_index_canonical_test' else 'analysis_final_environment_test' if name=='analysis_index_canonical_guards_test' else name
  c=list(next(c for c in normal['commands'] if '-c' in c and c[-1]==str(A/(reference+'.o'))));c[-1]=str(B/(name+'.o'))
  if reference!=name:c[c.index('-c')+1]=str(R/'ide/tests'/ (name+'.c'))
  c.insert(1,'-I'+str(B/'frontend'));run(name,c)
  c=list(next(c for c in normal['commands'] if c[-1]==str(A/(reference+'.exe'))));c=[str(B/(name+'.o')) if v==str(A/(reference+'.o')) else str(B/'xlang-analysis-core.a') if v==str(A/'xlang-analysis-core.a') else v for v in c];c[-1]=str(B/(name+'.exe'))
  if name=='analysis_index_canonical_test':c.insert(1,'-Wl,--wrap=xl6AstFree')
  if name=='analysis_function_index_asset_test':c.insert(1,'-Wl,--wrap=xlsFrontendIndexCreateCached')
  run(name+'-link',c);run(name+'-run',[str(B/(name+'.exe'))])
 for p,d in watch.items():assert sha(p)==d,p
 record['status']='passed';record['outputs']={p.name:sha(p) for p in B.iterdir() if p.is_file() and p.name!='result.json'}
except BaseException as error:record.update(status='failed',error=repr(error));raise
finally:save()
