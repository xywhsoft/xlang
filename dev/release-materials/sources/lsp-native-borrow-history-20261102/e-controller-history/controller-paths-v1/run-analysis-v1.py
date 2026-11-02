from pathlib import Path
from datetime import datetime,timezone
import copy,json,subprocess,sys
ROOT=Path('D:/GIT/x-lang');A=Path(__file__).resolve().parent;O=A/'analysis-v1';B=A/'full-v4';OLD=A.parent/'lsp-composite-functions-20261102/full-v1'
sys.path.insert(0,str(ROOT/'ide/tools'))
from build_lsp_lifecycle_profile import verify_build,sha
from benchmark_lsp_analysis import Counters

def main():
 normal=json.loads((B/'build-result.json').read_text());verify_build(normal,B)
 old=json.loads((OLD/'build-result.json').read_text());mapped=copy.deepcopy(old);mapped['inputs']={};aliases=[];manifest=json.loads((A/'capture.json').read_text())['files']
 for name,digest in old['inputs'].items():
  p=Path(name)
  if sha(p)!=digest:
   rel=p.relative_to(ROOT).as_posix();assert manifest.get(rel,{}).get('sha256')==digest,rel;q=A/'previous-ide-inputs'/rel;assert sha(q)==digest;aliases.append(dict(recordedPath=name,archivedPath=str(q),sha256=digest));p=q
  mapped['inputs'][str(p)]=digest
 verify_build(mapped,OLD)
 validation=A/'validation-v1/result.json';assert json.loads(validation.read_text())['status']=='passed'
 O.mkdir(exist_ok=False);tool=ROOT/'ide/tools/benchmark_lsp_analysis.py'
 files=[OLD/'xlang-ls.exe',B/'xlang-ls.exe',Path(__file__),validation,B/'build-result.json',OLD/'build-result.json',tool]
 files += [ROOT/'release'/name for name in ['xge.dll','xl.exe','xl.dll','xlc.dll','xlang_ide.exe','xlang-ls.exe','option/settings.json']]
 record=dict(status='running',accepted=False,utc=datetime.now(timezone.utc).isoformat(),scope='Targeted ordinary native function AST borrowing experiment vs matching M111 ownership/frontend. Ordinary original C-owned validation and final IR remain complete. A/A4 and A/B8 pairs,32/512 functions,14 generations,original64 gates. No own concurrent build/QA/bulk reports. Starts>=5GiB,stays>=3GiB. Not full end-to-end LSP/GUI/release performance admission.',baselineClosure=dict(inputs=len(old['inputs']),outputs=len(old['outputs']),exactAliases=aliases),candidateClosure=dict(inputs=len(normal['inputs']),outputs=len(normal['outputs'])),jobs=[],protected={str(p):sha(p) for p in files},resources=[])
 def save():(O/'result.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8',newline='\n')
 save()
 try:
  for name,candidate,pairs in [('analysis-aa',OLD/'xlang-ls.exe',4),('analysis-comparison',B/'xlang-ls.exe',8)]:
   Counters().qualify(record['resources'],5,phase='job-preflight',job=name)
   command=[sys.executable,'-X','utf8','-B',str(tool),'--baseline',str(OLD/'xlang-ls.exe'),'--candidate',str(candidate),'--output',str(O/name),'--functions','32,512','--pairs',str(pairs),'--generations','14']
   if name=='analysis-aa':command+=['--calibrate']
   row=dict(name=name,command=command,status='running');record['jobs'].append(row);save();print('Starting',name,flush=True)
   with (O/(name+'.log')).open('wb') as stream:
    child=subprocess.Popen(command,cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT);row['pid']=child.pid;save();child.wait()
   row.update(exit=child.returncode,status='complete' if child.returncode in (0,2) else 'failed');save();print(name,'exit',child.returncode,flush=True)
   if child.returncode not in (0,2):raise RuntimeError(name+' failed')
  for path,digest in record['protected'].items():assert sha(path)==digest,path
  verify_build(normal,B);verify_build(mapped,OLD);record['status']='complete'
 except BaseException as error:record.update(status='failed',error=repr(error));raise
 finally:save()

if __name__=='__main__':main()
