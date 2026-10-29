"""Offline pinned XRT/xruntime pool exit ABI and actual thread-tail acceptance.
No public-language/worker-exit integration, shipping or network acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
 p.add_argument('--out',type=Path,required=True);p.add_argument('--compiler',action='append',required=True);p.add_argument('--sanitize',action='store_true')
 a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists()
 def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
 compilers=[Path(shutil.which(n) or n).resolve() for n in a.compiler]
 files={Path(__file__).resolve(),*compilers,root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h',root/'test/support/task_pool_worker_exit_native.c'}
 for folder in ('release/include',):files.update(path for path in (root/folder).rglob('*') if path.is_file() and path.suffix=='.h')
 inputs={str(f):sha(f) for f in sorted(files)};out.mkdir(parents=True);rows=[];products=[];failures=[]
 report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Failures=failures,
  ManagedWorkerExitAccepted=False,ShippingAcceptance=False,NetworkTests=False,Scope=__doc__)
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
 def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
 def run(name,args,instrumented=False):
  log=out/(name+'.log');started=time.monotonic();env=dict(os.environ)
  if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
  with log.open('wb') as stream:r=subprocess.run(list(map(str,args)),cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
  rows.append(dict(Name=name,Exit=r.returncode,Command=list(map(str,args)),Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started));save()
  print(name+': '+str(r.returncode),flush=True);assert r.returncode==0,(name,str(log))
  if name.endswith('.run'):assert 'Pinned pool exit: 16 actual-worker' in log.read_text(encoding='utf8')
 libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm'];ext='.exe' if os.name=='nt' else ''
 def flags(cc):return [cc,'-std=c11','-Wall','-Werror',*(['-Wextra'] if cc.stem!='tcc' else []),*(['-D_GNU_SOURCE'] if os.name!='nt' else []),'-I'+str(root),'-I'+str(root/'release/include')]
 save()
 try:
  for kind in ('xrt','xruntime'):
   source=out/(kind+'.c');source.write_text('#define XRT_MODULE_TASK_POOL\n#define XRT_MODULE_THREAD_KEY\n#define XRT_MODULE_ATOMIC\n#define XRT_MODULE_MEMORY_DEBUG\n'+
    ('#define XRUNTIME_MODULE_RUNTIME_TYPE\n' if kind=='xruntime' else '')+'#define '+kind.upper()+'_IMPLEMENTATION\n#include "lib/'+kind+'/'+kind+'.h"\n#include "test/support/task_pool_worker_exit_native.c"\n',encoding='utf8');product(source)
   for index,cc in enumerate(compilers):
    for opt in ('O0','O2'):
     name=kind+'.'+str(index)+'.'+opt;binary=out/(name+ext)
     run(name+'.build',flags(cc)+['-'+opt,source,*libs,'-o',binary]);product(binary);run(name+'.run',[binary])
   if a.sanitize:
    assert os.name!='nt';cc=next(c for c in compilers if c.stem!='tcc');name=kind+'.instrumented';binary=out/name
    run(name+'.build',flags(cc)+['-O1','-g','-fno-omit-frame-pointer','-fsanitize=address,undefined','-fno-pie','-no-pie',source,*libs,'-o',binary]);product(binary);run(name+'.run',[binary],True)
  report['InputsUnchanged']=all(sha(f)==h for f,h in inputs.items());report['LogsUnchanged']=all(sha(r['Log'])==r['Sha256'] for r in rows)
  assert report['InputsUnchanged'] and report['LogsUnchanged'];report['Complete']=True;save()
 except BaseException as error:failures.append(repr(error));save();raise
if __name__=='__main__':main()
