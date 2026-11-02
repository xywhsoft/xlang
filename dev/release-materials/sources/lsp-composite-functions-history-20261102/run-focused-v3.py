from pathlib import Path
import json,sys,subprocess
R=Path(__file__).resolve().parents[4];A=Path(__file__).resolve().parent;B=R/'ide/build/Release/lsp-final-annotations-20261102/full-v1';O=A/'focused-v3'
sys.path.insert(0,str(R/'ide/tools'))
from build_lsp_lifecycle_profile import sha
O.mkdir(exist_ok=False);d=json.loads((B/'build-result.json').read_text());report=dict(status='running',commands=[],protected={})
for p in json.loads((A/'capture.json').read_text())['files']:report['protected'][str(R/p)]=sha(R/p)
report['protected'][str(R/'ide/tests/analysis_composite_function_test.c')]=sha(R/'ide/tests/analysis_composite_function_test.c')
def save():(O/'result.json').write_text(json.dumps(report,indent=2)+'\n')
def run(c):
 q=subprocess.run(c,cwd=O,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=180);report['commands'].append(dict(command=c,exit=q.returncode,stdout=q.stdout,stderr=q.stderr));save();print(Path(c[c.index('-o')+1]).name if '-o' in c else Path(c[0]).name,q.returncode,flush=True)
 if q.stdout:print(q.stdout[-2500:],flush=True)
 if q.returncode:print(q.stderr[-5000:],flush=True)
 assert q.returncode==0
objects=[]
try:
 for reference in [R/'ide/src/analysis/typed_module.c',R/'ide/src/analysis/function_unit.c',B/'frontend/xl6_sem_index.c']:
  c=list(next(c for c in d['commands'] if '-c' in c and str(reference) in c));obj=O/(reference.stem+'.o');c[c.index('-o')+1]=str(obj);run(c);objects.append(obj)
 for name in ['analysis_composite_function_test','analysis_typed_function_test','analysis_function_unit_test','analysis_function_unit_fault_test','analysis_function_index_asset_test','analysis_typed_module_test','analysis_typed_module_fault_test']:
  refname='analysis_typed_function_test' if name=='analysis_composite_function_test' else name
  source=R/'ide/tests'/(name+'.c');ref=R/'ide/tests'/(refname+'.c');c=list(next(c for c in d['commands'] if '-c' in c and str(ref) in c));c[c.index(str(ref))]=str(source);obj=O/(name+'.o');c[c.index('-o')+1]=str(obj);run(c)
  c=list(next(c for c in d['commands'] if '-o' in c and Path(c[c.index('-o')+1]).name==refname+'.exe'));selected=[p for p in objects if name!='analysis_function_unit_fault_test' or p.stem!='function_unit'];c[1:2]=[str(obj),*[str(p) for p in selected]];exe=O/(name+'.exe');c[c.index('-o')+1]=str(exe);run(c);run([str(exe)])
 for p,h in report['protected'].items():assert sha(p)==h,p
 report['status']='passed'
except BaseException as error:report.update(status='failed',error=repr(error));raise
finally:save()
