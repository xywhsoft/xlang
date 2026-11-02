from pathlib import Path
import hashlib,json,os,shutil,subprocess
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
b=json.loads((O/'gui-rebuild.json').read_text());baseline=json.loads((O/'baseline.json').read_text())
v1=json.loads((O/'validation.json').read_text());v3=json.loads((O/'validation-runtime-v3.json').read_text())
assert [t['name'] for t in v1['tests']]==['sha256_test','catalog_test','state_repair_test'] and v3['status']=='passed' and len(v3['tests'])==7
for p,d in {**b['inputs'],**baseline['protected']}.items():assert sha(p)==d,p
assert sha(b['candidate'])==b['candidateSha256']
target=R/'release/xlang_ide.exe';pending=O/'xlang_ide.publish.exe';assert not pending.exists()
shutil.copyfile(b['candidate'],pending);assert sha(pending)==b['candidateSha256'];os.replace(pending,target)
record=dict(status='published-pending-smoke',guiSha256=sha(target),rootCompilerServerSdkAndSettingsUnchanged=True,commands=[])
def save():(O/'publication.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
save()
try:
 for lang in ['xlang','c']:
  args=[str(target),'--hidden','--frames','4','--no-session','--no-auto-diagnostics','--demo',lang,'--settings',str(O/'smoke-settings.json'),'--session-root',str(O/'smoke-session'),'--diagnostic-root',str(O/'smoke-diagnostics'),'--screenshot',str(O/('smoke-'+lang+'.png'))]
  log=O/('smoke-'+lang+'.log')
  with log.open('wb') as stream:result=subprocess.run(args,cwd=R/'release',stdout=stream,stderr=subprocess.STDOUT,timeout=45)
  record['commands'].append(dict(command=args,exit=result.returncode,log=str(log),sha256=sha(log)));save();assert result.returncode==0,log
  assert (O/('smoke-'+lang+'.png')).is_file()
 for p,d in baseline['protected'].items():
  if Path(p)!=target:assert sha(p)==d,p
 assert sha(target)==b['candidateSha256']
 record.update(status='passed-targeted-gui-validation',guiBytes=target.stat().st_size,fullReleaseAdmission=False,existingCatalogRetained=True);save();print(json.dumps(record,indent=2))
except BaseException as error:
 record.update(status='failed',error=repr(error));save();raise
