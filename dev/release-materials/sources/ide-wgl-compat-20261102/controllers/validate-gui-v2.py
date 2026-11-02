from pathlib import Path
import hashlib,json,os,re,shutil,subprocess
R=Path('D:/GIT/x-lang');E=Path(__file__).resolve().parent;T=E/'runtime';SDK=E/'sdk';b=json.loads((E/'gui-build.json').read_text());assert b['status']=='passed'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
protected={**b['inputs'],**json.loads((E/'baseline.json').read_text())['protected']}
protected.update({str(p):sha(p) for p in [E/'gui-build.json',E/'wgl-check.json',Path(__file__),SDK/'xge.dll']})
record=dict(status='running',scope='Candidate matched SDK/GUI with original release resources. Real WGL faults,GL3.3-only request shim,normal C/xlang render and native GUI regressions. Not physical HD4200/Win7 verification or full release admission.',protected=protected,commands=[],tests=[],faults=[])
env=dict(os.environ,PATH=str(T)+os.pathsep+os.environ['PATH']);temp=E/'temp-v2';temp.mkdir();env.update(TEMP=str(temp),TMP=str(temp))
def save():(E/'gui-validation-v2.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
def run(name,args,expected=0,environment=env,timeout=120):
 log=E/('v2-'+name+'.log');row=dict(name=name,command=list(map(str,args)),log=str(log));record['commands'].append(row);save()
 with log.open('wb') as stream:
  p=subprocess.Popen(list(map(str,args)),cwd=T,env=environment,stdout=stream,stderr=subprocess.STDOUT);row['pid']=p.pid;save()
  try:p.wait(timeout=timeout)
  except BaseException:p.kill();p.wait();raise
 row.update(exit=p.returncode,sha256=sha(log));save()
 if p.returncode!=expected:raise ValueError(name+': '+log.read_text('utf8',errors='replace')[-1800:])
 return log
try:
 def exports(p):
  data=subprocess.check_output([str(Path(b['compiler']).with_name('objdump.exe')),'-p',p],text=True)
  return set(re.findall(r'\[\s*\d+\]\s+\+base\[\s*\d+\]\s+[0-9a-fA-F]+\s+(\S+)',data))
 before=exports(R/'release/xge.dll');after=exports(SDK/'xge.dll');assert len(before)>3000 and before==after,(len(before),len(after));record['unchangedExportNames']=len(before)
 def smoke(name,lang='xlang',expected=0,environment=env):
  diag=E/('v2-diagnostics-'+name);args=[T/'xlang_ide.exe','--hidden','--frames','4','--no-session','--no-auto-diagnostics','--demo',lang,'--settings',E/'smoke-settings.json','--session-root',E/'session','--diagnostic-root',diag]
  if not expected:args+=['--screenshot',E/(name+'.png')]
  log=run(name,args,expected,environment);text=log.read_text('utf8',errors='replace')
  assert 'Assertion failed' not in text and '_assert' not in text
  if expected:
   reports=list(diag.glob('xlang-ide-*.log'));assert reports,(diag,text)
   report=reports[0].read_text('utf8');assert 'sapp/wgl' in report and 'OpenGL 3.3' in report and 'Windows=' in report,report
   record['faults'].append(dict(name=name,diagnostic=str(reports[0]),sha256=sha(reports[0])))
  else:assert (E/(name+'.png')).is_file()
  save()
 shutil.copyfile(E/'faults/xge.dll',T/'xge.dll')
 try:
  for mode in ['missing-library','missing-export','dummy-create','dummy-current','missing-attrib','create','real-current']:smoke('gui-fault-'+mode,expected=1,environment=dict(env,XIDE_WGL_FAULT=mode))
 finally:shutil.copyfile(SDK/'xge.dll',T/'xge.dll')
 cases=E/'native-cases-v2';cases.mkdir()
 for name in ['sha256_test','workbench_test','editor_view_test','input_test','window_test','dpi_test']:
  source=R/'ide/tests'/(name+'.c');protected[str(source)]=sha(source);exe=T/(name+'.exe');run(name+'-compile',[b['compiler'],*b['flags'],source,*b['objects'],*b['libraries'],'-o',exe]);case=cases/name;case.mkdir();args=[exe] if name=='workbench_test' else [exe,case];log=run(name,args,timeout=240)
  record['tests'].append(dict(name=name,status='passed',output=log.read_text('utf8',errors='replace')[-1700:],executableSha256=sha(exe)));save();print(name,'passed',flush=True)
 for p,d in protected.items():assert sha(p)==d,p
 assert sha(T/'xge.dll')==b['outputs']['xge'];record.update(status='passed',fullReleaseAdmission=False,physicalWin7Hd4200Tested=False);save()
except BaseException as error:record.update(status='failed',error=repr(error));save();raise
