"""Selected-source Socket ownership/IO acceptance. Only own loopback dynamic
ports, ordinary lifecycle/bytes/OOM checks; no external targets/security tests.
Not fresh shipping product or complete network-module acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
 p.add_argument('--out',type=Path,required=True);p.add_argument('--compiler',action='append',required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
 root=a.root.resolve();out=a.out.resolve();assert not out.exists() and (not a.sanitize or os.name!='nt')
 compilers=[Path(shutil.which(cc) or cc).resolve() for cc in a.compiler];gcc=next(c for c in compilers if c.stem.lower()!='tcc')
 def sha(f):return hashlib.sha256(Path(f).read_bytes()).hexdigest()
 files={Path(__file__).resolve(),*compilers,root/'test/support/net_socket_plan.c',root/'test/support/net_socket_runtime.c',root/'test/support/net_socket_close_failure.c',root/'test/support/executor_catalog_plan.c'}
 for folder in ('src','lib/xrt','lib/xruntime','release/include'):
  files.update(f for f in (root/folder).rglob('*') if f.is_file() and f.suffix in ('.c','.h','.inc'))
 inputs={str(f):sha(f) for f in sorted(files)};out.mkdir(parents=True);rows=[];products=[];failures=[]
 report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Failures=failures,
  Plans={},NegativeControls=[],OwnLoopbackOnly=True,NetworkSecurityTests=False,ExternalNetworkTests=False,
  InstrumentedSelectedClosure=False,ShippingLibraryAcceptance=False,Scope=__doc__)
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
 def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
 def run(name,args,proof=None,reject=False,sanitize=False):
  log=out/(name+'.log');env=dict(os.environ);started=time.monotonic()
  if sanitize:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
  with log.open('wb') as stream:r=subprocess.run(list(map(str,args)),cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
  rows.append(dict(Name=name,Command=list(map(str,args)),Exit=r.returncode,ExpectedRejection=reject,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started));save()
  print(name+': '+str(r.returncode),flush=True);assert (r.returncode!=0 if reject else r.returncode==0),log
  text=log.read_text(encoding='utf8',errors='replace')
  if reject:assert 'assert' in text.lower(),log
  if proof:assert proof in text,log
  return text
 def flags(cc):
  result=[cc,'-std=c11','-Wall','-Werror','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(out)]
  if cc.stem.lower()!='tcc':result+=['-Wextra']
  if os.name!='nt':result+=['-D_GNU_SOURCE']
  return result
 libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm'];ext='.exe' if os.name=='nt' else ''
 save()
 try:
  for opt in ('O0','O2'):
   plan=out/('plan.'+opt+ext)
   run('plan.'+opt+'.build',flags(gcc)+['-'+opt,'-Wno-missing-field-initializers','-ffunction-sections','-fdata-sections','-fno-jump-tables',
    '-fno-asynchronous-unwind-tables','-fno-unwind-tables',root/'test/support/net_socket_plan.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',plan]);product(plan)
   run('plan.'+opt+'.run',[plan],'Socket plan: exact physical owner/handle domains')
   if opt=='O0':
    for name in ('owner','control','io'):
     sources=run(name+'.sources',[plan,'--sources',name]).splitlines();assert len(sources)==len(set(sources));report['Plans'][name]=sources
     modules=run(name+'.modules',[plan,'--modules',name]);assert '_MODULE_ALL' not in modules
     domains=run(name+'.domains',[plan,'--domains',name])
     if name=='io':
      for file,text in (('net-socket-modules.h',modules),('net-socket-domains.h',domains)):
       path=out/file;path.write_text(text,encoding='utf8');product(path)
  providers=[root/s for s in report['Plans']['io']];proof='Socket runtime: 8 guarded owner lifetimes, 16 graph/restore orders'
  for i,cc in enumerate(compilers):
   for opt in ('O0','O2'):
    name=f'{i}.{opt}';binary=out/(name+ext)
    run(name+'.build',flags(cc)+['-'+opt,root/'test/support/net_socket_runtime.c',*providers,*libs,'-o',binary]);product(binary)
    run(name+'.run',[binary],proof)
    closeBinary=out/(name+'.close'+ext)
    closeSources=[f for f in providers if f.name!='xl6_net_socket_owner.c']
    run(name+'.close.build',flags(cc)+['-'+opt,root/'test/support/net_socket_close_failure.c',*closeSources,*libs,'-o',closeBinary]);product(closeBinary)
    run(name+'.close.run',[closeBinary],'Socket close: 8 consumed error/NULL-error paths')
  controls=(('control-read-overlap','xl6_net_socket_owner.c','&&!owner->control&&(!control||!owner->active)','&&(!control||!owner->active)'),
   ('active-not-busy','xl6_net_socket_owner.c','&&(owner->active||owner->closing);','&&false;'),
   ('copied-ops-admitted','xl6_net_socket_owner.c','ref.Ops==&ownershipOps&&count(ref.Data,&refs)','ref.Ops&&count(ref.Data,&refs)'),
   ('finish-native-leak','xl6_net_socket_owner.c','return !native||xrtNetSocketClose(native);','(void)native;return true;'),
   ('text-nul-truncated','xl6_net_socket_io.c','input.Data,input.Size},size,NULL)','input.Data,strlen(input.Data)},size,NULL)'))
  for name,filename,old,new in controls:
   source=root/'src/runtime'/filename;body=source.read_text(encoding='utf8');assert body.count(old)==1,(name,'inexact mutation')
   mutant=out/(name+'.c');mutant.write_text(body.replace(old,new),encoding='utf8');product(mutant)
   selected=[mutant if f==source else f for f in providers];binary=out/(name+ext)
   run(name+'.build',flags(gcc)+['-O0','-I'+str(root/'src/runtime'),root/'test/support/net_socket_runtime.c',*selected,*libs,'-o',binary]);product(binary)
   run(name+'.refused',[binary],reject=True);report['NegativeControls'].append(name);save()
  if a.sanitize:
   binary=out/'instrumented';run('instrumented.build',flags(gcc)+['-O1','-g','-fno-omit-frame-pointer','-fsanitize=address,undefined','-fno-pie','-no-pie',
    root/'test/support/net_socket_runtime.c',*providers,*libs,'-o',binary]);product(binary);run('instrumented.run',[binary],proof,sanitize=True);report['InstrumentedSelectedClosure']=True
  report['InputsUnchanged']=all(sha(f)==h for f,h in inputs.items());report['LogsUnchanged']=all(sha(r['Log'])==r['Sha256'] for r in rows)
  assert report['InputsUnchanged'] and report['LogsUnchanged'];report['Complete']=True;save()
 except BaseException as error:failures.append(repr(error));save();raise
if __name__=='__main__':main()
