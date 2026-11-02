from pathlib import Path
import json,sys,subprocess
R=Path(__file__).resolve().parents[4];A=Path(__file__).resolve().parent;B=R/'ide/build/Release/lsp-composite-functions-20261102/full-v1';O=A/'focused-v5'
sys.path.insert(0,str(R/'ide/tools'))
from build_lsp_lifecycle_profile import sha
from build_lsp_core import prepare_native_sem
O.mkdir(exist_ok=False);(O/'frontend').mkdir();_,generated,overlay=prepare_native_sem(R/'ide/build/Release/lsp-c-interop-alignment-20261102/frontend',O)
d=json.loads((B/'build-result.json').read_text());report=dict(status='running',commands=[],overlay=overlay,protected={})
for rel in json.loads((A/'capture.json').read_text())['files']:report['protected'][str(R/rel)]=sha(R/rel)
for rel in ['ide/src/analysis/ast_size.c','ide/src/analysis/ast_size.h','ide/src/analysis/frontend_sem_native.inc','ide/tests/analysis_native_borrow_test.c']:report['protected'][str(R/rel)]=sha(R/rel)
def save():(O/'result.json').write_text(json.dumps(report,indent=2)+'\n')
def run(c):
 q=subprocess.run(c,cwd=O,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=180);report['commands'].append(dict(command=c,exit=q.returncode,stdout=q.stdout,stderr=q.stderr));save();print(Path(c[c.index('-o')+1]).name if '-o' in c else Path(c[0]).name,q.returncode,flush=True)
 if q.stdout:print(q.stdout[-3000:],flush=True)
 if q.returncode:print(q.stderr[-5000:],flush=True)
 assert not q.returncode
objects=[]
try:
 for name in ['ast_size','function_unit','typed_module','frontend_raw_body','frontend_context','check','syntax','semantic','xl6_sem']:
  reference=B/'frontend/xl6_sem.c' if name=='xl6_sem' else R/'ide/src/analysis'/((name if name!='ast_size' else 'ast_storage')+'.c')
  source=generated if name=='xl6_sem' else R/'ide/src/analysis'/(name+'.c')
  c=list(next(c for c in d['commands'] if '-c' in c and str(reference) in c));c[c.index(str(reference))]=str(source);c.insert(1,'-I'+str(O/'frontend'));obj=O/(name+'.o')
  for flag in ['-MF','-MT','-o']:
   if flag in c:c[c.index(flag)+1]=str(O/(name+('.d' if flag=='-MF' else '.o')))
  run(c);objects.append(obj)
 for name in ['analysis_native_borrow_test','analysis_composite_function_test','analysis_final_annotations_test','analysis_semantic_memo_test','analysis_function_unit_test','analysis_function_unit_fault_test','analysis_ir_input_test']:
  refname='analysis_semantic_memo_test' if name=='analysis_native_borrow_test' else name
  source=R/'ide/tests'/(name+'.c');ref=R/'ide/tests'/(refname+'.c');c=list(next(c for c in d['commands'] if '-c' in c and str(ref) in c));c[c.index(str(ref))]=str(source);c.insert(1,'-I'+str(O/'frontend'));obj=O/(name+'.o');c[c.index('-o')+1]=str(obj);run(c)
  c=list(next(c for c in d['commands'] if '-o' in c and Path(c[c.index('-o')+1]).name==refname+'.exe'));selected=[p for p in objects if (name!='analysis_function_unit_fault_test' or p.stem!='function_unit')];c[1:2]=[str(obj),*[str(p) for p in selected]];exe=O/(name+'.exe');c+=['-Wl,--wrap=xlsFrontendSemCheckMemoFinal'] if name=='analysis_native_borrow_test' else [];c[c.index('-o')+1]=str(exe);run(c);run([str(exe)])
 for p,h in report['protected'].items():assert sha(p)==h,p
 report['status']='passed'
except BaseException as error:report.update(status='failed',error=repr(error));raise
finally:save()
