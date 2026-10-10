"""Real native Set methods and exact checked import, not full53/shipping acceptance.

O0/O2, 13 complete allocation and no-diagnostic scope-entry prefixes per level,
and two compiled implementation controls. Optional sanitizers cover the selected
native providers only. Scope refusal uses GNU ld wrap, not OS exhaustion.
"""
import argparse,ast,concurrent.futures,hashlib,json,os,re,shutil,subprocess,sys
from pathlib import Path
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def assigned(tree):
 return ast.literal_eval(next(n.value for n in ast.walk(tree) if isinstance(n,ast.Assign)
  and any(isinstance(t,ast.Name) and t.id=='sources' for t in n.targets)))
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','out'): p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--sanitize',action='store_true'); a=p.parse_args()
 root=a.root.resolve(); out=a.out.resolve(); cc=shutil.which('gcc'); assert cc and not out.exists()
 assert not a.sanitize or os.name!='nt'
 population=root/'tools/check_stdlib_list_persistence.py'; consumers=root/'tools/check_stdlib_set_consumers.py'
 sources=assigned(ast.parse(population.read_text(encoding='utf-8')))
 sources[sources.index('test/support/value_copy_native.c')]='test/support/set_consumers_native.c'
 tree=ast.parse(consumers.read_text(encoding='utf-8'))
 sources+=ast.literal_eval(next(n.value for n in ast.walk(tree) if isinstance(n,ast.AugAssign)
  and isinstance(n.target,ast.Name) and n.target.id=='sources'))
 sources[sources.index('test/cases/stdlib_set_consumers_native_host.c')]='test/cases/stdlib_set_source_native_host.c'
 sources+=['src/runtime/xl6_set_methods.c','src/runtime/xl6_set_import_chain.c',
  'src/runtime/xl6_array_import_chain.c','src/runtime/xl6_array_input.c','src/runtime/xl6_array_sequence_admission.c',
  'src/runtime/xl6_collection_count.c','src/runtime/xl6_collection_count_all.c']
 sources=list(dict.fromkeys(sources))
 files={Path(__file__).resolve(),population,consumers,Path(cc),Path(sys.executable),*[root/s for s in sources],
  root/'test/cases/stdlib_set_consumers_native_host.c',root/'test/support/list_native.c'}
 for folder in ('src/runtime','src/api','lib/xrt','lib/xruntime','release/include','test/support'):
  files.update((root/folder).rglob('*.h'))
 inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
 out.mkdir(parents=True); rows=[]; prefixes=[]; controls=[]; objects=[]; products=[]
 report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,
  NegativeControls=controls,Objects=objects,Products=products,Sanitized=a.sanitize,Full53Acceptance=False,ShippingAcceptance=False)
 flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-I.','-Irelease','-Irelease/include','-Isrc/runtime']
 links=['-Wl,--wrap=xl6ErrorScopeBegin','-Wl,--wrap=xl6ErrorScopeBeginCleanup']; env=dict(os.environ)
 if os.name!='nt': flags+=['-D_GNU_SOURCE']; links+=['-pthread','-lm']
 if a.sanitize:
  flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']; links+=['-fsanitize=address,undefined','-no-pie']
  env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 def run(name,command,reject=False):
  command=list(map(str,command)); r=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
  data=r.stdout+r.stderr; log=out/(name+'.log'); log.write_bytes(data)
  rows.append(dict(Name=name,Command=command,Exit=r.returncode,ExpectedRejection=reject,Log=str(log),LogSha256=sha(log)))
  assert (r.returncode!=0 if reject else r.returncode==0),(name,data[-6000:])
  if reject: assert b'assert' in data.lower()
  return data.decode(errors='replace')
 try:
  cache={}
  for opt in ('O0','O2'):
   objs=[out/(opt+'-'+str(i)+'.o') for i in range(len(sources))]
   def compile_one(pair):
    i,s=pair; run(opt+'.compile.'+str(i),[cc,*flags,'-'+opt,'-c',s,'-o',objs[i]])
   with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: list(pool.map(compile_one,enumerate(sources)))
   cache[opt]=objs; objects.extend(dict(Path=str(o),Sha256=sha(o)) for o in objs)
   binary=out/('source-'+opt+('.exe' if os.name=='nt' else '')); run(opt+'.build',[cc,*objs,*links,'-o',binary])
   assert '[PASS]' in run(opt+'.vectors',[binary,'vectors'])
   for mode in ('oom','scopes'):
    data=run(opt+'.'+mode,[binary,mode]); found=re.findall(r'Set source closed lane=(\d+) mode='+mode+r' runs=(\d+) first-no-hit=(\d+)',data)
    assert len(found)==13
    for lane,runs,first in found:
     lane,runs,first=map(int,(lane,runs,first)); assert runs==first+1
     points=re.findall(r'Set source lane='+str(lane)+r' mode='+mode+r' point=(\d+) hit=(\d) balanced',data)
     assert points==[(str(i),str(int(i<first))) for i in range(runs)]
     prefixes.append(dict(Optimization=opt,Mode=mode,Lane=lane,Runs=runs,FirstNoHit=first))
   products.append(dict(Path=str(binary),Sha256=sha(binary))); print(opt+' native source complete',flush=True)
  for name,source,before,after in (
   ('aliased-static-copy','src/runtime/xl6_set_methods.c','finishValue(&scope,xl6SetErasedCloneV1(source))','finishValue(&scope,xrtValueRetain(source))'),
   ('missing-native-input','src/runtime/xl6_set_import_chain.c','append,inputBegin,inputEnd,inputConversion','append,NULL,inputEnd,inputConversion')):
   body=(root/source).read_text(encoding='utf-8'); assert body.count(before)==1
   changed=out/(name+'.c'); changed.write_text(body.replace(before,after),encoding='utf-8')
   obj=out/(name+'.o'); run(name+'.compile',[cc,*flags,'-O0','-c',changed,'-o',obj]); objs=list(cache['O0']); objs[sources.index(source)]=obj
   binary=out/(name+('.exe' if os.name=='nt' else '')); run(name+'.build',[cc,*objs,*links,'-o',binary]); run(name+'.reject',[binary,'vectors'],True)
   controls.append(dict(Name=name,Compiled=True,Rejected=True,Source=str(changed),SourceSha256=sha(changed),Product=str(binary),ProductSha256=sha(binary)))
  report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
  report['Complete']=report['InputsUnchanged'] and len(prefixes)==52 and len(controls)==2
 except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
 finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
