
from pathlib import Path
import json,subprocess,os,shutil,hashlib,time,uuid
root=Path(r'D:/GIT/x-lang');out=root/'ide/build/Release/dock-view-20261107';m=json.loads((out/'objects/gui-candidate-build-inputs.json').read_text(encoding='utf-8-sig'));cc=m['compiler'];flags=m['flags'];objects=m['objects'];libs=m['libraries'];native=root/'release/win_x64';env=dict(os.environ);env['PATH']=str(native)+';'+str(Path(cc).parent)+';'+env['PATH'];rows=[];inputs={}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(name,cmd,timeout=120):
 start=time.monotonic();r=subprocess.run(list(map(str,cmd)),env=env,cwd=native,capture_output=True,timeout=timeout);log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr);rows.append({'name':name,'command':list(map(str,cmd)),'exitCode':r.returncode,'seconds':round(time.monotonic()-start,3),'logSha256':sha(log)});(out/'test-results.json').write_text(json.dumps({'complete':False,'rows':rows,'inputs':inputs},indent=2)+'\n');print(name,r.returncode,flush=True)
 if r.returncode:raise RuntimeError(str(log)+'\n'+(r.stdout+r.stderr).decode('utf-8',errors='replace')[-3500:])
def runtimeRun(exe,name,args):
 target=native/('xide-verify-'+uuid.uuid4().hex+'.exe');assert not target.exists();shutil.copy2(exe,target)
 try:run(name,[target,*args])
 finally:target.unlink()
for fixture in ['dock_windows_view_test','input_test','settings_view_test','dock_tabs_view_test']:
 source=root/'ide/tests'/(fixture+'.c') if fixture=='dock_windows_view_test' else Path(r'F:/xlang-validation/ide-dock-view-20261107/source/ide/tests')/(fixture+'.c');inputs[str(source)]=sha(source);exe=out/(fixture+'.exe');work=out/(fixture+'-final-r3');work.mkdir(exist_ok=False)
 run(fixture+'-build',[cc,*flags,source,*objects,*libs,'-o',exe]);runtimeRun(exe,fixture+'-run',[work])
exe=out/'objects/xlang_ide.link.exe';inputs[str(exe)]=sha(exe)
runtimeRun(exe,'candidate-smoke',['--hidden','--frames','4','--no-session','--no-auto-diagnostics','--settings',out/'smoke-settings.json','--session-root',out/'smoke-session','--diagnostic-root',out/'smoke-diagnostics','--screenshot',out/'smoke.png'])
assert all(sha(Path(p))==h for p,h in inputs.items())
assert all(sha(Path(p))==h for p,h in m['inputs'].items())
(out/'test-results.json').write_text(json.dumps({'complete':True,'passed':True,'rows':rows,'inputs':inputs,'inputsUnchanged':True,'runtime':str(native),'temporaryRootExecutablesRemoved':True,'productionIdentities':{n:sha(native/n) for n in ['xge.dll','xlang-ls.exe','xl.exe','xlc.dll']}},indent=2)+'\n')
