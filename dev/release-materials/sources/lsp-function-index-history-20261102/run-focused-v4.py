from pathlib import Path
import sys,json,subprocess,hashlib
ROOT=Path(__file__).resolve().parents[4];RUN=Path(__file__).resolve().parent;OUT=RUN/'focused-v4';B=ROOT/'ide/build/Release/lsp-typed-function-ownership-20261102/full-v2'
sys.path.insert(0,str(ROOT/'ide/tools'));from native_index_overlay import index
normal=json.loads((B/'build-result.json').read_text());OUT.mkdir(exist_ok=False)
_,generated,overlay=index(ROOT/'ide/build/Release/lsp-c-interop-alignment-20261102/frontend',OUT)
record=dict(status='running',scope='Focused diagnostics only;regenerate every private collector consumer including embedded allocation fixture;not full-product admission',overlay=overlay,protected={},commands=[])
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
for rel in json.loads((RUN/'capture.json').read_text())['files']:
 p=ROOT/rel;record['protected'][str(p)]=sha(p)
for rel in ['ide/src/analysis/frontend_function_index.h','ide/src/analysis/frontend_function_index.inc','ide/tests/analysis_function_index_asset_test.c']:
 p=ROOT/rel;record['protected'][str(p)]=sha(p)
def invoke(c):
 p=subprocess.run(c,cwd=OUT,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=180);record['commands'].append(dict(command=c,exit=p.returncode,stdout=p.stdout,stderr=p.stderr));print(Path(c[c.index('-o')+1]).name if '-o' in c else Path(c[0]).name,p.returncode,flush=True)
 if p.stdout:print(p.stdout[-1800:],flush=True)
 if p.returncode:print(p.stderr[-6500:]);raise RuntimeError('focused command failed')
def compile(source,reference,name,fixture=False):
 c=list(next(c for c in normal['commands'] if '-c' in c and str(reference) in c));c[c.index(str(reference))]=str(source)
 if fixture:c.insert(1,'-I'+str(OUT/'frontend'))
 for flag in ['-MF','-MT','-o']:
  if flag in c:c[c.index(flag)+1]=str(OUT/(name+('.d' if flag=='-MF' else '.o')))
 invoke(c);return OUT/(name+'.o')
try:
 objects=[compile(generated,B/'frontend/xl6_sem_index.c','xl6_sem_index')]
 for name in ['typed_module','function_unit','check']:objects.append(compile(ROOT/'ide/src/analysis'/(name+'.c'),ROOT/'ide/src/analysis'/(name+'.c'),name))
 for name in ['analysis_function_index_asset_test','analysis_typed_function_test','analysis_typed_function_fallback_test','analysis_index_storage_test','analysis_index_view_test','analysis_entity_versions_test','analysis_ir_input_test']:
  source=ROOT/'ide/tests'/(name+'.c');referenceName='analysis_typed_function_test' if name in ['analysis_function_index_asset_test','analysis_typed_function_fallback_test'] else name;reference=ROOT/'ide/tests'/(referenceName+'.c');record['protected'][str(source)]=sha(source)
  obj=compile(source,reference,name,name=='analysis_index_storage_test')
  template=next(c for c in normal['commands'] if '-o' in c and Path(c[c.index('-o')+1]).name==referenceName+'.exe');c=list(template)
  if name in ['analysis_function_index_asset_test','analysis_typed_function_fallback_test']:c=[v for v in c if not v.startswith('-Wl,--wrap=')]
  selected=[OUT/'typed_module.o'] if name=='analysis_index_storage_test' else objects
  c[1:2]=[str(obj),*[str(p) for p in selected]];c[c.index('-o')+1]=str(OUT/(name+'.exe'))
  if name=='analysis_function_index_asset_test':c+=['-Wl,--wrap='+f for f in ['xrtRealloc','xrtFree','xrtCalloc','xlsFrontendIndexCreate','xlsFrontendIndexBindFunctions']]
  invoke(c);invoke([str(OUT/(name+'.exe'))])
 for p,h in record['protected'].items():assert sha(p)==h,p
 record['status']='passed'
except BaseException as error:record.update(status='failed',error=repr(error));raise
finally:(OUT/'result.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
