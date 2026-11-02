from pathlib import Path
from datetime import datetime,timezone
import copy,json,subprocess,sys
ROOT=Path(__file__).resolve().parents[4];A=Path(__file__).resolve().parent;O=A/'formal-v3'
BASE=ROOT/'ide/build/Release/lsp-frontend-alignment-20261101/full-v2';BUILD=A/'full-v4'
sys.path.insert(0,str(ROOT/'ide/tools'))
from build_lsp_lifecycle_profile import verify_build,sha
from benchmark_lsp_analysis import Counters
aliases=[A/'previous-ide-inputs',A.parent/'lsp-typed-function-ownership-20261102/previous-ide-inputs',A.parent/'lsp-function-index-assets-20261102/previous-ide-inputs']
jobs=[('analysis-comparison',ROOT/'ide/tools/benchmark_lsp_analysis.py'),('navigation-comparison',ROOT/'ide/tools/benchmark_lsp_navigation.py'),('symbols-comparison',ROOT/'ide/build/Release/lsp-index-display-20261101/benchmark_symbols.py')]

def closure(folder):
 original=json.loads((folder/'build-result.json').read_text());adapted=copy.deepcopy(original);adapted['inputs']={};changed=[]
 for name,digest in original['inputs'].items():
  p=Path(name)
  if not p.is_file() or sha(p)!=digest:
   rel=p.relative_to(ROOT);matches=[base/rel for base in aliases if (base/rel).is_file() and sha(base/rel)==digest]
   if not matches:raise ValueError('Missing exact historical input: '+name)
   q=matches[0];changed.append(dict(recordedPath=name,archivedPath=str(q),sha256=digest));p=q
  adapted['inputs'][str(p)]=digest
 assert len(original['inputs'])==len(adapted['inputs'])
 verify_build(adapted,folder)
 return adapted,dict(report=str(folder/'build-result.json'),reportSha256=sha(folder/'build-result.json'),inputs=len(original['inputs']),outputs=len(original['outputs']),exactHistoricalInputAliases=changed)

def main():
 fixed=[];closures=[]
 for folder in [BASE,BUILD]:
  data,receipt=closure(folder);fixed.append((folder,data));closures.append(receipt)
 old= A/'formal-v2/result.json';previous=json.loads(old.read_text())
 assert previous['jobs'][0]['name']=='analysis-aa' and previous['jobs'][0]['status']=='complete' and previous['jobs'][0]['exit']==2
 assert previous['jobs'][1]['name']=='analysis-comparison' and previous['jobs'][1]['status']=='failed'
 assert 'system memory/commit floor:' in (A/'formal-v2/analysis-comparison.log').read_text()
 validation=A/'validation-v2/result.json';assert json.loads(validation.read_text())['status']=='passed'
 O.mkdir(exist_ok=False)
 files=[BASE/'xlang-ls.exe',BUILD/'xlang-ls.exe',Path(__file__),validation,old,A/'formal-v2/analysis-aa/result.json',*[p/'build-result.json' for p in [BASE,BUILD]],*[tool for _,tool in jobs]]
 for name in ['xge.dll','xl.exe','xl.dll','xlc.dll','xlang_ide.exe','xlang-ls.exe','option/settings.json']:files.append(ROOT/'release'/name)
 record=dict(status='running',accepted=False,utc=datetime.now(timezone.utc).isoformat(),scope='Resume only resource-invalid analysis comparison and previously unrun navigation/outline. Original fixed M107 candidate and baseline,8 pairs,32/512 functions,14 generations and original gates unchanged. Completed A/A4 with three failures remains authoritative;not repeated or pooled. No own concurrent build/QA/bulk hashing. Starts>=5GiB,stays>=3GiB. Not M108/M109 performance,GUI or release admission.',closures=closures,jobs=[],protected={str(p):sha(p) for p in files},resources=[],previous=dict(path=str(old),sha256=sha(old),completedAA='retained unchanged;exit2;three failed gates',interruptedAnalysis='all partial samples retained;not used for acceptance'))
 def save():(O/'result.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8',newline='\n')
 save()
 try:
  for name,tool in jobs:
   Counters().qualify(record['resources'],5,phase='job-preflight',job=name)
   command=[sys.executable,'-X','utf8','-B',str(tool),'--baseline',str(BASE/'xlang-ls.exe'),'--candidate',str(BUILD/'xlang-ls.exe'),'--output',str(O/name),'--functions','32,512','--pairs','8','--generations','14']
   row=dict(name=name,command=command,status='running');record['jobs'].append(row);save();print('Starting',name,flush=True)
   with (O/(name+'.log')).open('wb') as stream:
    child=subprocess.Popen(command,cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT);row['pid']=child.pid;save();child.wait()
   row.update(exit=child.returncode,status='complete' if child.returncode in (0,2) else 'failed');save();print(name,'exit',child.returncode,flush=True)
   if child.returncode not in (0,2):raise RuntimeError(name+' failed')
  for p,digest in record['protected'].items():assert sha(p)==digest,p
  for folder,data in fixed:verify_build(data,folder)
  record['status']='complete'
 except BaseException as error:record.update(status='failed',error=repr(error));raise
 finally:save()

if __name__=='__main__':main()
