from pathlib import Path
import json,hashlib,subprocess,sys,time
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;B=O/'full-v2';A=Path(json.loads((O/'source-base.json').read_text())['baseline']);P=O/'validation-v2'
sys.path.insert(0,str(R/'ide/tools'))
from build_lsp_lifecycle_profile import verify_build
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
b=json.loads((B/'build-result.json').read_text());verify_build(b,B)
base=json.loads((A/'build-result.json').read_text());capsule=json.loads((O/'source-base.json').read_text()); aliases=json.loads((O/'baseline-aliases.json').read_text())['aliases']
def basecheck():
 for p,d in base['inputs'].items():assert sha(aliases.get(p,p))==d,p
 for n,d in base['outputs'].items():assert sha(A/n)==d,n
basecheck();P.mkdir()
oldroot=O;old=json.loads((O/'validation-v1/result.json').read_text());assert old['status']=='failed' and old['commands'][-1]['name']=='ast-replacements'
old['commands']=[row for row in old['commands'] if row['status']=='passed'];assert len(old['commands'])==61
protected={str(R/x):sha(R/x) for x in ['release/xlang_ide.exe','release/xlang-ls.exe','release/xge.dll','release/xl.exe','release/xl.dll','release/xlc.dll','release/option/settings.json','dev/release-materials/provenance/current-delivery.json']}
protected[str(Path(__file__))]=sha(__file__)
record=dict(status='running',accepted=False,scope='Native fallback/omission,full fresh AST/typed-index/IR,56 protocol suites,geometry,canonical and fixed-scalar receipt/resource proofs;not performance,GUI or publication.',normalBuild=str(B),baseline=str(A),referenceVersion='0.101',commands=[],protected=protected,rootTmpInitiallyPresent=(R/'release/tmp').exists())
def save():(P/'result.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
try:
 for row in old['commands']:
  assert sha(row['log'])==row['sha256'],row['name']
  record['commands'].append(dict(row,reusedVerifiedReceipt=True));save()
 verify_build(b,B);basecheck()
 for path,digest in protected.items():assert sha(path)==digest,path
 assert (R/'release/tmp').exists()==record['rootTmpInitiallyPresent']
 check=json.loads((O/'ast-replacements-faults-v2/result.json').read_text());assert check['status']=='passed'
 for p,d in check['protected'].items():assert sha(p)==d,p
 record['commands'].append(dict(name='ast-replacements',status='passed',command=[sys.executable,'-X','utf8','-B',str(R/'ide/tools/check_ast_replacements.py'),'--build',str(B),'--output',str(O/'ast-replacements-faults-v2')],result=str(O/'ast-replacements-faults-v2/result.json'),resultSha256=sha(O/'ast-replacements-faults-v2/result.json')));save()
 for name,tool in [('c-interop','check_c_interop_frontend.py')]:
  c=[sys.executable,'-X','utf8','-B',str(R/'ide/tools'/tool),'--build',str(B),'--output',str(O/(name+'-faults-v1'))]
  log=P/(name+'.log');r=dict(name=name,command=c,status='running',log=str(log));record['commands'].append(r);save();start=time.monotonic()
  with log.open('wb') as stream:q=subprocess.run(c,cwd=R,stdout=stream,stderr=subprocess.STDOUT,timeout=1200)
  r.update(exit=q.returncode,status='passed' if not q.returncode else 'failed',seconds=time.monotonic()-start,sha256=sha(log));save();print(name,r['status'],flush=True);assert not q.returncode,log.read_text(encoding='utf8',errors='replace')[-1500:]
 verify_build(b,B);basecheck()
 for path,digest in protected.items():assert sha(path)==digest,path
 record['status']='passed'
except BaseException as error:record.update(status='failed',error=repr(error));raise
finally:save()
