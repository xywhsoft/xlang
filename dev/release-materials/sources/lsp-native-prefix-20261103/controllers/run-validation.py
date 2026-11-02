from pathlib import Path
import json,hashlib,subprocess,sys,time
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;B=O/'full-v2';A=O/'baseline-v2';P=O/'validation-v1'
sys.path.insert(0,str(R/'ide/tools'))
from build_lsp_lifecycle_profile import verify_build
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
b=json.loads((B/'build-result.json').read_text());verify_build(b,B)
base=json.loads((A/'build-result.json').read_text());capsule=json.loads((O/'baseline-closure.json').read_text()); aliases={v['recordedPath']:v['archivedPath'] for v in capsule['archivedChangedInputs']}
def basecheck():
 for p,d in base['inputs'].items():assert sha(aliases.get(p,p))==d,p
 for n,d in base['outputs'].items():assert sha(A/n)==d,n
basecheck();P.mkdir()
oldroot=Path('E:/codex-builds-xlang-lsp-20261102-native-borrow');old=json.loads((oldroot/'validation-v1/result.json').read_text())
protected={str(R/x):sha(R/x) for x in ['release/xlang_ide.exe','release/xlang-ls.exe','release/xge.dll','release/xl.exe','release/xl.dll','release/xlc.dll','dev/release-materials/provenance/current-delivery.json']}
protected[str(Path(__file__))]=sha(__file__)
record=dict(status='running',accepted=False,scope='Native fallback/omission,full fresh AST/index/IR and 56 protocol suites;not performance,GUI or publication.',normalBuild=str(B),baseline=str(A),referenceVersion='0.101',commands=[],protected=protected,rootTmpInitiallyPresent=(R/'release/tmp').exists())
def save():(P/'result.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
try:
 for row in old['commands']:
  n=row['name'];c=[v.replace(str(oldroot/'full-v4'),str(B)).replace(str(oldroot/'validation-v1'),str(P)).replace(str(oldroot/'omissions-v1'),str(O/'omissions-v1')).replace(str(oldroot/'wire-v1'),str(O/'wire-v1')) for v in row['command']]
  for flag in ['--baseline','--reference']:
   if flag in c:c[c.index(flag)+1]=str(A/'xlang-ls.exe')
  if '--baseline-version' in c:c[c.index('--baseline-version')+1]='0.101'
  elif n in ['raw-bodies','raw-signature','raw-signature-open','memo-types']:c+=['--baseline-version','0.101']
  log=P/(n+'.log');r=dict(name=n,command=c,status='running',log=str(log));record['commands'].append(r);save();start=time.monotonic()
  with log.open('wb') as stream:v=subprocess.run(c,cwd=R,stdout=stream,stderr=subprocess.STDOUT,timeout=240)
  r.update(exit=v.returncode,status='passed' if not v.returncode else 'failed',seconds=time.monotonic()-start,sha256=sha(log));save()
  print(n,r['status'],flush=True)
  assert not v.returncode,(n,log.read_text(encoding='utf8',errors='replace')[-1800:])
 verify_build(b,B);basecheck()
 for path,digest in protected.items():assert sha(path)==digest,path
 assert (R/'release/tmp').exists()==record['rootTmpInitiallyPresent']
 record['status']='passed'
except BaseException as error:record.update(status='failed',error=repr(error));raise
finally:save()
