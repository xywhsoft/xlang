"""Actual generated typed and erased foreach over genuine native Set inputs.

Fresh source compiler required. Strict deterministic C, independently pruned
O0/O2 runtime closures, complete allocation and no-diagnostic provider refusal
prefixes, finally cleanup, ordinary wire Set JIT compatibility, and Array
exclusion. Native test inputs authenticate the real TypeIds from XLIR; this
does not certify language Set producers/imports/restore or full53 acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','public-directory','out'): p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args(); root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve()
 assert not out.exists(); cc=shutil.which('gcc'); assert cc
 manifest=public/'build.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig'))
 assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
 driver=Path(build['Driver']); assert sha(driver).lower()==build['DriverSha256'].lower()
 product=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
 fixture=root/'test/cases/stdlib_set_iteration_native.xl'
 array=root/'test/cases/stdlib_list_iterator_array_prune.xl'
 contract=root/'test/cases/xlir_set_iteration_contract.c'; host=root/'test/cases/stdlib_set_iteration_language_host.c'
 memory=root/'test/support/module_product_memory.c'; module_host=root/'test/cases/xlir_module_product_oom_host.c'
 files={Path(__file__).resolve(),Path(sys.executable),Path(cc),manifest,driver,product,fixture,array,contract,host,memory,
  module_host,root/'test/cases/xlir_artifact_runtime_contract.c'}
 for folder in ('src','lib','release/include','test/support'): files.update((root/folder).rglob('*.h'))
 for r in build['Inputs']+build['Headers']+build['Objects']:
  assert sha(r['Path']).lower()==r['Sha256'].lower(); files.add(Path(r['Path']))
 inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(files)]
 out.mkdir(); rows=[]; prefixes=[]; products=[]; objects=[]; selected_plans=[]; native_inputs=[]
 report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,
  Products=products,Objects=objects,PrunedExecutions=selected_plans,NativeInputExecutions=native_inputs,Scope=__doc__,
  NativeSetGeneratedIteration=True,LanguageSetFactoryCutover=False,SetRestoreCutover=False,Full53Acceptance=False)
 env=dict(os.environ); suffix='.exe' if os.name=='nt' else ''
 if os.name=='nt': shutil.copy2(product,out/'xl.dll')
 else: env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
 flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root/'release'),
  '-I'+str(root/'release/include'),'-DXL_STATIC_EMBED','-I'+str(root/'lib/tcc'),'-I'+str(public/'include'),
  '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
  '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
 if os.name!='nt': flags+=['-D_GNU_SOURCE']
 systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
 full_objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).name not in
  ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
 links=[*full_objects,product,*systems]+([] if os.name=='nt' else ['-Wl,-rpath,'+str(product.parent)])
 def run(name,cmd):
  cmd=list(map(str,cmd)); r=subprocess.run(cmd,cwd=root,env=env,capture_output=True,timeout=240)
  data=r.stdout+r.stderr; log=out/(name+'.log'); log.write_bytes(data)
  rows.append(dict(Label=name,Command=cmd,Exit=r.returncode,Log=str(log),LogSha256=sha(log)))
  assert not r.returncode,(name,data[-6000:]); return data.decode(errors='replace')
 try:
  binary=out/('contract'+suffix); bindings=out/'bindings.h'
  run('contract-build',[cc,*flags,'-O0',contract,*links,'-o',binary])
  log=run('contract-run',[binary,fixture.as_posix(),array.as_posix(),bindings]); assert 'Array excludes Set' in log
  plans=[json.loads(s) for s in log.splitlines() if s.startswith('{"Fixture":')]; assert len(plans)==2
  factory=[json.loads(s) for s in log.splitlines() if s.startswith('{"HostInputFactory":')]; assert len(factory)==1
  files.add(bindings)
  for index,(case,plan) in enumerate(zip((fixture,array),plans)):
   tag='set' if not index else 'array'; generated=out/(tag+'.c'); repeat=out/(tag+'.repeat.c')
   run(tag+'-cgen',[driver,case,'cgen-default',generated]); run(tag+'-repeat',[driver,case,'cgen-default',repeat])
   assert sha(generated)==sha(repeat)
   text=generated.read_text(encoding='utf-8-sig')
   assert ('xl6SetValueIteratorCreateCheckedV1(' in text)==(not index)
   assert ('xl6CollectionValueIteratorCreateV1(' in text)==(not index)
   for opt in ('O0','O2'):
    lane=tag+'-'+opt; impl=out/(lane+'.runtime.c')
    impl.write_text(''.join('#define '+m+'\n' for m in plan['NativeModules'])+
     '#define XRT_MODULE_MEMORY_DEBUG\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
    providers=[]
    for i,s in enumerate([impl,*[root/s for s in plan['Sources']],memory]):
     obj=out/(lane+'-'+str(i)+'.o'); run(lane+'-provider-'+str(i),[cc,*flags,'-'+opt,'-c',s,'-o',obj]); providers.append(obj)
     objects.append(dict(Path=str(obj),Sha256=sha(obj)))
    executable=out/(lane+suffix)
    run(lane+'-module-build',[cc,*flags,'-'+opt,'-include',generated,module_host,*providers,*systems,'-o',executable])
    assert 'balanced' in run(lane+'-module-run',[executable])
    products.append(dict(Path=str(executable),Sha256=sha(executable)))
    if not index:
     extra=[s for s in factory[0]['Sources'] if s not in plan['Sources']]
     modules=list(dict.fromkeys([*plan['NativeModules'],*factory[0]['NativeModules']]))
     native_impl=out/(lane+'.host-input.runtime.c')
     native_impl.write_text(''.join('#define '+m+'\n' for m in modules)+
      '#define XRT_MODULE_MEMORY_DEBUG\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
     native_providers=list(providers[1:])
     for i,s in enumerate([native_impl,*[root/s for s in extra]]):
      obj=out/(lane+'-input-'+str(i)+'.o')
      run(lane+'-input-provider-'+str(i),[cc,*flags,'-'+opt,'-c',s,'-o',obj]); native_providers.append(obj)
      objects.append(dict(Path=str(obj),Sha256=sha(obj)))
     executable=out/(lane+'-native'+suffix)
     run(lane+'-native-build',[cc,*flags,'-'+opt,'-include',generated,'-include',bindings,host,*native_providers,*systems,
      '-Wl,--wrap=xl6SetIteratorSnapshotErasedV1','-Wl,--wrap=xl6SetIteratorAdvanceValueV1','-o',executable])
     for mode in ('oom','providers'):
      data=run(lane+'-'+mode,[executable,mode]); found=re.findall(
       r'Set generated closed function=(\d+) mode='+mode+r' runs=(\d+) first-no-hit=(\d+)',data); assert len(found)==2
      for f,runs,first in found:
       f,runs,first=map(int,(f,runs,first)); assert runs==first+1
       pts=re.findall(r'Set generated function='+str(f)+r' mode='+mode+r' point=(\d+) hit=(\d) balanced',data)
       assert pts==[(str(i),str(int(i<first))) for i in range(runs)]
       prefixes.append(dict(Optimization=opt,Mode=mode,Function=f,Runs=runs,FirstNoHit=first))
     products.append(dict(Path=str(executable),Sha256=sha(executable)))
     native_inputs.append(dict(Optimization=opt,GeneratedRuntimeSources=plan['Sources'],HostInputOnlySources=extra,
      NativeModules=modules,HostInputFactoryRootsSelectedByActualSymbolOwner=True))
    selected_plans.append(dict(Fixture=tag,Optimization=opt,Sources=plan['Sources'],NativeModules=plan['NativeModules']))
   run(tag+'-jit',[driver,case,'run',out/(tag+'.jit.result')])
  report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
  report['Complete']=report['InputsUnchanged'] and len(prefixes)==8 and len(selected_plans)==4; assert report['Complete']
 finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 print('Native Set source iteration: pruned C O0/O2 and eight complete prefixes',flush=True)
if __name__=='__main__': main()
