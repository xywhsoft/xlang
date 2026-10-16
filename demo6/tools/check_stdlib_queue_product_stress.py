"""Real shared-product queue roles, owned transport and actor-local fault prefixes.
Both runtime/compiler libraries at O0/O2, SPSC/MPSC/MPMC plus WaitQueue MPMC,
exact NUL text, single/batch delivery, close/drain and physical lifetime ledgers.
Source-free language JIT/module/relocated AOT and genuine-SO family plans.
No provider interposition, compiler source objects, scheduler-wide OOM or full53 claim.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,re,shutil,subprocess,sys,time

FAMILIES=('spsc','mpsc','mpmc','wait_queue')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','build-report','out'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--jobs',type=int,default=2);a=p.parse_args();root,out=a.root.resolve(),a.out.resolve()
 assert not out.exists() and 1<=a.jobs<=4;task=Path(__file__).resolve().parents[1];win=os.name=='nt'
 cc,inspect=[shutil.which(n) for n in ('gcc','objdump' if win else 'readelf')];assert cc and inspect
 sys.path.insert(0,str(root/'tools'));from gen_tcc_vfs_resources import load_resources
 build=json.loads(a.build_report.read_text(encoding='utf-8-sig'));assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
 products=[root/'release'/n for n in (('xl.exe','xl.dll','xlc.dll') if win else ('xl','libxl.so','libxlc.so'))]
 recorded={Path(r['Path']).resolve():r['Sha256'].lower() for r in build['Products']};assert all(recorded.get(f)==sha(f) for f in products)
 native={n:task/'test/cases'/f for n,f in (('kernel','stdlib_concurrent_queue_native_host.c'),('wait','stdlib_wait_queue_native.c'),('actor','stdlib_queue_actor_fault_host.c'))}
 memory=root/'test/support/module_product_memory.c';observer=task/'test/cases/stdlib_internal_container_product_host.c';planner=task/'test/cases/stdlib_queue_product_plan.c'
 sources={n:task/'test/cases'/('stdlib_'+n+'_pressure.xl') for n in FAMILIES}
 files={Path(__file__).resolve(),Path(sys.executable),Path(cc),Path(inspect),a.build_report.resolve(),memory,observer,planner,*native.values(),*sources.values()}
 for r in build['Inputs']+build['Products']:assert sha(r['Path'])==r['Sha256'].lower();files.add(Path(r['Path']))
 inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)];out.mkdir();isolated=out/'isolated';release=isolated/'release';release.mkdir(parents=True)
 sdk,tmp,moved=[out/n for n in ('sdk','tmp','relocated')]
 for d in (sdk,tmp,moved):d.mkdir()
 for f in products:
  shutil.copy2(f,release/f.name)
  if win and f.suffix=='.dll':shutil.copy2(f,isolated/f.name)
 for f in (root/'release/include').glob('*.h'):shutil.copy2(f,sdk/f.name)
 bd=root/('build/tmp' if win else 'build/linux');manifest=root/'tools'/('tcc_vfs_manifest_win64_runtime.txt' if win else 'tcc_vfs_manifest_linux_runtime.txt');dist=root/'res/tcc/win64/x86_64' if win else bd/'tcc-dist'
 for r in load_resources(manifest,root,dist,bd/'xrt.o',bd):
  if r.virtual.startswith('include/__xl_runtime/'):
   target=sdk/Path(r.virtual).relative_to('include');target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(r.source,target)
 shutil.copy2(root/'lib/xruntime/xruntime.h',sdk/'xruntime.h')
 env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')};env.update(TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp))
 rows=[];failures=[];prefixes=[];lanes=[];language=[];plans=[]
 report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,Failures=failures,Prefixes=prefixes,Lanes=lanes,Language=language,Plans=plans,
  ProviderOverrides=False,CompilerSourceObjects=False,RuntimeInstrumented=False,SchedulerWideOomAcceptance=False,Full53Acceptance=False)
 def run(tag,cmd,timeout=180):
  cmd=list(map(str,cmd));start=time.monotonic();r=subprocess.run(cmd,cwd=isolated,env=env,capture_output=True,timeout=timeout);log=out/(tag+'.log');log.write_bytes(r.stdout+r.stderr)
  rows.append(dict(Name=tag,Command=cmd,Exit=r.returncode,Seconds=time.monotonic()-start,Log=str(log),LogSha256=sha(log)))
  assert r.returncode==0,(tag,(r.stdout+r.stderr)[-5000:]);return (r.stdout+r.stderr).decode(errors='replace')
 def fail(tag,error):failures.append(dict(Name=tag,Error=repr(error)));print(tag+': FAILED '+repr(error),flush=True)
 def links(product):return [release/product.name] if win else ['-L'+str(release),'-l'+product.stem[3:],'-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
 flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(sdk)]+([] if win else ['-D_GNU_SOURCE'])
 nativeflags=[*flags,'-DXL6_QUEUE_PRODUCT_TEST','-I'+str(root),'-I'+str(root/'release')]
 compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
 def prefix(tag,binary,args,kind,product,opt,family,operation):
  for point in range(129):
   text=run(tag+'.'+str(point),[binary,*args,point]);match=re.search(r'\bhit=(\d+)\b',text);assert match,(tag,text)
   if match[1]=='0':
    assert point>0,tag+' never hit actual allocator failure'
    prefixes.append(dict(Kind=kind,Product=product.name,Optimization=opt,Family=family,Operation=operation,FirstNoHit=point,Runs=point+1));return
  raise AssertionError('allocation prefix did not close '+tag)
 def native_lane(job):
  product,opt=job;tag=product.stem+'.'+opt;transport=[]
  for kind in ('kernel','wait'):
   try:
    binary=isolated/(tag+'.'+kind+('.exe' if win else ''));run(tag+'.'+kind+'.build',[cc,*nativeflags,'-'+opt,native[kind],memory,*links(product),'-o',binary])
    if kind=='kernel':assert 'typed queue kernel:' in run(tag+'.vectors',[binary])
    for repeat in range(2):
     text=run(tag+'.'+kind+'.overlap.'+str(repeat),[binary,*(['threads'] if kind=='kernel' else [])])
     if kind=='kernel':
      matches=re.findall(r'transport family=(\d+) text=(\d+) batch=(\d+) producers=(\d+) consumers=(\d+) delivered=(\d+) fifo=(\d+) balanced',text);assert len(matches)==12
      for row in matches:
       family,t,b,producers,consumers,delivered,fifo=map(int,row);assert family<3 and t<2 and b<2
       assert producers==(1 if family==0 else 4) and consumers==(4 if family==2 else 1) and delivered==producers*1024 and fifo==int(family!=2)
       transport.append(dict(Family=FAMILIES[family],Text=t,Batch=b,Delivered=delivered,Repeat=repeat))
     else:
      matches=re.findall(r'wait-transport text=(\d+) producers=4 consumers=4 delivered=1024 closed-drained',text);assert sorted(matches)==['0','1']
      transport.extend(dict(Family='wait_queue',Text=int(t),Batch=0,Delivered=1024,Repeat=repeat) for t in matches)
    for family in range(3 if kind=='kernel' else 1):
     for operation in range(5 if kind=='kernel' else 2):
      try:prefix(tag+'.'+kind+'.f'+str(family)+'.o'+str(operation),binary,[family,operation] if kind=='kernel' else ['fault',operation],kind,product,opt,FAMILIES[family] if kind=='kernel' else 'wait_queue',operation)
      except Exception as e:fail(tag+'.'+kind+'.prefix.'+str(family)+'.'+str(operation),e)
   except Exception as e:fail(tag+'.'+kind,e)
  for family,name in zip(FAMILIES,('SpscQueue','MpscQueue','MpmcQueue')):
   try:
    binary=isolated/(tag+'.actor.'+family+('.exe' if win else ''));run(tag+'.actor.'+family+'.build',[cc,*nativeflags,'-'+opt,'-DXL6_TEST_QUEUE=xl6'+name,native['actor'],memory,*links(product),'-o',binary])
    for operation in range(3):
     try:prefix(tag+'.actor.'+family+'.o'+str(operation),binary,[operation,'oom'],'actor',product,opt,family,operation)
     except Exception as e:fail(tag+'.actor.'+family+'.prefix.'+str(operation),e)
   except Exception as e:fail(tag+'.actor.'+family,e)
  lanes.append(dict(Product=product.name,Optimization=opt,Transport=transport));print(tag+': native lane finished',flush=True)
 try:
  with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:list(pool.map(native_lane,[(p,o) for p in products[1:] for o in ('O0','O2')]))
  binary=isolated/('observer'+('.exe' if win else ''));compiler=products[2];run('observer.build',[cc,*flags,'-O2',observer,*links(compiler),'-o',binary])
  assert not any((isolated/n).exists() for n in ('src','lib','res','include'))
  for family,source in sources.items():
   try:
    local=isolated/source.name;shutil.copy2(source,local);image=isolated/(family+('.exe' if win else ''))
    for mode in ('run','module','aot'):
     work=tmp/(family+'-'+mode);work.mkdir();assert 'internal-container '+mode+' complete' in run(family+'.'+mode,[binary,release/compiler.name,local,mode,work,image])
    for opt in ('O0','O2'):
     for f in ('module.c','xl6_xrt_impl.c'):run(family+'.'+opt+'.'+f,[cc,*flags,*compat,'-'+opt,'-c',work/f,'-o',out/(family+'.'+opt+'.'+f+'.o')])
    target=moved/image.name;shutil.move(image,target);text=run(family+'.imports',[inspect,'-p' if win else '-d',target])
    deps=re.findall(r'DLL Name:\s*(\S+)',text) if win else re.findall(r'\(NEEDED\).*?\[(.*?)\]',text)
    assert not any(n.lower() in ('xl.dll','xlc.dll') or 'libxl' in n.lower() for n in deps);assert win or not re.search(r'\((?:RPATH|RUNPATH)\)',text)
    run(family+'.relocated',[target]);language.append(dict(Family=family,Jit=True,Module=True,RelocatedAot=True,StrictC=['O0','O2']));print(family+': language passed',flush=True)
   except Exception as e:fail(family+'.language',e)
  if not win:
   binary=isolated/'planner';run('planner.build',[cc,*nativeflags,'-I'+str(root/'lib/tcc'),'-O0',planner,*links(compiler),'-o',binary])
   for family,source in sources.items():
    text=run('plan.'+family,[binary,source]);selected=text.splitlines()
    for other in FAMILIES:assert ('src/runtime/xl6_'+('wait_queue' if other=='wait_queue' else 'queue_'+other)+'.c' in selected)==(other==family),(family,other)
    assert 'src/runtime/xl6_channel.c' not in selected
    plans.append(dict(Family=family,Sources=selected))
  report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
  report['NativeRoleStressAcceptance']=len(lanes)==4 and all(len(l['Transport'])==28 for l in lanes)
  report['CompleteFaultPrefixes']=len(prefixes)==104
  report['Complete']=report['InputsUnchanged'] and report['NativeRoleStressAcceptance'] and report['CompleteFaultPrefixes'] and len(language)==4 and (win or len(plans)==4) and not failures
 except Exception as e:fail('gate',e)
 report['PhaseCount']=len(rows);report['Products']=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(out.rglob('*')) if f.is_file()]
 (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(Complete=report['Complete'],Phases=len(rows),Failures=failures)),flush=True)
 return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
