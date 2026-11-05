"""Build three CPU comparison programs with one selected O2 runtime closure.

This preparation performs correctness execution only. It publishes no timing
ratio and is never a shipping acceptance. A later sampler must alternate order,
keep warmups/distributions and qualify cold counts independently.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','packages','engine','out'):
  p.add_argument('--'+name,type=Path,required=True)
 choice=p.add_mutually_exclusive_group(required=True)
 choice.add_argument('--build',type=Path)
 choice.add_argument('--overlay',type=Path)
 p.add_argument('--counter',action='store_true')
 p.add_argument('--fixture',choices=('warm','shape'),default='warm')
 a=p.parse_args();root,packages,engine,out=[getattr(a,n).resolve() for n in ('root','packages','engine','out')]
 sys.path.insert(0,str(root/'tools'))
 from foreign_validation_build import load_build
 from compiler_response import command_with_response
 inputs=load_build(a.build,a.overlay);cohort=inputs['cohort'];base=inputs['base'];cc=shutil.which('gcc');assert cc
 fixture=Path(__file__).resolve().parent
 assert not out.exists();out.mkdir(parents=True);(out/'tmp').mkdir()
 sdk=packages/'_xge_native/include';private=packages/'_xge_native/src/runtime.c'
 windows=os.name=='nt';ext='.exe' if windows else '.out'
 env=dict(os.environ,XLANG_PACKAGE_PATH=str(packages),XL6_SOURCE_NATIVE_ROOT=str(cohort),
  TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
 runtime=inputs['runtime'];native=engine/('xge.dll' if windows else 'libxge.so')
 native_link=engine/('xge.lib' if windows else 'libxge.so')
 assert native.read_bytes()==(packages/'_xge_native/native'/native.name).read_bytes()
 assert (engine/'xge_build_config.h').read_bytes()==(sdk/'xge_build_config.h').read_bytes()
 if windows:
  shutil.copy2(runtime,out/runtime.name);shutil.copy2(native,out/native.name)
 else:env['LD_LIBRARY_PATH']=str(engine)+':'+str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
 runtime_sdk=Path(base.get('RuntimeDirectory') or base['SdkDirectory'])
 # The checked native ABI uses source-matched declarations, including optional
 # cold extensions in an explicit successor. Do not alter the parent's SDK.
 alias_base=inputs['basePath'].parent/'include'
 alias_inputs=[]
 for path in alias_base.rglob('*'):
  if path.is_file() and path.suffix in ('.h','.inc','.def'):
   target=out/'include'/path.relative_to(alias_base);target.parent.mkdir(parents=True,exist_ok=True)
   shutil.copy2(path,target);alias_inputs.append(path)
 for folder in (cohort/'src/runtime',cohort/'src/api'):
  for path in folder.glob('*.h'):
   target=out/'include/__xl_runtime'/path.name
   if target.is_file():shutil.copy2(path,target);alias_inputs.append(path)
 include=['-I'+str(x) for x in (out,fixture,sdk,out/'include',cohort,runtime_sdk,runtime_sdk/'include')]
 flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*include]
 flags+=['-Wno-'+x for x in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','missing-field-initializers','cast-function-type')]
 if windows:flags+=['-DXGE_DLL','-DXUI_DLL']
 else:flags+=['-D_GNU_SOURCE']
 if a.counter:flags+=['-DBINDING_PERF_COUNT']
 if a.fixture=='shape':flags+=['-DBINDING_PERF_SEQUENCE']
 systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(engine)]
 sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
 workload=fixture/('shape_workload.xl' if a.fixture=='shape' else 'workload.xl')
 handwritten=fixture/('handwritten_shape.c' if a.fixture=='shape' else 'handwritten.c')
 pinned={str(f):sha(f) for f in (Path(__file__),fixture/'planner.c',handwritten,fixture/'binding_perf_native.c',fixture/'binding_perf.h',fixture/'language_host.c',workload,private,native,native_link)}
 pinned.update(inputs['inputs']);pinned.update((str(f),sha(f)) for f in alias_inputs)
 pinned.update((str(f),sha(f)) for f in packages.rglob('*') if f.is_file())
 report=dict(complete=False,passed=False,scope=__doc__,compilerBuild=inputs['buildManifest'],compilerOverlay=inputs['overlay'],
  fullCompilerRebuild=inputs['fullSourceRebuild'],
  optimization='O2',fixture=a.fixture,counterBinary=a.counter,shippingDelivery=False,performanceAccepted=False,coldAccepted=False,inputs=pinned,rows=[],objects=[])
 def save():(out/'preparation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 def run(name,cmd,timeout=600):
  cmd=command_with_response(list(map(str,cmd)),cc,out/(name+'.rsp'))
  start=time.monotonic();r=subprocess.run(cmd,cwd=cohort,env=env,capture_output=True,timeout=timeout)
  log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
  report['rows'].append(dict(name=name,command=cmd,exitCode=r.returncode,logSha256=sha(log),seconds=time.monotonic()-start));save()
  print(name,r.returncode,flush=True);assert r.returncode==0,log
  return r.stdout
 planner=out/('planner'+ext)
 # Complete compiler objects are used only by this offline observer.
 run('planner-build',[cc,*flags,fixture/'planner.c',*inputs['objects'],runtime,*systems,'-o',planner])
 native_source=out/'selected-native.c'
 plan=json.loads(run('plan',[planner,workload,native_source]))
 report['plan']=plan
 assert '#define XL6_ARTIFACT_XRT_IMPLEMENTATION 1' in native_source.read_text()
 # The production trimmed TU may include a small resident implementation
 # after XRT (the host address catalog). Materialize its exact declared source
 # path and local headers from this cohort, rather than falling back to an
 # older parent's compiler-source SDK alias.
 pending=[cohort/name for name in re.findall(r'#include <__xl_compiler/([^>]+)>',native_source.read_text())]
 copied=set()
 while pending:
  path=pending.pop().resolve();relative=path.relative_to(cohort.resolve())
  if path in copied:continue
  copied.add(path);target=out/'include/__xl_compiler'/relative
  target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target);pinned[str(path)]=sha(path)
  for name in re.findall(r'^\s*#\s*include\s*"([^"\n]+)"',path.read_text(),re.M):
   candidate=path.parent/name
   if candidate.is_file():pending.append(candidate)
 if a.counter:
  native_source.write_text('#define XRT_MODULE_MEMORY_DEBUG\n'+native_source.read_text(),encoding='utf-8')
  report['counterOnlyNativeModules']=['XRT_MODULE_MEMORY_DEBUG']
 if a.fixture=='shape':
  schemas={row['elementCName']:row for row in plan['schemas']}
  assert {'xui_text_cluster_t','xui_text_caret_t'}<=schemas.keys()
  text='#include <__xl_runtime/xl6_native_sequence.h>\n'
  for name,cname in (('cluster','xui_text_cluster_t'),('caret','xui_text_caret_t')):
   row=schemas[cname]
   text+=f'static const XL6_NativeSequenceSchemaV1 perf_{name}_schema={{sizeof(XL6_NativeSequenceSchemaV1),XL6_NATIVE_SEQUENCE_SCHEMA_V1_TAG,UINT64_C({row["sequenceIdentity"]}),UINT64_C({row["elementIdentity"]}),sizeof({cname}),_Alignof({cname}),sizeof({cname})}};\n'
  (out/'shape_schemas.h').write_text(text,encoding='utf-8')
 generated=out/'language.c';run('cgen',[inputs['driver'],workload,'cgen-default',generated])
 # Compile each actual selected resident TU and the production-generated XRT
 # TU at O2. No compiler API, TCC, full public-driver object list or xl.dll is
 # linked into any of the three measured/correctness programs.
 selected=[native_source,*[cohort/n for n in plan['sources']],private,fixture/'binding_perf_native.c']
 assert len(selected)==len(set(selected))
 objects=[]
 for i,source in enumerate(selected):
  obj=out/('resident-'+str(i)+'.o');run('resident-'+str(i),[cc,*flags,'-c',source,'-o',obj]);objects.append(obj)
  report['objects'].append(dict(source=str(source),sourceSha256=sha(source),path=str(obj),sha256=sha(obj),optimization='O2'))
 programs={}
 for mode in ('raw','safe','language'):
  exe=out/(mode+ext)
  sources=[generated,fixture/'language_host.c'] if mode=='language' else [handwritten]
  define=['-DBINDING_PERF_SAFE'] if mode=='safe' else []
  run(mode+'-build',[cc,*flags,*define,*sources,*objects,native_link,*systems,'-o',exe])
  run(mode+'-correctness',[exe],120);programs[mode]=dict(path=str(exe),sha256=sha(exe))
 report['programs']=programs
 report['inputsUnchanged']=all(sha(f)==h for f,h in pinned.items());assert report['inputsUnchanged']
 report.update(complete=True,passed=True);save()

if __name__=='__main__':main()
