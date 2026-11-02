from pathlib import Path
import json,sys,subprocess,hashlib
ROOT=Path(__file__).resolve().parents[4];A=Path(__file__).resolve().parent;O=A/'focused-v3'
B=ROOT/'ide/build/Release/lsp-function-index-assets-20261102/full-v1'
sys.path.insert(0,str(ROOT/'ide/tools'))
from build_lsp_core import prepare_native_sem
from build_lsp_lifecycle_profile import sha
O.mkdir(exist_ok=False);(O/'frontend').mkdir()
_,generated,overlay=prepare_native_sem(ROOT/'ide/build/Release/lsp-c-interop-alignment-20261102/frontend',O)
normal=json.loads((B/'build-result.json').read_text());capture=json.loads((A/'capture.json').read_text())
protected={str(ROOT/p):sha(ROOT/p) for p in capture['files']}
for name in ['ide/src/analysis/frontend_sem_final.inc','ide/tests/analysis_final_annotations_test.c']:protected[str(ROOT/name)]=sha(ROOT/name)
record=dict(status='running',scope='Focused matching opaque API diagnostic,not full product/performance admission',overlay=overlay,protected=protected,commands=[])
def save():(O/'result.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8',newline='\n')
def run(command):
 r=subprocess.run(command,cwd=O,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=180)
 record['commands'].append(dict(command=command,exit=r.returncode,stdout=r.stdout,stderr=r.stderr));save()
 print(Path(command[command.index('-o')+1]).name if '-o' in command else Path(command[0]).name,r.returncode,flush=True)
 if r.stdout:print(r.stdout[-2500:],flush=True)
 if r.returncode:print(r.stderr[-5000:],flush=True)
 assert not r.returncode
def compile(source,reference,name):
 c=list(next(c for c in normal['commands'] if '-c' in c and str(reference) in c));c[c.index(str(reference))]=str(source)
 for flag in ['-MF','-MT','-o']:
  if flag in c:c[c.index(flag)+1]=str(O/(name+('.d' if flag=='-MF' else '.o')))
 run(c);return O/(name+'.o')
try:
 objects=[compile(generated,B/'frontend/xl6_sem.c','xl6_sem'),compile(ROOT/'ide/src/analysis/check.c',ROOT/'ide/src/analysis/check.c','check')]
 for name in ['analysis_final_annotations_test','analysis_final_environment_test','analysis_semantic_memo_test','analysis_semantic_final_input_test','analysis_ir_input_test','analysis_function_index_asset_test']:
  referenceName='analysis_semantic_memo_test' if name=='analysis_final_annotations_test' else 'analysis_semantic_final_input_test' if name=='analysis_final_environment_test' else name
  reference=ROOT/'ide/tests'/(referenceName+'.c');source=ROOT/'ide/tests'/(name+'.c');record['protected'][str(source)]=sha(source)
  command=list(next(c for c in normal['commands'] if '-c' in c and str(reference) in c));command[command.index(str(reference))]=str(source);command.insert(1,'-I'+str(O/'frontend'))
  obj=O/(name+'.o')
  for flag in ['-MF','-MT','-o']:
   if flag in command:command[command.index(flag)+1]=str(O/(name+('.d' if flag=='-MF' else '.o')))
  run(command)
  command=list(next(c for c in normal['commands'] if '-o' in c and Path(c[c.index('-o')+1]).name==referenceName+'.exe'))
  if name=='analysis_final_environment_test':command=[v for v in command if not v.startswith('-Wl,--wrap=')]
  selected=[O/'check.o'] if name in ['analysis_final_environment_test','analysis_semantic_final_input_test'] else objects
  command[1:2]=[str(obj),*[str(v) for v in selected]];command[command.index('-o')+1]=str(O/(name+'.exe'));run(command);run([str(O/(name+'.exe'))])
 for p,digest in record['protected'].items():assert sha(p)==digest,p
 record['status']='passed'
except BaseException as error:record.update(status='failed',error=repr(error));raise
finally:save()
