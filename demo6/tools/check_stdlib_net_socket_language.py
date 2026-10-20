"""Full-source Socket JIT/module/standalone/IR acceptance. Only own loopback
dynamic ports; no external names/endpoints/security tests. Not shipping or
complete Socket advanced-datagram/vector/Resolver/transport acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
 p.add_argument('--source-build',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--native-root',type=Path)
 p.add_argument('--datagram',action='store_true',help='Include independent datagram values, integration and closed ABI contracts')
 a=p.parse_args();root=a.root.resolve();out=a.out.resolve();manifest=a.source_build.resolve()
 def sha(f):return hashlib.sha256(Path(f).read_bytes()).hexdigest()
 build=json.loads(manifest.read_text(encoding='utf-8-sig'));assert not out.exists() and build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
 assert all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers'])
 driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower();cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();sdk=driver.parent/'include'
 cases=('stdlib_net_socket','stdlib_net_socket_tcp','stdlib_net_socket_udp','primitive_product_i64_u64','stdlib_atomics','cgen_native_product_i64x3')
 negatives=('bad_stdlib_net_socket_kind','bad_stdlib_net_socket_option','bad_stdlib_net_socket_raw','bad_native_product_unsigned_head','bad_native_product_narrow_tail')
 if a.datagram:
  cases=('stdlib_net_datagram_values','stdlib_net_datagram_io',*cases)
  negatives=(*negatives,'bad_stdlib_net_datagram_flags','bad_stdlib_net_datagram_info')
 contracts=[root/'test/cases/xlir_net_socket_contract.c']
 if a.datagram:contracts.append(root/'test/cases/xlir_net_datagram_contract.c')
 inputs={str(f):sha(f) for f in (Path(__file__).resolve(),manifest,driver,cc,*contracts,*[root/('test/cases/'+n+'.xl') for n in cases+negatives])}
 out.mkdir(parents=True);rows=[];products=[];failures=[];env=dict(os.environ)
 if os.name!='nt':assert a.native_root;env['XL6_SOURCE_NATIVE_ROOT']=str(a.native_root.resolve())
 report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Failures=failures,
  SourceBuild=str(manifest),SourceProductAcceptance=True,ShippingLibraryAcceptance=False,OwnLoopbackOnly=True,ExternalNetworkTests=False,NetworkSecurityTests=False,Scope=__doc__)
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
 def product(f):products.append(dict(Path=str(f),Sha256=sha(f)));save()
 def run(name,args,reject=False):
  log=out/(name+'.log');started=time.monotonic()
  with log.open('wb') as stream:r=subprocess.run(list(map(str,args)),cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
  rows.append(dict(Name=name,Command=list(map(str,args)),Exit=r.returncode,ExpectedRejection=reject,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started));save()
  print(name+': '+str(r.returncode),flush=True);assert r.returncode==(1 if reject else 0),(name,str(log))
  return log.read_text(encoding='utf8',errors='replace')
 flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(sdk)]
 if os.name!='nt':flags+=['-D_GNU_SOURCE']
 compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-unused-label','-Wno-missing-field-initializers']
 save()
 try:
  for case in cases:
   source=root/('test/cases/'+case+'.xl');generated=out/(case+'.c');ir=out/(case+'.json')
   run(case+'.emit',[driver,source,'cgen-default',generated]);product(generated);run(case+'.ir',[driver,source,'ir-lowered',ir]);product(ir)
   data=json.loads(ir.read_text());assert data['moduleExecutionOwnershipAbi']==0 and data['moduleContinuationAbi']==0 and data['moduleTaskPoolAbi']=='none'
   text=generated.read_text();assert 'xl6NetworkService' not in text and 'xlir_async_pool' not in text and 'xlir_execution_source' not in text
   for opt in ('O0','O2'):
    obj=out/(case+'.'+opt+'.o');run(case+'.'+opt,[cc,*flags,*compat,'-'+opt,'-c',generated,'-o',obj]);product(obj)
   for mode in ('run','module'):run(case+'.'+mode,[driver,source,mode,out/(case+'.'+mode)])
   prefix=out/(case+'.aot');exe=Path(str(prefix)+('.exe' if os.name=='nt' else '.out'));arg=prefix if os.name=='nt' else exe
   run(case+'.aot.build',[driver,source,'exe',arg]);product(exe);run(case+'.aot.run',[exe])
   native=Path(str(arg)+'.temps')/'xl6_xrt_impl.c';nativeText=native.read_text();product(native);assert '_MODULE_ALL' not in nativeText
   for mod in ('NET_ENGINE','NET_DNS','NET_RESOLVER','EXECUTOR','TASK_POOL','TLS','NET_TCP','NET_UDP'):
    assert '#define XRT_MODULE_'+mod+'\n' not in nativeText,mod
   assert ('#define XRT_MODULE_NET_SOCKET\n' in nativeText)==(case.startswith('stdlib_net_socket') or case=='stdlib_net_datagram_io')
   assert '#define XRT_MODULE_BUFFER\n' not in nativeText or case in ('stdlib_net_socket_tcp','stdlib_net_socket_udp','stdlib_net_datagram_io')
   imports=run(case+'.imports',['E:/software/w64devkit/bin/objdump.exe','-p',exe] if os.name=='nt' else ['readelf','-d',exe])
   assert not any(name in imports.lower() for name in ('xl.dll','xlc.dll','libxl.so','libxlc.so'))
  for case in negatives:
   text=run(case+'.refused',[driver,root/('test/cases/'+case+'.xl'),'cgen-default',out/(case+'.c')],True)
   reason='C ABI extern product requires a closed primitive product profile' if case.startswith('bad_native_product_') else \
    "no matching callable class member 'NetSocket.enableMetadata'" if case.endswith('datagram_flags') else \
    "no matching callable class member 'NetSocket.receiveFromInto'" if case.endswith('datagram_info') else \
    "no matching callable class member 'NetSocket.setOption'" if case.endswith('_option') else '没有匹配的 create 重载'
   assert reason in text,text
   assert not (out/(case+'.c')).exists()
  objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
  assert len(objects)==len(build['Objects'])-1
  for contract in contracts:
   for opt in ('O0','O2'):
    label=contract.stem+'.'+opt
    obj=out/(label+'.o');run(label+'.build',[cc,*flags,'-'+opt,'-c',contract,'-o',obj]);product(obj)
    binary=out/(label+('.exe' if os.name=='nt' else ''))
    if os.name=='nt':link=[*objects,obj,root/'release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32','-o',binary]
    else:
     lib=a.native_root.resolve()/'release';link=[*objects,obj,'-L'+str(lib),'-lxl','-Wl,-rpath,'+str(lib),'-Wl,--export-dynamic','-pthread','-ldl','-lm','-o',binary]
    response=out/(label+'.rsp');response.write_text('\n'.join('"'+str(x).replace('\\','/')+'"' for x in link)+'\n',encoding='utf8');product(response)
    run(label+'.link',[cc,'@'+str(response)]);product(binary)
    if os.name=='nt' and not (out/'xl.dll').exists():shutil.copy2(root/'release/xl.dll',out/'xl.dll');product(out/'xl.dll')
    proof=run(label+'.run',[binary,root/('test/cases/stdlib_net_datagram_io.xl' if contract.stem=='xlir_net_datagram_contract' else 'test/cases/stdlib_net_socket_tcp.xl')]);assert 'XLIR:' in proof
  report['InputsUnchanged']=all(sha(f)==h for f,h in inputs.items()) and all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs'])
  report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows);assert report['InputsUnchanged'] and report['LogsUnchanged'];report['Complete']=True;save()
 except BaseException as error:failures.append(repr(error));save();raise
if __name__=='__main__':main()
