"""Offline source-product NetEngine owned tasks and TaskGroup composition.
No application sockets, network data, security campaign or shipping-library claim.
Runs all independent cases to discover failures in batches rather than stopping
at the first bad language case; a failed stage never counts as acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('source-build','out'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--native-root',type=Path)
 a=p.parse_args();root=a.root.resolve();out=a.out.resolve();manifest=a.source_build.resolve()
 assert not out.exists();build=json.loads(manifest.read_text(encoding='utf-8-sig'))
 def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
 assert build.get('SourceXrt') and not build.get('RuntimeOnly') and build['EnableExeCompiler']==1
 assert all(sha(r['Path'])==r['Sha256'].lower() for r in build['Inputs']+build['Objects']+build['Headers'])
 assert sha(build['Driver'])==build['DriverSha256'].lower()
 cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();driver=Path(build['Driver']);sdk=driver.parent/'include'
 cases=('stdlib_net_task','stdlib_net_task_results','stdlib_net_task_errors','stdlib_net_task_pending',
  'stdlib_net_task_unobserved','stdlib_task_group_net_composition')
 negatives=tuple('bad_stdlib_net_task_'+n for n in ('callback','multi_result','generator_result','explicit_null','affinity','token'))+('bad_generic_independent_parameter',)
 regressions=('generic_independent_parameters','stdlib_net_engine','stdlib_task_pool_results','stdlib_task_pool_errors','stdlib_task_group_pool_composition')
 contract=root/'test/cases/xlir_net_task_language_contract.c'
 files={Path(__file__).resolve(),manifest,cc,driver,contract,*[root/('test/cases/'+n+'.xl') for n in cases+negatives+regressions]}
 inputs={str(f):sha(f) for f in sorted(files)};rows=[];products=[];failures=[];out.mkdir(parents=True);env=dict(os.environ)
 if os.name!='nt':assert a.native_root;env['XL6_SOURCE_NATIVE_ROOT']=str(a.native_root.resolve())
 report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,SourceBuild=str(manifest),SourceBuildSha256=sha(manifest),
  Inputs=inputs,Rows=rows,Products=products,Failures=failures,SourceProductAcceptance=True,ShippingLibraryAcceptance=False,
  ApplicationNetworkTraffic=False,NetworkSecurityTests=False,Scope=__doc__)
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
 def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
 def run(name,args,proof=None,reject=False):
  log=out/(name+'.log');started=time.monotonic()
  with log.open('wb') as stream:r=subprocess.run(list(map(str,args)),cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
  rows.append(dict(Name=name,Command=list(map(str,args)),Exit=r.returncode,ExpectedRejection=reject,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started));save()
  print(name+': '+str(r.returncode),flush=True)
  assert r.returncode==(1 if reject else 0),(name,str(log))
  text=log.read_text(encoding='utf8',errors='replace')
  if proof:assert proof in text,(name,'missing proof')
  return text
 flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(sdk)]
 if os.name!='nt':flags+=['-D_GNU_SOURCE']
 compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-unused-label','-Wno-missing-field-initializers']
 def executable(prefix):return Path(str(prefix)+('.exe' if os.name=='nt' else '.out'))
 def language(case,primary):
  source=root/('test/cases/'+case+'.xl');generated=out/(case+'.c')
  run(case+'.emit',[driver,source,'cgen-default',generated]);product(generated)
  text=generated.read_text()
  if primary:
   ir=out/(case+'.json');run(case+'.ir',[driver,source,'ir-lowered',ir]);product(ir);data=json.loads(ir.read_text())
   assert data['moduleExecutionOwnershipAbi']==1 and data['moduleTaskPoolAbi']=='none'
   assert 'xl6NetEngineSubmit' in text and '(XL6_CheckedResultClosureBorrowV1)' in text
   assert 'xlir_async_pool' not in text and 'xl6TaskSubmitClosureV1(' not in text
   for opt in ('O0','O2'):
    obj=out/(case+'.'+opt+'.o');run(case+'.'+opt,[cc,*flags,*compat,'-'+opt,'-c',generated,'-o',obj]);product(obj)
  for mode in ('run','module'):run(case+'.'+mode,[driver,source,mode,out/(case+'.'+mode)])
  prefix=out/(case+'.aot');exe=executable(prefix);arg=prefix if os.name=='nt' else exe
  run(case+'.aot.build',[driver,source,'exe',arg]);product(exe);run(case+'.aot.run',[exe])
  if primary:
   native=Path(str(arg)+'.temps')/'xl6_xrt_impl.c';assert native.is_file();product(native);body=native.read_text()
   assert '_MODULE_ALL' not in body and '#define XRT_MODULE_TASK_NET\n' in body
   assert '#define XRT_MODULE_TASK_POOL\n' not in body and '#define XRT_MODULE_EXECUTOR\n' not in body
 def abi():
  obj=out/'contract.o';run('contract.build',[cc,*flags,'-O0','-c',contract,'-o',obj]);product(obj)
  objects=[r['Path'] for r in build['Objects'] if Path(r['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
  assert len(objects)==len(build['Objects'])-1;binary=out/('contract.exe' if os.name=='nt' else 'contract')
  if os.name=='nt':link=[cc,*objects,obj,root/'release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32','-o',binary]
  else:
   lib=a.native_root.resolve()/'release';link=[cc,*objects,obj,'-L'+str(lib),'-lxl','-Wl,-rpath,'+str(lib),'-Wl,--export-dynamic','-pthread','-ldl','-lm','-o',binary]
  response=out/'contract.rsp';response.write_text('\n'.join('"'+str(v).replace('\\','/')+'"' for v in link[1:])+'\n',encoding='utf8');product(response)
  run('contract.link',[cc,'@'+str(response)]);product(binary)
  if os.name=='nt':shutil.copy2(root/'release/xl.dll',out/'xl.dll');product(out/'xl.dll')
  run('contract.run',[binary,root/'test/cases/stdlib_net_task.xl'],'NetTask XLIR:')
 save()
 for case in cases+regressions:
  try:language(case,case in cases)
  except BaseException as error:failures.append(dict(Case=case,Error=repr(error)));save();print('FAILED '+case,flush=True)
 for case in negatives:
  try:
   target=out/(case+'.c');run(case+'.refused',[driver,root/('test/cases/'+case+'.xl'),'cgen-default',target],'public',True);assert not target.exists()
  except BaseException as error:failures.append(dict(Case=case,Error=repr(error)));save()
 try:abi()
 except BaseException as error:failures.append(dict(Case='IR contract',Error=repr(error)));save()
 report['InputsUnchanged']=all(sha(f)==h for f,h in inputs.items()) and all(sha(r['Path'])==r['Sha256'].lower() for r in build['Inputs'])
 report['LogsUnchanged']=all(sha(r['Log'])==r['Sha256'] for r in rows)
 report['Complete']=not failures and report['InputsUnchanged'] and report['LogsUnchanged'];save()
 assert report['Complete'],failures
if __name__=='__main__':main()
