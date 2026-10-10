"""Native Set language factories, static methods, checked import and persistence.

Fresh complete source compiler; exact corrupt-IR refusal; deterministic strict
C; real independently selected O0/O2 runtime closures; whole-program logical
allocation prefixes and JIT. Leaf Set excludes List and Array excludes Set.
Selected Linux generated/runtime closures may be ASan/UBSan instrumented; the
whole compiler, shipping DLLs and full53 are NOT certified by this gate.
"""
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
from pathlib import Path
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','public-directory','out'): p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--sanitize',action='store_true'); a=p.parse_args()
 root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve()
 assert not out.exists() and (not a.sanitize or os.name!='nt')
 manifest=public/'build.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig'))
 assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
 if os.name!='nt': assert build['Complete'] and build['InputsUnchanged']
 cc=shutil.which('gcc'); assert cc; driver=Path(build['Driver']); assert sha(driver).lower()==build['DriverSha256'].lower()
 product=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
 cases=root/'test/cases'; fixtures=[cases/(s+'.xl') for s in ('stdlib_set_source_native','stdlib_set_source_leaf','stdlib_list_methods_array_prune')]
 contract=cases/'xlir_set_source_contract.c'; host=cases/'xlir_module_product_oom_host.c'; memory=root/'test/support/module_product_memory.c'
 files={Path(__file__).resolve(),Path(sys.executable),Path(cc),manifest,driver,product,contract,host,memory,
  cases/'xlir_artifact_runtime_contract.c',*fixtures}
 for folder in ('src','lib','release/include','test/support'): files.update((root/folder).rglob('*.h'))
 for r in build['Inputs']+build['Headers']+build['Objects']:
  assert sha(r['Path']).lower()==r['Sha256'].lower(); files.add(Path(r['Path']))
 inputs=[dict(Path=str(s),Sha256=sha(s)) for s in sorted(files)]
 out.mkdir(parents=True); rows=[]; prefixes=[]; plans=[]; products=[]; objects=[]
 report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,
  PrunedExecutions=plans,Products=products,Objects=objects,LanguageSetFactoryCutover=True,SetNativeRestore=True,
  ShippingAcceptance=False,Full53Acceptance=False,PrunedGeneratedRuntimeSanitized=a.sanitize,WholeCompilerSanitized=False)
 env=dict(os.environ); suffix='.exe' if os.name=='nt' else ''
 if os.name=='nt': shutil.copy2(product,out/'xl.dll')
 else: env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
 if a.sanitize: env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),
  '-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc'),'-I'+str(public/'include'),'-Wno-unused-function','-Wno-unused-variable',
  '-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
  '-Wno-missing-field-initializers','-Wno-pointer-sign']+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
 systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
 links=[r['Path'] for r in build['Objects'] if Path(r['Path']).name not in ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
 links+=[product]+([] if os.name=='nt' else ['-Wl,-rpath,'+str(product.parent)])+systems
 san=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if a.sanitize else []
 def run(name,command):
  command=list(map(str,command)); start=time.monotonic(); result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=600)
  data=result.stdout+result.stderr; log=out/(name+'.log'); log.write_bytes(data)
  rows.append(dict(Name=name,Command=command,Exit=result.returncode,Log=str(log),LogSha256=sha(log),Seconds=time.monotonic()-start))
  print(name+': '+str(result.returncode),flush=True); assert not result.returncode,(name,data[-8000:].decode(errors='replace'))
  return data.decode(errors='replace')
 try:
  binary=out/('contract'+suffix); run('contract.build',[cc,*flags,'-O0',contract,*links,'-o',binary])
  log=run('contract.run',[binary,*[f.as_posix() for f in fixtures]])
  found=re.search(r'creates=(\d+) imports=(\d+) mutations=(\d+)',log); assert found
  report['Factories'],report['Imports'],report['CorruptContractsRefused']=map(int,found.groups())
  selected=[json.loads(s) for s in log.splitlines() if s.startswith('{"Fixture":')]
  assert len(selected)==3
  for index,(fixture,plan) in enumerate(zip(fixtures,selected)):
   tag=('source','leaf','array')[index]; generated=out/(tag+'.c'); repeat=out/(tag+'.repeat.c')
   run(tag+'.cgen',[driver,fixture,'cgen-default',generated]); run(tag+'.repeat',[driver,fixture,'cgen-default',repeat]); assert sha(generated)==sha(repeat)
   text=generated.read_text(encoding='utf-8-sig')
   if index!=2:
    assert 'xl6SetCreateTakeV1(' in text and 'xl6SetValueAddTakeV1(' in text
    assert not re.search(r'\bxrtValueSet(?:AddNew|Has|Remove|Union|Intersection|Difference|SymmetricDifference|IsSubset|IsSuperset)?\(',text)
   else: assert 'xl6SetCreateTakeV1(' not in text
   for opt in ('O0','O2'):
    lane=tag+'.'+opt; impl=out/(lane+'.runtime.c')
    impl.write_text(''.join('#define '+m+'\n' for m in plan['NativeModules'])+
     '#define XRT_MODULE_MEMORY_DEBUG\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
    providers=[]
    for i,source in enumerate([impl,*[root/s for s in plan['Sources']],memory]):
     obj=out/(lane+'.'+str(i)+'.o'); run(lane+'.provider.'+str(i),[cc,*flags,*san,'-'+opt,'-c',source,'-o',obj]); providers.append(obj)
     objects.append(dict(Path=str(obj),Sha256=sha(obj)))
    binary=out/(lane+suffix); entry=['-DXL6_PRODUCT_USER_ENTRY'] if index!=2 else []
    run(lane+'.build',[cc,*flags,*san,'-'+opt,*entry,'-include',generated,host,*providers,*systems,'-o',binary])
    log=run(lane+'.prefixes',[binary,'--all']); match=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',log); assert match
    runs,first=map(int,match.groups()); assert runs==first+1
    prefixes.append(dict(Fixture=tag,Optimization=opt,Runs=runs,FirstNoHit=first))
    plans.append(dict(Fixture=tag,Optimization=opt,Sources=plan['Sources'],NativeModules=plan['NativeModules']))
    products.append(dict(Path=str(binary),Sha256=sha(binary)))
   run(tag+'.jit',[driver,fixture,'run',out/(tag+'.jit.result')])
  report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs); assert report['InputsUnchanged']
  report['Complete']=len(prefixes)==6
 except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
 finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
