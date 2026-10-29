"""Batch ordinary foundation acceptance against actual CLI and both libraries.

No external traffic or security-specialist testing. All cases are collected
before reporting failure; strict C, heap/OOM and pruning have separate evidence.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,re,shutil,subprocess,sys,threading

CASES=[
 'stdlib_foundation_system','stdlib_foundation_memory','stdlib_foundation_core',
 'stdlib_foundation_casts','stdlib_foundation_casts_async','stdlib_foundation_restore',
 'stdlib_foundation_pools','stdlib_foundation_language_oom',
 'stdlib_foundation_system_trim','stdlib_foundation_memory_trim',
 'stdlib_graph_clone','stdlib_graph_clone_dict_leaf','stdlib_graph_clone_continuation','stdlib_graph_clone_restore',
 'stdlib_value_copy','stdlib_runtime_callable_func_defaults','stdlib_runtime_callable_func_parameters',
 'stdlib_runtime_callable_func_errors','stdlib_runtime_method_defaults','stdlib_runtime_method_defaults_errors',
 'xlir_native_dynamic_fields_values','weak_class_lifecycle','stdlib_stack_batch_public',
 'stdlib_tree_bytes_keys','stdlib_concurrent_queue_owned']
BAD=['bad_stdlib_foundation_memory_import','bad_stdlib_foundation_memory_handle',
 'bad_stdlib_foundation_stats_no_raise','bad_stdlib_foundation_pool_scalar','bad_stdlib_foundation_pool_private']
EXAMPLES=['release/examples/foundation/system.xl','release/examples/foundation/memory.xl',
 'release/examples/foundation/pools.xl']
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['root','out']:p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--jobs',type=int,default=2);p.add_argument('--case',action='append',choices=CASES)
 p.add_argument('--quick',action='store_true',help='JIT and negative cases only; never shipping acceptance')
 p.add_argument('--driver',type=Path,help='Development-only source driver; implies --quick')
 a=p.parse_args();root=a.root.resolve();out=a.out.resolve();selected=a.case or CASES
 if a.driver:a.quick=True;a.driver=a.driver.resolve()
 assert 1<=a.jobs<=4 and not out.exists();out.mkdir(parents=True);windows=os.name=='nt'
 rows=[];failures=[];lock=threading.Lock()
 report=dict(Complete=False,ShippingAcceptance=False,Quick=a.quick,ExternalTraffic=False,Rows=rows,Failures=failures)
 env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k.upper() not in ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 def run(name,command,cwd=root,negative=False):
  command=list(map(str,command));log=out/(name+'.log')
  try:r=subprocess.run(command,cwd=cwd,env=env,capture_output=True,timeout=240);code=r.returncode;data=r.stdout+r.stderr
  except subprocess.TimeoutExpired as e:code='timeout';data=(e.stdout or b'')+(e.stderr or b'')
  log.write_bytes(data)
  with lock:rows.append(dict(Name=name,Command=command,Exit=code,Negative=negative,Log=str(log),Sha256=sha(log)))
  assert code==(1 if negative else 0),(name,data[-6000:].decode(errors='replace'))
  return data
 def attempt(action):
  try:action()
  except Exception as error:failures.append(repr(error));print(str(error)[:900],flush=True)
 try:
  run('metadata',[sys.executable,root/'tools/gen_stdlib_foundation.py','--check'])
  run('snapshots',[sys.executable,root/'tools/sync_xrt_snapshot.py','--check'])
  native=['xl6_'+n+'.c' for n in ['system_info','memory_stats','memory_diagnostics','memory_report','memory_stats_domain','memory_diagnostics_domain','object_pool_control','object_pool_control_domain']]
  for n in ['compiler_dll.rsp.in','runtime_dll.rsp.in']:
   source=(root/'tools'/n).read_text(encoding='utf-8-sig').replace('\\','/')
   for item in native:assert len(re.findall(r'\bsrc/runtime/'+re.escape(item)+r'\b',source))==1,(n,item)
  names=['xl.exe','xl.dll','xlc.dll'] if windows else ['xl','libxl.so','libxlc.so']
  inputs={Path(__file__).resolve(),*([a.driver] if a.driver else [root/'release'/n for n in names]),*[root/'test/cases'/(n+'.xl') for n in selected+BAD]}
  inputs.update(root/'src/runtime'/n for n in native)
  inputs.update(root/n for n in EXAMPLES)
  inputs.update([root/'src/stdlib/object_pools.xl.in',root/'tools/gen_stdlib_foundation.py',root/'src/stdlib/xl6_stdlib_foundation.inc'])
  report['Inputs']={str(q):sha(q) for q in sorted(inputs)}
  isolated=out/'isolated';release=isolated/'release';release.mkdir(parents=True);relocated=out/'relocated';relocated.mkdir()
  if not a.driver:
   for n in names:shutil.copy2(root/'release'/n,release/n)
  cli=release/names[0];library=release/names[2];cc=shutil.which('gcc');assert cc
  flags=['-std=c11','-Wall','-Wextra','-Werror','-O2','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
  links=[library] if windows else ['-L'+str(release),'-lxlc','-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
  # Windows resolves implicit PE imports before main()/tccDllPath. Stage
  # dependent hosts beside the fresh DLLs, ahead of older PATH installations.
  binaries=release if windows else isolated
  host=binaries/('host.exe' if windows else 'host');observer=binaries/('observer.exe' if windows else 'observer')
  if not a.quick:
   for binary,n in [(host,'stdlib_product_fixture_host.c'),(observer,'stdlib_pruning_product_host.c')]:
    run(binary.stem+'.build',[cc,*flags,root/'test/cases'/n,*links,'-o',binary])
   for n in names[1:]:
    binary=binaries/(n+'.foundation-native'+('.exe' if windows else ''))
    nativeLinks=[release/n] if windows else ['-L'+str(release),'-l:'+n,'-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
    attempt(lambda n=n,binary=binary,nativeLinks=nativeLinks:(run(n+'.native.build',[cc,*flags,root/'test/cases/stdlib_foundation_native_host.c',root/'test/support/module_product_memory.c',*nativeLinks,'-o',binary]),run(n+'.native.run',[binary])))
  def positive(n):
   q=root/'test/cases'/(n+'.xl');saved=out/(n+'.saved');saved.mkdir()
   run(n+'.jit',[a.driver,q,'run',saved/'jit'] if a.driver else [cli,'run',q],root if a.driver else isolated)
   if not a.quick:
    run(n+'.module',[host,library,q,'module',saved/'module-tmp'],isolated)
    run(n+'.cgen',[cli,'cgen',q,'--emit',saved/'generated.c'],isolated)
    image=saved/('program.exe' if windows else 'program')
    run(n+'.aot-build',[observer,library,q,saved,image],isolated)
    independent=relocated/(n+('.exe' if windows else ''));shutil.copy2(image,independent)
    inspect=shutil.which('objdump' if windows else 'readelf');assert inspect
    data=run(n+'.imports',[inspect,'-p' if windows else '-d',independent],isolated)
    deps=re.findall(rb'DLL Name:\s*([^\s]+)',data) if windows else re.findall(rb'Shared library:\s*\[([^\]]+)\]',data)
    assert deps and not {d.decode().lower() for d in deps}&{name.lower() for name in names}
    if not windows:assert b'(RPATH)' not in data and b'(RUNPATH)' not in data
    run(n+'.relocated',[independent],relocated)
   print(n+' passed',flush=True)
  with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
   for result in concurrent.futures.as_completed([pool.submit(positive,n) for n in selected]):attempt(result.result)
  for relative in EXAMPLES:
   source=root/relative;name='example-'+source.stem
   attempt(lambda source=source,name=name:run(name,[a.driver,source,'run',out/name] if a.driver else [cli,'run',source],root if a.driver else isolated))
  for n in BAD:attempt(lambda n=n:run(n,[a.driver,root/'test/cases'/(n+'.xl'),'cgen-default',out/(n+'.c')] if a.driver else [cli,'cgen',root/'test/cases'/(n+'.xl'),'--emit',out/(n+'.c')],root if a.driver else isolated,True))
  if not a.quick:
   sys.path.insert(0,str(root/'tools'));from gen_tcc_vfs_resources import load_resources
   sdk=out/'sdk';sdk.mkdir()
   manifest=root/'tools'/('tcc_vfs_manifest_win64_runtime.txt' if windows else 'tcc_vfs_manifest_linux_runtime.txt')
   # Select the shipping private-header rows before resolving resources. A
   # deployed CLI/DLL set does not require a local TCC source build or xrt.o.
   headerRows=[]
   for line in manifest.read_text(encoding='utf-8-sig').splitlines():
    parts=line.strip().split('|')
    if len(parts)>=3 and (parts[1]=='include/__xl_runtime' or parts[1].startswith('include/__xl_runtime/')):headerRows.append(line)
   assert headerRows
   selectedManifest=out/'runtime-headers.manifest';selectedManifest.write_text('\n'.join(headerRows)+'\n',encoding='utf-8')
   report['Inputs'][str(manifest)]=sha(manifest)
   for resource in load_resources(selectedManifest,root,root,root,out):
    if resource.virtual.startswith('include/__xl_runtime/'):
     dest=sdk/Path(resource.virtual).relative_to('include');dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(resource.source,dest)
     report['Inputs'][str(resource.source)]=sha(resource.source)
   strict=[*flags,'-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-I'+str(sdk)]
   for n in ['stdlib_foundation_'+name for name in ['system','memory','core','casts','casts_async','restore','pools','language_oom']]:
    if n not in selected:continue
    module=out/(n+'.saved')/'module.c'
    if not module.is_file():failures.append('No saved module for strict C: '+n);continue
    for opt in ['O0','O2']:
     source=root/'test/cases'/('stdlib_codec_template_language_oom_host.c' if n.endswith(('language_oom','casts','restore')) else 'stdlib_codec_template_language_host.c')
     binary=binaries/(n+'.'+opt+('.exe' if windows else ''))
     attempt(lambda n=n,opt=opt,source=source,binary=binary,module=module:(run(n+'.'+opt+'.build',[cc,*strict,'-'+opt,'-DXLIR_GENERATED_MODULE="'+module.as_posix()+'"',source,root/'test/support/module_product_memory.c',*links,'-o',binary]),run(n+'.'+opt+'.lifetime',[binary])))
   for kind,required,forbidden in [('system',set(),{'MEMORY_STATS','MEMORY_DEBUG','MEMORY_DEBUG_REPORT','NET_ENGINE','TLS'}),('memory',{'MEMORY_STATS'},{'MEMORY_DEBUG','MEMORY_DEBUG_REPORT','NET_ENGINE','TLS','TEMPLATE'})]:
    n='stdlib_foundation_'+kind+'_trim'
    if n not in selected:continue
    def pruning(n=n,required=required,forbidden=forbidden):
     path=out/(n+'.saved')/'xl6_xrt_impl.c';assert path.is_file(),str(path)
     data=run(n+'.features',[cc,'-E','-dM','-I'+str(root/'lib/xruntime'),path])
     features={m.decode() for m in re.findall(rb'^#define XRT_FEATURE_([A-Z0-9_]+)\b',data,re.M)}
     assert required<=features and not features&forbidden,(n,features&forbidden)
    attempt(pruning)
  report['InputsUnchanged']=all(sha(q)==h for q,h in report['Inputs'].items())
  assert report['InputsUnchanged'] and not failures,failures
  report['Complete']=True;report['ShippingAcceptance']=not a.quick
 except Exception as error:report['Error']=repr(error);print(report['Error'],flush=True)
 finally:save()
 print(json.dumps({k:report[k] for k in ['Complete','ShippingAcceptance','Quick']}),flush=True)
 if not report['Complete']:raise SystemExit(1)
if __name__=='__main__':main()
