"""Offline public-source TaskPool ABI/JIT/module/standalone acceptance.
Does not certify real TaskPool worker-exit, fresh shipping DLL/SO/VFS or the
whole concurrency/five-group goal. Uses genuine TCC core, no network tests.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
 p.add_argument('--source-build',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--native-root',type=Path)
 a=p.parse_args();root=a.root.resolve();out=a.out.resolve();manifest=a.source_build.resolve();build=json.loads(manifest.read_text(encoding='utf-8-sig'))
 assert not out.exists(),'Choose fresh evidence directory'
 def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
 assert build.get('SourceXrt') and not build.get('RuntimeOnly') and build['EnableExeCompiler']==1
 assert all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers'])
 assert sha(build['Driver'])==build['DriverSha256'].lower()
 cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();driver=Path(build['Driver']);sdk=driver.parent/'include';ext='.exe' if os.name=='nt' else ''
 cases=('stdlib_task_pool','stdlib_task_pool_results','stdlib_task_pool_control','stdlib_task_pool_errors',
  'stdlib_task_pool_self_release','stdlib_task_pool_unused','stdlib_task_pool_borrowed','stdlib_future_c_scalars')
 negatives=('bad_stdlib_task_pool_missing_import','bad_stdlib_task_pool_callback','bad_stdlib_task_pool_multi_result',
  'bad_stdlib_task_pool_generator_result','bad_stdlib_task_pool_explicit_null_result')
 regression=('stdlib_promise_values','stdlib_promise_terminal','stdlib_task_group_start','stdlib_executor','stdlib_executor_errors')
 files={Path(__file__).resolve(),manifest,cc,driver,root/'test/cases/xlir_task_pool_language_contract.c',
  *[root/('test/cases/'+n+'.xl') for n in cases+negatives+regression]}
 inputs={str(path):sha(path) for path in sorted(files)};out.mkdir(parents=True);rows=[];products=[];failures=[];env=dict(os.environ)
 if os.name!='nt':assert a.native_root;env['XL6_SOURCE_NATIVE_ROOT']=str(a.native_root.resolve())
 report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,SourceBuild=str(manifest),SourceBuildSha256=sha(manifest),Inputs=inputs,
  Rows=rows,Products=products,Failures=failures,SourceProductAcceptance=True,ShippingLibraryAcceptance=False,WorkerExitAccepted=False,
  NetworkTests=False,NetworkSecurityTests=False,Scope=__doc__)
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
 def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
 def run(name,args,proof=None,reject=False):
  log=out/(name+'.log');started=time.monotonic()
  with log.open('wb') as stream:r=subprocess.run(list(map(str,args)),cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
  rows.append(dict(Name=name,Command=list(map(str,args)),Exit=r.returncode,ExpectedRejection=reject,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started));save()
  print(name+': '+str(r.returncode),flush=True);assert (r.returncode==1 if reject else r.returncode==0),(name,str(log))
  text=log.read_text(encoding='utf8',errors='replace')
  if proof:assert proof in text,(name,'missing proof')
  return text
 flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(sdk)]
 if os.name!='nt':flags+=['-D_GNU_SOURCE']
 compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-unused-label','-Wno-missing-field-initializers']
 save()
 try:
  for case in cases:
   source=root/('test/cases/'+case+'.xl');generated=out/(case+'.c');ir=out/(case+'.json')
   run(case+'.emit',[driver,source,'cgen-default',generated]);product(generated)
   run(case+'.ir',[driver,source,'ir-lowered',ir]);product(ir)
   data=json.loads(ir.read_text());owns=not case.endswith(('_unused','_borrowed'))
   assert data['moduleExecutionOwnershipAbi']==int(owns)
   assert data['moduleContinuationAbi']==(2 if case=='stdlib_future_c_scalars' else 0) and data['moduleTaskPoolAbi']=='none'
   text=generated.read_text();assert ('static XL6_ExecutionSourceV1* xlir_execution_source;' in text)==owns
   if not case.endswith('_unused'):
    assert '(XL6_CheckedResultClosureBorrowV1)' in text and 'xlir_checked_result_t' in text
    assert 'xl6TaskPoolSubmitLanguageV1((XL6_FutureValueProfileV1)' in text
   assert 'xlir_async_pool' not in text and 'xl6TaskSubmitClosureV1(' not in text
   for opt in ('O0','O2'):
    obj=out/(case+'.'+opt+'.o');run(case+'.'+opt,[cc,*flags,*compat,'-'+opt,'-c',generated,'-o',obj]);product(obj)
   for mode in ('run','module'):run(case+'.'+mode,[driver,source,mode,out/(case+'.'+mode)])
   prefix=out/(case+'.aot');exe=Path(str(prefix)+('.exe' if os.name=='nt' else '.out'));arg=prefix if os.name=='nt' else exe
   run(case+'.aot.build',[driver,source,'exe',arg]);product(exe);run(case+'.aot.run',[exe])
   native=Path(str(arg)+'.temps')/'xl6_xrt_impl.c';assert native.is_file();nativeText=native.read_text();product(native)
   assert '_MODULE_ALL' not in nativeText
   assert ('#define XRT_MODULE_TASK_POOL\n' in nativeText)==(not case.endswith('_unused'))
   assert '#define XRT_MODULE_EXECUTOR\n' not in nativeText
  for case in negatives:
   run(case+'.refused',[driver,root/('test/cases/'+case+'.xl'),'cgen-default',out/(case+'.c')],proof='public',reject=True)
   assert not (out/(case+'.c')).exists()
  for case in regression:
   source=root/('test/cases/'+case+'.xl')
   for mode in ('run','module'):run(case+'.'+mode,[driver,source,mode,out/(case+'.'+mode)])
   prefix=out/(case+'.aot');exe=Path(str(prefix)+('.exe' if os.name=='nt' else '.out'))
   run(case+'.aot.build',[driver,source,'exe',prefix if os.name=='nt' else exe]);product(exe);run(case+'.aot.run',[exe])
  obj=out/'contract.o';run('contract.build',[cc,*flags,'-O0','-c',root/'test/cases/xlir_task_pool_language_contract.c','-o',obj]);product(obj)
  objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
  assert len(objects)==len(build['Objects'])-1;binary=out/('contract'+ext)
  if os.name=='nt':link=[cc,*objects,obj,root/'release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32','-o',binary]
  else:
   lib=a.native_root.resolve()/'release';link=[cc,*objects,obj,'-L'+str(lib),'-lxl','-Wl,-rpath,'+str(lib),'-Wl,--export-dynamic','-pthread','-ldl','-lm','-o',binary]
  response=out/'contract.rsp';response.write_text('\n'.join('"'+str(arg).replace('\\','/')+'"' for arg in link[1:])+'\n',encoding='utf8');product(response)
  run('contract.link',[cc,'@'+str(response)]);product(binary)
  if os.name=='nt':shutil.copy2(root/'release/xl.dll',out/'xl.dll');product(out/'xl.dll')
  run('contract.run',[binary,*[root/('test/cases/'+n+'.xl') for n in ('stdlib_task_pool','stdlib_task_pool_unused','stdlib_task_pool_borrowed')]],proof='TaskPool XLIR:')
  report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items()) and all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs'])
  report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
  assert report['InputsUnchanged'] and report['LogsUnchanged'];report['Complete']=True;save()
 except BaseException as error:failures.append(repr(error));save();raise
if __name__=='__main__':main()
