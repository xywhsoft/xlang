"""Offline selected-provider NetEngine typed producer/rollback acceptance.
No shipping/public language acceptance, network tests or ALL native selector.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
 p.add_argument('--out',type=Path,required=True);p.add_argument('--compiler',action='append',required=True);p.add_argument('--sanitize',action='store_true')
 a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists()
 compilers=[Path(shutil.which(c) or c).resolve() for c in a.compiler];gcc=next(c for c in compilers if c.stem.lower()!='tcc')
 def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
 files={Path(__file__).resolve(),*compilers,*[root/('test/support/'+n) for n in ('net_task_language_plan.c','net_task_language_native.c','task_net_frame_outcome.c','executor_catalog_plan.c')]}
 for folder in ('src','lib/xrt','lib/xruntime','release/include'):
  files.update(f for f in (root/folder).rglob('*') if f.is_file() and f.suffix in ('.h','.c','.inc'))
 inputs={str(f):sha(f) for f in sorted(files)};out.mkdir(parents=True);rows=[];products=[];failures=[]
 report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Failures=failures,
  Sources=[],Modules=[],InstrumentedSelectedClosure=False,ShippingAcceptance=False,NetworkTests=False,Scope=__doc__)
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
 def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
 def run(name,args,proof=None,instrumented=False):
  log=out/(name+'.log');env=dict(os.environ);started=time.monotonic()
  if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
  with log.open('wb') as stream:r=subprocess.run(list(map(str,args)),cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
  rows.append(dict(Name=name,Exit=r.returncode,Command=list(map(str,args)),Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started));save()
  print(name+': '+str(r.returncode),flush=True);assert r.returncode==0,(name,str(log))
  text=log.read_text(encoding='utf8',errors='replace')
  if proof:assert proof in text,(name,'missing proof')
  return text
 def flags(cc):return [cc,'-std=c11','-Wall','-Werror',*(['-Wextra'] if cc.stem.lower()!='tcc' else []),*(['-D_GNU_SOURCE'] if os.name!='nt' else []),'-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(out)]
 libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm'];ext='.exe' if os.name=='nt' else ''
 save()
 try:
  plan=out/('plan'+ext)
  run('plan.build',flags(gcc)+['-O2','-Wno-missing-field-initializers','-ffunction-sections','-fdata-sections','-fno-asynchronous-unwind-tables','-fno-unwind-tables',*(['-fno-jump-tables'] if os.name=='nt' else []),root/'test/support/net_task_language_plan.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',plan]);product(plan)
  run('plan.run',[plan],'NetTask language plan: actual typed')
  names=run('sources',[plan,'--sources']).splitlines();assert len(names)==len(set(names)) and 'src/runtime/xl6_task_net_language.c' in names
  modules=run('modules',[plan,'--modules']);assert '_MODULE_ALL' not in modules
  report.update(Sources=names,Modules=modules.splitlines());save()
  header=out/'task-modules.h';header.write_text(modules,encoding='utf8');product(header)
  kernel=out/'native.c';kernel.write_text('#include "task-modules.h"\n#ifndef XL6_TASK_CHECKED_MINIMAL\n#define XRT_MODULE_MEMORY_DEBUG\n#endif\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n',encoding='utf8');product(kernel)
  providers=[root/n for n in names];fixture=root/'test/support/net_task_language_native.c'
  for index,cc in enumerate(compilers):
   for opt in ('O0','O2'):
    for minimal in (False,True):
     name=str(index)+'.'+opt+('.minimal' if minimal else '.checked');binary=out/(name+ext)
     run(name+'.build',flags(cc)+['-'+opt,*(['-DXL6_TASK_CHECKED_MINIMAL'] if minimal else []),fixture,*providers,kernel,*libs,'-o',binary]);product(binary)
     run(name+'.run',[binary],'NetTask language resident: 72')
  if a.sanitize:
   assert os.name!='nt';binary=out/'instrumented'
   run('instrumented.build',flags(gcc)+['-O1','-g','-fno-omit-frame-pointer','-fsanitize=address,undefined','-fno-pie','-no-pie',fixture,*providers,kernel,*libs,'-o',binary]);product(binary)
   run('instrumented.run',[binary],'NetTask language resident: 72',instrumented=True);report['InstrumentedSelectedClosure']=True
  report['InputsUnchanged']=all(sha(f)==h for f,h in inputs.items());report['LogsUnchanged']=all(sha(r['Log'])==r['Sha256'] for r in rows)
  assert report['InputsUnchanged'] and report['LogsUnchanged'];report['Complete']=True;save()
 except BaseException as error:failures.append(repr(error));save();raise
if __name__=='__main__':main()
