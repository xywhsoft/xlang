
from pathlib import Path
import json,subprocess,os,shutil,hashlib,time,uuid
root=Path(r'D:/GIT/x-lang');out=root/'ide/build/Release/minimap-setting-20261107';m=json.loads((out/'build.json').read_text());native=root/'release/win_x64';env=dict(os.environ);env['PATH']=str(native)+';'+str(Path(m['compiler']).parent)+';'+env['PATH'];rows=[];inputs={}
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def run(name,cmd,timeout=120):
 t=time.monotonic();r=subprocess.run(list(map(str,cmd)),cwd=native,env=env,capture_output=True,timeout=timeout);log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr);rows.append(dict(name=name,command=list(map(str,cmd)),exitCode=r.returncode,seconds=round(time.monotonic()-t,3),logSha256=sha(log)));(out/'acceptance.json').write_text(json.dumps(dict(complete=False,rows=rows,inputs=inputs),indent=2)+'\n');print(name,r.returncode,flush=True)
 if r.returncode:raise RuntimeError(str(log)+'\n'+(r.stdout+r.stderr).decode('utf-8',errors='replace')[-4000:])
def runtime(exe,name,args):
 target=native/('xide-verify-'+uuid.uuid4().hex+'.exe');assert not target.exists();shutil.copy2(exe,target)
 try:run(name,[target,*args])
 finally:target.unlink()
for fixture in ['settings_view_test','editor_minimap_test','dock_windows_view_test','input_test']:
 source=root/'ide/tests'/(fixture+'.c') if fixture in ['settings_view_test','editor_minimap_test','dock_windows_view_test'] else Path(r'F:/xlang-validation/ide-dock-view-20261107/source/ide/tests')/(fixture+'.c');inputs[str(source)]=sha(source);exe=out/(fixture+'.exe');work=out/(fixture+'-final');work.mkdir(exist_ok=False)
 run(fixture+'-build',[m['compiler'],*m['flags'],source,*m['objects'],*m['libraries'],'-o',exe]);runtime(exe,fixture+'-run',[work])
exe=Path(m['gui']['path']);runtime(exe,'gui-smoke',['--hidden','--frames','4','--no-session','--no-auto-diagnostics','--settings',out/'smoke-settings.json','--session-root',out/'smoke-session','--diagnostic-root',out/'diagnostics','--screenshot',out/'smoke.png'])
assert all(sha(p)==h for p,h in inputs.items()) and all(sha(p)==h for p,h in m['inputs'].items()) and all(sha(p)==h for p,h in m['objectSha256'].items())
(out/'acceptance.json').write_text(json.dumps(dict(complete=True,passed=True,rows=rows,inputs=inputs,inputsUnchanged=True,runtime=str(native),temporaryRootExecutablesRemoved=True,gui=m['gui']),indent=2)+'\n')
