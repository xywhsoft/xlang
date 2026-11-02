from pathlib import Path
import hashlib,json,os,shutil,subprocess
R=Path('D:/GIT/x-lang');O=R/'ide/build/Release/wgl-compat-20261102';S=O/'source';E=Path(__file__).resolve().parent;SDK=E/'sdk';F=E/'faults';F.mkdir()
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
b=json.loads((S/'build/ide/build-report.json').read_text());gui=json.loads((R/'ide/build/Release/gui-build-inputs.json').read_text());cc=b['compiler'][0]
record=dict(status='running',scope='Test-only WGL producer refusals and a driver shim that rejects requests above3.3; no fault hooks in production SDK.',commands=[],cases=[],inputs={str(p):sha(p) for p in [R/'ide/tests/wgl_fault_hooks.c',R/'ide/tests/wgl_fault_hooks.h',R/'ide/tests/wgl_startup_test.c',Path(__file__)]})
def save():(E/'wgl-check.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
def run(name,args,cwd=S,env=None,timeout=180):
 log=E/(name+'.log');row=dict(name=name,command=list(map(str,args)),log=str(log));record['commands'].append(row);save()
 with log.open('wb') as stream:
  p=subprocess.Popen(list(map(str,args)),cwd=cwd,env=env,stdout=stream,stderr=subprocess.STDOUT);row['pid']=p.pid;save()
  try:p.wait(timeout=timeout)
  except BaseException:p.kill();p.wait();raise
 row.update(exit=p.returncode,sha256=sha(log));save()
 if p.returncode:raise ValueError(name+': '+log.read_text('utf8',errors='replace')[-2200:])
 return log
try:
 run('fault-hooks-compile',[cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-cast-function-type','-I'+str(R/'ide/tests'),'-c',R/'ide/tests/wgl_fault_hooks.c','-o',F/'hooks.o'])
 run('fault-main-compile',[cc,*b['flags'],'-DXIDE_WGL_FAULT_IMPORTS','-include',R/'ide/tests/wgl_fault_hooks.h','-c','single/xge.c','-o',F/'main.o'])
 objects=[S/'build/ide/obj'/(name.replace('/','_')+'.o') for name in b['sources']]
 assert objects[0].name=='single_xge.c.o';objects[0]=F/'main.o'
 objects += [S/'build/shared/obj/extlibs_harfbuzz_src_harfbuzz.cc.o',S/'build/ide/obj/xge_res.o',F/'hooks.o']
 libs=['-lm','-lws2_32','-liphlpapi','-lgdi32','-luser32','-lshell32','-lole32','-loleaut32','-luuid','-limm32','-lwinmm','-lavrt','-lopengl32']
 run('fault-dll-link',[cc,'-shared','-o',F/'xge.dll',*objects,*libs])
 flags=[v.replace(str(R/'ide/third_party/xge_sdk'),str(SDK)) for v in gui['flags']]
 run('probe-compile',[cc,*flags,'-Wno-cast-function-type','-I'+str(R/'ide/tests'),R/'ide/tests/wgl_startup_test.c',SDK/'xge.lib','-luser32','-o',F/'probe.exe'])
 for mode in ['compat33','missing-library','missing-export','dummy-create','dummy-current','missing-attrib','create','real-current']:
  env=dict(os.environ,XIDE_WGL_FAULT=mode);log=run('case-'+mode,[F/'probe.exe',mode],cwd=R/'release',env=env,timeout=30)
  record['cases'].append(dict(mode=mode,status='passed',output=log.read_text('utf8',errors='replace')[-1600:]));save();print(mode,'passed',flush=True)
 for p,d in record['inputs'].items():assert sha(p)==d,p
 record.update(status='passed',faultDllSha256=sha(F/'xge.dll'),productionDllSha256=sha(SDK/'xge.dll'));save()
except BaseException as error:
 record.update(status='failed',error=repr(error));save();raise
