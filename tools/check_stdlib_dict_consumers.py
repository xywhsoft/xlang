"""Real native Dict consumers and the shared List/Dict/Set/Array graph memo.

Strict O0/O2, exact empty/NUL/UTF8 keys, retained-reference edges, immutable
snapshots, complete allocation and no-diagnostic scope-entry prefixes, and
compiled implementation controls. Sanitizers cover this native closure only;
neither source Dict factory nor restore cutover is claimed.
"""
from pathlib import Path
import argparse,ast,concurrent.futures,hashlib,json,os,re,shutil,subprocess,sys

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','out'): p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--sanitize',action='store_true'); a=p.parse_args()
 root=a.root.resolve(); out=a.out.resolve(); cc=shutil.which('gcc')
 assert cc and not out.exists() and (not a.sanitize or os.name!='nt')
 population=root/'tools/check_stdlib_list_persistence.py'
 tree=ast.parse(population.read_text(encoding='utf-8'))
 sources=ast.literal_eval(next(n.value for n in ast.walk(tree) if isinstance(n,ast.Assign)
     and any(isinstance(t,ast.Name) and t.id=='sources' for t in n.targets)))
 sources+=['test/cases/stdlib_dict_consumers_native_host.c',
  'src/runtime/xl6_set_graph.c','src/runtime/xl6_dict_graph.c',
  'src/runtime/xl6_value_copy_collections.c','src/runtime/xl6_persistence_project_collections.c',
  'src/runtime/xl6_set_value_iterator.c','src/runtime/xl6_dict_value_iterator.c',
  'src/runtime/xl6_list_value_iterator.c','src/runtime/xl6_collection_value_iterator.c',
  'src/runtime/xl6_value_iterator.c','src/runtime/xl6_array_iterator.c',
  'src/runtime/xl6_array_value_iterator.c','src/runtime/xl6_array.c',
  'src/runtime/xl6_value_contains.c','src/runtime/xl6_value_wire_contains.c',
  'src/runtime/xl6_array_methods.c','src/runtime/xl6_array_method_find.c',
  'src/runtime/xl6_array_method_insert.c','src/runtime/xl6_array_method_remove.c',
  'src/runtime/xl6_dynamic_collection.c','src/runtime/xl6_collection_count_all.c',
  'src/runtime/xl6_collection_count.c']
 sources=list(dict.fromkeys(sources))
 files={Path(__file__).resolve(),population,Path(cc),Path(sys.executable),*[root/s for s in sources]}
 for folder in ('src/runtime','src/api','lib/xrt','lib/xruntime','release/include','test/support'):
  files.update((root/folder).rglob('*.h'))
 files.add(root/'test/support/list_native.c')
 inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
 out.mkdir(parents=True); rows=[]; prefixes=[]; controls=[]; products=[]; objects=[]
 report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,
  NegativeControls=controls,Products=products,Objects=objects,Platform=sys.platform,Sanitized=a.sanitize,
  DictConsumerProviders=True,LanguageDictFactoryCutover=False,DictRestoreCutover=False,Full53Acceptance=False,
  Scope=__doc__,ScopeInjection='GNU ld --wrap; not actual OS context exhaustion')
 flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include','-Isrc/runtime']
 links=['-Wl,--wrap=xl6ErrorScopeBegin','-Wl,--wrap=xl6ErrorScopeBeginCleanup']; env=dict(os.environ)
 if os.name!='nt': flags+=['-D_GNU_SOURCE']; links+=['-pthread','-lm']
 if a.sanitize:
  flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']
  links+=['-fsanitize=address,undefined','-no-pie']; env.update(
   ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 def run(label,command,reject=False):
  command=list(map(str,command)); r=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=240)
  data=r.stdout+r.stderr; log=out/(label+'.log'); log.write_bytes(data)
  rows.append(dict(Label=label,Command=command,Exit=r.returncode,ExpectedRejection=reject,Log=str(log),LogSha256=sha(log)))
  assert (r.returncode!=0 if reject else r.returncode==0),(label,data[-5000:])
  if reject: assert b'assert' in data.lower(),(label,data[-1000:])
  return data.decode(errors='replace')
 try:
  cached={}
  for opt in ('O0','O2'):
   objs=[out/(opt+'-'+str(i)+'.o') for i in range(len(sources))]
   def compile_one(pair):
    i,s=pair; run(opt+'-compile-'+str(i),[cc,*flags,'-'+opt,'-c',s,'-o',objs[i]])
   with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: list(pool.map(compile_one,enumerate(sources)))
   cached[opt]=objs; objects.extend(dict(Path=str(o),Sha256=sha(o)) for o in objs)
   binary=out/('consumers-'+opt+('.exe' if os.name=='nt' else ''))
   run(opt+'-link',[cc,*objs,*links,'-o',binary]); assert '[PASS]' in run(opt+'-vectors',[binary,'vectors'])
   for mode in ('oom','scopes'):
    data=run(opt+'-'+mode,[binary,mode]); found=re.findall(
     r'Dict consumers closed lane=(\d+) mode='+mode+r' runs=(\d+) first-no-hit=(\d+)',data)
    assert len(found)==8
    for lane,runs,first in found:
     lane,runs,first=map(int,(lane,runs,first)); assert runs==first+1
     points=re.findall(r'Dict consumers lane='+str(lane)+r' mode='+mode+r' point=(\d+) hit=(\d) balanced',data)
     assert points==[(str(i),str(int(i<first))) for i in range(runs)]
     prefixes.append(dict(Optimization=opt,Mode=mode,Lane=lane,Runs=runs,FirstNoHit=first))
   products.append(dict(Path=str(binary),Sha256=sha(binary)))
  for name,source,before,after in (
   ('aliased-dict-copy','src/runtime/xl6_value_copy.c','dict ? xl6DictErasedCloneV1(source)','dict ? xrtValueRetain(source)'),
   ('dict-provider-omitted','src/runtime/xl6_value_copy_collections.c','xl6DictGraphProviderV1()','xl6SetGraphProviderV1()'),
   ('generic-dict-snapshot-omitted','src/runtime/xl6_collection_value_iterator.c',
    'xl6DictValueIteratorNativeOpsV1Get()->Create(source,output)','0'),
   ('dict-key-nul-truncated','src/runtime/xl6_dict_value_iterator.c','key->String=text;',
    'key->String=text; key->String.Size=strlen(text.Data);')):
   body=(root/source).read_text(encoding='utf-8'); assert before in body,name
   replacement=body.replace(before,after)
   if name=='dict-key-nul-truncated': replacement='#include <string.h>\n'+replacement
   changed=out/(name+'.c'); changed.write_text(replacement,encoding='utf-8')
   obj=out/(name+'.o'); run(name+'-compile',[cc,*flags,'-O0','-c',changed,'-o',obj])
   objects.append(dict(Path=str(obj),Sha256=sha(obj))); objs=list(cached['O0']); objs[sources.index(source)]=obj
   binary=out/(name+('.exe' if os.name=='nt' else ''))
   run(name+'-link',[cc,*objs,*links,'-o',binary]); run(name+'-reject',[binary,'vectors'],True)
   controls.append(dict(Name=name,Compiled=True,Rejected=True,Source=str(changed),SourceSha256=sha(changed),
    Product=str(binary),ProductSha256=sha(binary)))
  report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
  report['Complete']=report['InputsUnchanged'] and len(controls)==4 and len(prefixes)==32; assert report['Complete']
 finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 print('Dict consumers: O0/O2, 32 full prefixes, four real mutants rejected',flush=True)

if __name__=='__main__': main()
