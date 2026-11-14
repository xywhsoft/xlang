from pathlib import Path
import hashlib,json,os,re,shutil,subprocess,sys,time
import argparse
from full_compiler_qualification import verify_full_compiler
parser=argparse.ArgumentParser(description='Owned error callback logical close stops following queued commands; full compiler, CPU SDK, three link modes and C/C++ headers.')
for name in('root','compiler','compiler-report','native-root','dynamic-packages','static-packages','out'):
 parser.add_argument('--'+name,type=Path,required=True)
a=parser.parse_args();r,cc,build,native,dynamic,static,out=[v.resolve()for v in(a.root,a.compiler,a.compiler_report,a.native_root,a.dynamic_packages,a.static_packages,a.out)]
win=os.name=='nt';verify_full_compiler(build,cc);sdk=cc.parent/'include';case=r/'test/cases/foreign_xui_error_close.xl';out.mkdir(parents=True,exist_ok=False);sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
files=[Path(__file__),cc,build,case,case.with_suffix('.c'),r/'tools/full_compiler_qualification.py']
files.extend(r/'test/cases'/n for n in('foreign_xui_input_owned.c','foreign_xui_input_owned.h','foreign_xui_input_extended.c','foreign_xui_input_extended.h','foreign_xui_text_layout.c','foreign_xui_text_layout.h'));files.extend(native/'tests/xui'/n for n in('xui_test_proxy.c','xui_test_proxy.h'))
for directory in(sdk,dynamic,static):files.extend(v for v in directory.rglob('*')if v.is_file())
inputs={str(v):sha(v)for v in files};report=dict(complete=False,passed=False,shippingDelivery=False,fullCompilerBuild=True,scope='Language error callback requestClose; malformed queued IME followed by command must not run after logical close.',inputs=inputs,rows=[])
def save():
 p=out/'results.json.tmp';p.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');os.replace(p,out/'results.json')
def run(name,cmd,pkg):
 env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'};env['XLANG_PACKAGE_PATH']=str(pkg);key='PATH'if win else'LD_LIBRARY_PATH';env[key]=str(pkg/'_xge_native/native')+os.pathsep+env.get(key,'');cmd=list(map(str,cmd));start=time.monotonic();z=subprocess.run(cmd,cwd=r,env=env,capture_output=True,timeout=300);log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr);report['rows'].append(dict(name=name,command=cmd,exitCode=z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert not z.returncode,log;return z.stdout+z.stderr
try:
 inc=['--inc-path',r,'--inc-path',sdk,'--inc-path',native];c=out/'close.c';h=out/'close.h';run('emit',[cc,'cgen',case,'--emit',c,'--emit-header',h,*inc],dynamic);assert 'xuiRequestClose('in c.read_text(encoding='utf-8-sig')
 for std,tool,lang in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
  run('header-'+std,[shutil.which(tool),'-std='+std,'-Wall','-Wextra','-Werror',*['-I'+str(v)for v in(r,sdk,sdk/'include',native,native/'include',*sorted((dynamic).glob('*/include')))],'-x',lang,'-fsyntax-only',h],dynamic)
 for kind in('jit','dynamic','static'):
  pkg=static if kind=='static'else dynamic
  if kind=='jit':data=run(kind,[cc,'run',case,*inc],pkg)
  else:
   binary=out/(kind+('.exe'if win else''));run(kind+'-build',[cc,'build',case,'-o',binary,*inc],pkg);data=run(kind,[binary],pkg)
  assert b'InputOwners close:'in data
 verify_full_compiler(build,cc);report.update(complete=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()));report['passed']=report['inputsUnchanged'];assert report['passed']
except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
finally:save()
