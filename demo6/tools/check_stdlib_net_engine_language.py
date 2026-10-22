"""Offline managed Engine full-source JIT/module/pruned AOT and IR contracts.
Only own worker loops and timers. No sockets, DNS or security tests.
Does not certify fresh shipping DLL/SO/VFS or all network/TLS libraries.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
 p.add_argument('--source-build',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
 p.add_argument('--native-root',type=Path)
 a=p.parse_args();root=a.root.resolve();out=a.out.resolve();manifest=a.source_build.resolve()
 def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
 build=json.loads(manifest.read_text(encoding='utf-8-sig'))
 assert not out.exists() and build.get('Complete',True) and build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
 assert all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers'])
 driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
 cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();sdk=driver.parent/'include'
 cases=('stdlib_net_engine_options','stdlib_net_engine','stdlib_net_engine_factory','stdlib_net_engine_errors','stdlib_net_engine_timers','stdlib_net_engine_cycle')
 negatives=('bad_stdlib_net_engine_callback','bad_stdlib_net_engine_backend','bad_stdlib_net_engine_raw','bad_stdlib_net_engine_snapshot_create','bad_stdlib_net_engine_snapshot_default')
 contract=root/'test/cases/xlir_net_engine_contract.c';unobserved=root/'test/cases/stdlib_net_engine_unobserved.xl'
 inputs={str(path):sha(path) for path in (Path(__file__).resolve(),manifest,driver,cc,contract,unobserved,
  *[root/('test/cases/'+n+'.xl') for n in cases+negatives])}
 out.mkdir(parents=True);rows=[];products=[];failures=[];env=dict(os.environ)
 if os.name!='nt':assert a.native_root;env['XL6_SOURCE_NATIVE_ROOT']=str(a.native_root.resolve())
 report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,ProductsUnchanged=False,SourceBuild=str(manifest),Inputs=inputs,
  Rows=rows,Products=products,Failures=failures,SourceProductAcceptance=True,ShippingLibraryAcceptance=False,
  NetworkTraffic=False,NetworkSecurityTests=False,Scope=__doc__)
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
 def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
 def run(name,args,reject=False):
  log=out/(name+'.log');started=time.monotonic()
  with log.open('wb') as stream:r=subprocess.run(list(map(str,args)),cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
  rows.append(dict(Name=name,Command=list(map(str,args)),Exit=r.returncode,ExpectedRejection=reject,
   Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started));save()
  print(name+': '+str(r.returncode),flush=True)
  assert r.returncode==(1 if reject else 0),(name,str(log))
  return log.read_text(encoding='utf8',errors='replace')
 flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(sdk)]
 if os.name!='nt':flags+=['-D_GNU_SOURCE']
 compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-unused-label','-Wno-missing-field-initializers']
 save()
 try:
  for case in cases:
   source=root/('test/cases/'+case+'.xl');generated=out/(case+'.c');ir=out/(case+'.json')
   run(case+'.emit',[driver,source,'cgen-default',generated]);product(generated)
   run(case+'.ir',[driver,source,'ir-lowered',ir]);product(ir)
   data=json.loads(ir.read_text());ownsSource=case!='stdlib_net_engine_options'
   assert data['moduleExecutionOwnershipAbi']==int(ownsSource) and data['moduleContinuationAbi']==0 and data['moduleTaskPoolAbi']=='none'
   text=generated.read_text();assert ('static XL6_ExecutionSourceV1* xlir_execution_source;' in text)==ownsSource
   assert 'xl6NetworkService' not in text and 'xlir_async_pool' not in text and 'xlir_continuation_executor' not in text
   for opt in ('O0','O2'):
    obj=out/(case+'.'+opt+'.o');run(case+'.'+opt,[cc,*flags,*compat,'-'+opt,'-c',generated,'-o',obj]);product(obj)
   for mode in ('run','module'):run(case+'.'+mode,[driver,source,mode,out/(case+'.'+mode)])
   prefix=out/(case+'.aot');exe=Path(str(prefix)+('.exe' if os.name=='nt' else '.out'));arg=prefix if os.name=='nt' else exe
   run(case+'.aot.build',[driver,source,'exe',arg]);product(exe);run(case+'.aot.run',[exe])
   native=Path(str(arg)+'.temps')/'xl6_xrt_impl.c';assert native.is_file();nativeText=native.read_text();product(native)
   assert '_MODULE_ALL' not in nativeText
   for module in ('EXECUTOR','TASK_POOL','NET_RESOLVER','NET_DNS','TLS','NET_TCP','COROUTINE_SCHEDULER'):
    assert '#define XRT_MODULE_'+module+'\n' not in nativeText,module
   if os.name=='nt':imports=run(case+'.imports',['E:/software/w64devkit/bin/objdump.exe','-p',exe])
   else:imports=run(case+'.imports',['readelf','-d',exe])
   assert not any(name in imports.lower() for name in ('xl.dll','xlc.dll','libxl.so','libxlc.so'))
  for case in negatives:
   diagnostic=run(case+'.refused',[driver,root/('test/cases/'+case+'.xl'),'cgen-default',out/(case+'.c')],True)
   reasons={'bad_stdlib_net_engine_callback':"no matching callable class member 'NetEngine.post'",
    'bad_stdlib_net_engine_backend':"no matching callable class member 'NetEngineOptions.withBackend'",
    'bad_stdlib_net_engine_raw':'没有匹配的 create 重载',
    'bad_stdlib_net_engine_snapshot_create':'default class constructor requires a verified zero-argument layout ABI',
    'bad_stdlib_net_engine_snapshot_default':'value declaration has no verified initializer ABI'}
   assert reasons[case] in diagnostic and not (out/(case+'.c')).exists(),(case,diagnostic)
  for mode in ('run','module'):
   for n in range(4):
    diagnostic=run('unobserved.'+mode+str(n),[driver,unobserved,mode,out/('unobserved.'+mode+str(n))],True)
    assert 'retirement' in diagnostic.lower() or 'unobserved engine' in diagnostic
  objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
  assert len(objects)==len(build['Objects'])-1
  for opt in ('O0','O2'):
   obj=out/('contract.'+opt+'.o');run('contract.'+opt+'.build',[cc,*flags,'-'+opt,'-c',contract,'-o',obj]);product(obj)
   binary=out/('contract.'+opt+('.exe' if os.name=='nt' else ''))
   if os.name=='nt':link=[*objects,obj,root/'release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32','-o',binary]
   else:
    lib=a.native_root.resolve()/'release';link=[*objects,obj,'-L'+str(lib),'-lxl','-Wl,-rpath,'+str(lib),'-Wl,--export-dynamic','-pthread','-ldl','-lm','-o',binary]
   response=out/('contract.'+opt+'.rsp');response.write_text('\n'.join('"'+str(arg).replace('\\','/')+'"' for arg in link)+'\n',encoding='utf8');product(response)
   run('contract.'+opt+'.link',[cc,'@'+str(response)]);product(binary)
   if os.name=='nt' and not (out/'xl.dll').exists():shutil.copy2(root/'release/xl.dll',out/'xl.dll');product(out/'xl.dll')
   proof=run('contract.'+opt+'.run',[binary,root/'test/cases/stdlib_net_engine.xl',root/'test/cases/stdlib_net_engine_options.xl'])
   assert 'Engine XLIR:' in proof and 'direct targets' in proof
  report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items()) and all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers'])
  report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
  report['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in products)
  assert report['InputsUnchanged'] and report['LogsUnchanged'] and report['ProductsUnchanged'];report['Complete']=True;save()
 except BaseException as error:failures.append(repr(error));save();raise
if __name__=='__main__':main()
