"""Offline numeric address public-source ABI/JIT/module/pruned AOT acceptance.
Numeric input only, including the separate DNS provider: no external lookup,
socket connections, Engine or network security tests. Not shipping DLL/SO/VFS
acceptance and not acceptance of all forty network modules.
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
 assert not out.exists() and build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
 assert all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers'])
 driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
 cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();sdk=driver.parent/'include'
 cases=('stdlib_net_address_values','stdlib_net_address_resolve_numeric')
 negatives=('bad_stdlib_net_address_handle','bad_stdlib_net_address_adopt')
 contract=root/'test/cases/xlir_net_address_contract.c'
 inputs={str(path):sha(path) for path in (Path(__file__).resolve(),manifest,driver,cc,contract,
  *[root/('test/cases/'+n+'.xl') for n in cases+negatives])}
 out.mkdir(parents=True);rows=[];products=[];failures=[];env=dict(os.environ)
 if os.name!='nt':assert a.native_root;env['XL6_SOURCE_NATIVE_ROOT']=str(a.native_root.resolve())
 report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,SourceBuild=str(manifest),Inputs=inputs,
  Rows=rows,Products=products,Failures=failures,SourceProductAcceptance=True,ShippingLibraryAcceptance=False,
  ExternalNetworkTests=False,NetworkSecurityTests=False,Scope=__doc__)
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
 def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
 def run(name,args,reject=False):
  log=out/(name+'.log');started=time.monotonic()
  with log.open('wb') as stream:r=subprocess.run(list(map(str,args)),cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
  rows.append(dict(Name=name,Command=list(map(str,args)),Exit=r.returncode,ExpectedRejection=reject,
   Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started));save()
  print(name+': '+str(r.returncode),flush=True)
  assert (r.returncode==1 if reject else r.returncode==0),(name,str(log))
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
   text=generated.read_text();assert 'xl6NetAddressParseV1(' in text and 'xl6_xrt_net_address_' not in text
   assert 'xl6NetAddressParseV1(xstrview' not in text and 'xl6NetworkService' not in text
   for opt in ('O0','O2'):
    obj=out/(case+'.'+opt+'.o');run(case+'.'+opt,[cc,*flags,*compat,'-'+opt,'-c',generated,'-o',obj]);product(obj)
   for mode in ('run','module'):run(case+'.'+mode,[driver,source,mode,out/(case+'.'+mode)])
   prefix=out/(case+'.aot');exe=Path(str(prefix)+('.exe' if os.name=='nt' else '.out'));arg=prefix if os.name=='nt' else exe
   run(case+'.aot.build',[driver,source,'exe',arg]);product(exe);run(case+'.aot.run',[exe])
   native=Path(str(arg)+'.temps')/'xl6_xrt_impl.c';assert native.is_file();nativeText=native.read_text();product(native)
   assert '#define XRT_MODULE_NET\n' in nativeText and '_MODULE_ALL' not in nativeText
   for module in ('NET_ENGINE','NET_RESOLVER','NET_INTERFACE','TLS','EXECUTOR','COROUTINE_SCHEDULER'):
    assert '#define XRT_MODULE_'+module+'\n' not in nativeText,module
   assert ('#define XRT_MODULE_NET_DNS\n' in nativeText)==case.endswith('_numeric')
  for case in negatives:
   run(case+'.refused',[driver,root/('test/cases/'+case+'.xl'),'cgen-default',out/(case+'.c')],True)
   assert not (out/(case+'.c')).exists()
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
   proof=run('contract.'+opt+'.run',[binary,*[root/('test/cases/'+n+'.xl') for n in cases]])
   assert 'address XLIR:' in proof and 'balanced storage' in proof
  report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items()) and all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs'])
  report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
  assert report['InputsUnchanged'] and report['LogsUnchanged']
  report['Complete']=True;save()
 except BaseException as error:failures.append(repr(error));save();raise
if __name__=='__main__':main()
