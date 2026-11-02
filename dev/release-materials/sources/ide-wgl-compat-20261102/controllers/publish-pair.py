from pathlib import Path
import hashlib,json,os,shutil,subprocess
R=Path('D:/GIT/x-lang');E=Path(__file__).resolve().parent;SDK=E/'sdk';T=E/'runtime';rootSdk=R/'ide/third_party/xge_sdk'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
b=json.loads((E/'gui-build.json').read_text());v=json.loads((E/'gui-validation-v2.json').read_text());w=json.loads((E/'wgl-check.json').read_text());base=json.loads((E/'baseline.json').read_text());assert b['status']==v['status']==w['status']=='passed'
for p,d in {**b['inputs'],**base['protected']}.items():assert sha(p)==d,p
assert sha(rootSdk/'sdk_manifest.json')==base['parentSdkManifestSha256']
for row in json.loads((rootSdk/'sdk_manifest.json').read_text())['files']:assert sha(rootSdk/row['path'])==row['sha256']
assert sha(T/'xge.dll')==b['outputs']['xge'] and sha(T/'xlang_ide.exe')==b['outputs']['xlang_ide']
backup=E/'before-publication';backup.mkdir();shutil.copytree(rootSdk,backup/'sdk')
for name in ['xge.dll','xlang_ide.exe']:shutil.copyfile(R/'release'/name,backup/name)
record=dict(status='publishing',commands=[],sdkFilesChanged=[],rootCompilerLspSettingsAndDataPreserved=True)
def save():(E/'publication.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
try:
 for p in sorted(SDK.rglob('*')):
  if not p.is_file():continue
  rel=p.relative_to(SDK);dest=rootSdk/rel
  if not dest.exists() or sha(dest)!=sha(p):dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,dest);record['sdkFilesChanged'].append(rel.as_posix())
 for name in ['xge.dll','xlang_ide.exe']:
  stage=R/'ide/build/Release/wgl-compat-20261102'/(name+'.publish');shutil.copyfile(T/name,stage);assert sha(stage)==sha(T/name);os.replace(stage,R/'release'/name)
 save()
 for lang in ['xlang','c']:
  log=E/('root-'+lang+'.log');args=[R/'release/xlang_ide.exe','--hidden','--frames','4','--no-session','--no-auto-diagnostics','--demo',lang,'--settings',E/'root-settings.json','--session-root',E/'root-session','--diagnostic-root',E/'root-diagnostics','--screenshot',E/('root-'+lang+'.png')]
  with log.open('wb') as stream:p=subprocess.run(list(map(str,args)),cwd=R/'release',stdout=stream,stderr=subprocess.STDOUT,timeout=45)
  record['commands'].append(dict(command=list(map(str,args)),exit=p.returncode,log=str(log),sha256=sha(log)));save();assert p.returncode==0,log
 for p,d in base['protected'].items():
  if Path(p).name not in ['xge.dll','xlang_ide.exe']:assert sha(p)==d,p
 for p in SDK.rglob('*'):
  if p.is_file():assert sha(rootSdk/p.relative_to(SDK))==sha(p),p
 record.update(status='passed-targeted-validation',rootGuiSha256=sha(R/'release/xlang_ide.exe'),rootXgeSha256=sha(R/'release/xge.dll'),fullReleaseAdmission=False,physicalWin7Hd4200Tested=False);save();print('Published matched root GUI/SDK and passed C/xlang smoke')
except BaseException as error:
 record.update(status='failed',error=repr(error));save();raise
