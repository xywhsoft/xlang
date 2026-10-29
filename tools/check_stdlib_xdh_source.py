"""Managed X25519/X448: current-source JIT/module/AOT, offline fixed vectors,
adapter ABI/OOM/leaf graphs, closed IR and selected native closure.
No traffic, malformed point/signature campaigns or security-specific tests.
This is not whole P4, X.509/TLS, shipping DLL/SO/VFS or whole-goal acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess

CASES=('stdlib_crypto_xdh_values','stdlib_crypto_x25519_import_only',
 'stdlib_crypto_x448_import_only','stdlib_crypto_xdh_raw_only',
 'stdlib_crypto_xdh_threads','stdlib_crypto_xdh_parameter_only','stdlib_crypto_sha256_only')
PROVIDERS=tuple('xl6_crypto_'+curve+suffix for curve in ('x25519','x448')
 for suffix in ('','_domain','_generate','_raw'))

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--source-build',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
 a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.out.resolve();manifest=a.source_build.resolve()
 sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
 build=json.loads(manifest.read_text(encoding='utf-8-sig'))
 assert not out.exists() and build.get('Complete',True) and build['SourceXrt'] and not build['RuntimeOnly']
 for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
 driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
 gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
 tcc=Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve();assert tcc.is_file()
 runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
 contract=root/'test/cases/xlir_crypto_xdh_contract.c';native=root/'test/support/crypto_xdh_runtime.c'
 plannerSource=root/'test/support/container_projection_artifact.c'
 files={Path(__file__).resolve(),manifest,driver,gcc,tcc,runtime,contract,native,plannerSource,
  root/'test/support/crypto_xdh_plan.c',*[root/('test/cases/'+case+'.xl') for case in CASES]}
 inputs={str(path):sha(path) for path in files};out.mkdir(parents=True);env=dict(os.environ)
 if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
 else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
 report=dict(Complete=False,Inputs=inputs,Rows=[],Products=[],Failures=[],Plans=[],SourceBuild=str(manifest),
  SourceProductAcceptance=True,ShippingLibraryAcceptance=False,NetworkTraffic=False,NetworkSecurityTests=False,Scope=__doc__)
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
 def product(path):report['Products'].append(dict(Path=str(path),Sha256=sha(path)));save()
 def run(name,args,reject=False):
  command=list(map(str,args));invocation=command
  if command[0] in (str(gcc),str(tcc)):
   response=out/(name+'.rsp')
   response.write_text('\n'.join('"'+arg.replace('\\','/').replace('"','\\"')+'"' for arg in command[1:])+'\n',encoding='utf8')
   product(response);invocation=[command[0],'@'+str(response)]
  log=out/(name+'.log')
  with log.open('wb') as stream:
   try:code=subprocess.run(invocation,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240).returncode
   except subprocess.TimeoutExpired:code=None
  report['Rows'].append(dict(Name=name,Command=command,Invocation=list(map(str,invocation)),Exit=code,
   ExpectedRejection=reject,Log=str(log),Sha256=sha(log)));save();print(name,code,flush=True)
  assert code==(1 if reject else 0),(name,str(log))
  return log.read_text(encoding='utf8',errors='replace')
 flags=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),
  '-I'+str(root/'release/include'),'-I'+str(driver.parent/'include')]
 if os.name!='nt':flags+=['-D_GNU_SOURCE']
 strict=['-Wall','-Wextra','-Werror']
 compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable',
  '-Wno-unused-label','-Wno-unused-const-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
 objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in
  ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
 assert len(objects)==len(build['Objects'])-1
 libraries=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else
  ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)])]
 suffix='.exe' if os.name=='nt' else '';save()
 try:
  expected={f'xl6Crypto{curve}{operation}V1' for curve in ('X25519','X448')
   for operation in ('Create','Destroy','Public','ExportPrivate','Shared','Generate','Raw')}
  exported=set(re.findall(r'XL6_TEXT_EXPORT\((xl6CryptoX(?:25519|448)\w+),',(root/'src/runtime/xl6_crypto_symbols.inc').read_text()))
  declared=set(re.findall(r'XL6_TEXT_API [^;\n]*?\b(xl6CryptoX(?:25519|448)\w+)\(',(root/'src/runtime/xl6_crypto_abi.h').read_text()))
  contracted=set(re.findall(r'\{"(xl6CryptoX(?:25519|448)\w+)"',(root/'src/ir/xl6_ir_crypto_contracts.inc').read_text()))
  assert expected==exported==declared==contracted
  report['ResidentEntries']=len(expected);save()
  count=int(re.search(r'#define XL6_STDLIB_FUNC_COUNT (\d+)',(root/'src/stdlib/xl6_stdlib_data.h').read_text()).group(1))
  for wrong in (count-1,count+1):
   path=out/f'bad.index.{wrong}.c'
   path.write_text(f'#define XL6_STDLIB_FUNC_COUNT {wrong}\n#define XL6_STDLIB_FUNC_INDEX_CAPACITY 4096\n#include "src/stdlib/xl6_stdlib_func_index.inc"\n',encoding='utf8');product(path)
   assert 'stdlib function index is stale' in run(f'bad.index.{wrong}',[gcc,*flags,'-fsyntax-only',path],True)
 except Exception as error:report['Failures'].append(dict(Name='registration',Error=repr(error)));save()
 for case in CASES:
  try:
   source=root/('test/cases/'+case+'.xl');generated=out/(case+'.c')
   run(case+'.emit',[driver,source,'cgen-default',generated]);product(generated);text=generated.read_text(encoding='utf8')
   assert 'xl6_xrt_x25519' not in text and 'xl6_xrt_x448' not in text
   assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6CryptoX(?:25519|448)\w*\([^;]*\)\s*\{',text,re.M)
   run(case+'.jit',[driver,source,'run',out/(case+'.jit')])
   for stage in ('typed','lowered'):
    path=out/(case+'.'+stage+'.json');run(case+'.'+stage,[driver,source,'ir-'+stage,path]);product(path)
   for opt in ('O0','O2'):
    obj=out/(case+'.'+opt+'.o');run(case+'.'+opt,[gcc,*flags,*strict,*compat,'-'+opt,'-c',generated,'-o',obj]);product(obj)
   run(case+'.module',[driver,source,'module',out/(case+'.module')])
   binary=out/(case+'.aot'+suffix);target=binary.with_suffix('') if os.name=='nt' else binary
   run(case+'.aot.build',[driver,source,'exe',target]);product(binary);run(case+'.aot.run',[binary])
   implementation=Path(str(target)+'.temps')/'xl6_xrt_impl.c';product(implementation)
   assert '_MODULE_ALL' not in implementation.read_text(encoding='utf8')
   imports=run(case+'.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
   assert not any(name in imports.lower() for name in ('xl.dll','xlc.dll','libxl.so','libxlc.so'))
  except Exception as error:report['Failures'].append(dict(Name=case,Error=repr(error)));save()
 negatives={
  'private':'func bad(X25519Key key)\n auto value=key.handle\nend\n',
  'default':'auto key=X448Key()\n',
  'string':'auto key=X25519Key("not bytes")\n',
  'wrong-argument':'func bad(X25519Key key)\n key.sharedSecret("not bytes")\nend\n',
  'readonly':'func bad(X448Key key)\n key.publicKey=b"data"\nend\n',
  'old-keypair':'auto key=crypto.x25519Keypair()\n',
  'old-shared':'auto key=crypto.x448SharedSecret("private","public")\n',
 }
 for name,body in negatives.items():
  try:
   path=out/('bad.'+name+'.xl');path.write_text(body,encoding='utf8');product(path);target=out/('bad.'+name+'.c')
   message=run('bad.'+name,[driver,path,'cgen-default',target],True)
   assert any(tag in message for tag in ('[sem]','[ir]','[checked]','[stdlib]')) and not target.exists(),message
  except Exception as error:report['Failures'].append(dict(Name=name,Error=repr(error)));save()
 for opt in ('O0','O2'):
  try:
   obj=out/('contract.'+opt+'.o');binary=out/('contract.'+opt+suffix)
   run('contract.'+opt+'.compile',[gcc,*flags,*strict,'-'+opt,'-c',contract,'-o',obj]);product(obj)
   run('contract.'+opt+'.link',[gcc,*objects,obj,*libraries,'-o',binary]);product(binary)
   assert 'XDH XLIR:' in run('contract.'+opt+'.run',[binary,root/'test/cases/stdlib_crypto_xdh_values.xl'])
  except Exception as error:report['Failures'].append(dict(Name='contract.'+opt,Error=repr(error)));save()
 try:
  nativePlanner=out/('native-planner'+suffix)
  run('native-planner.build',[gcc,*flags,*strict,'-O0','-Wno-missing-field-initializers',
   '-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables',
   root/'test/support/crypto_xdh_plan.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',nativePlanner])
  product(nativePlanner)
  selected=run('native-plan.sources',[nativePlanner,'--sources']).splitlines()
  modules=run('native-plan.modules',[nativePlanner,'--modules'])
  assert len(selected)==len(set(selected)) and not any('net_' in s or 'executor' in s for s in selected)
  report['NativeAdapterClosure']=dict(Sources=selected,Modules=modules);save()
  moduleHeader=out/'xdh-native-modules.h';moduleHeader.write_text(modules,encoding='utf8');product(moduleHeader)
  implementation=out/'xdh-native.c'
  body='#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n' if '#define XRUNTIME_MODULE_' in modules else '#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n'
  implementation.write_text('#include "xdh-native-modules.h"\n#define XRT_MODULE_MEMORY_DEBUG\n'+body,encoding='utf8');product(implementation)
  modes=[('gcc.O0',gcc,['-O0']),('gcc.O2',gcc,['-O2']),('tcc',tcc,[])]
  if os.name!='nt':modes.append(('native.asan',gcc,['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
  nativeLibraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
  for name,cc,opts in modes:
   try:
    binary=out/(name+suffix)
    run(name+'.build',[cc,*flags,'-I'+str(out),*opts,*([] if cc==tcc else strict),
     native,implementation,*[root/s for s in selected],*nativeLibraries,'-o',binary]);product(binary)
    assert 'managed XDH:' in run(name+'.run',[binary])
   except Exception as error:report['Failures'].append(dict(Name=name,Error=repr(error)));save()
 except Exception as error:report['Failures'].append(dict(Name='native-planning',Error=repr(error)));save()
 try:
  planner=out/('planner'+suffix)
  run('planner.build',[gcc,*flags,*strict,plannerSource,*objects,*libraries,'-o',planner]);product(planner)
  for case in CASES:
   prefix=out/(case+'.selected');run(case+'.plan',[planner,root/('test/cases/'+case+'.xl'),prefix])
   path=Path(str(prefix)+'.plan.json');product(path);product(Path(str(prefix)+'.xrt.c'))
   plan=json.loads(path.read_text());report['Plans'].append(dict(Fixture=case,**plan));save()
   assert not any(m.startswith(('XRT_MODULE_NET_','XRT_MODULE_TLS','XRT_MODULE_X509')) for m in plan['NativeModules'])
   if 'import_only' in case:
    assert 'XRT_MODULE_RANDOM_SECURE' not in plan['NativeModules']
    curve='x25519' if 'x25519' in case else 'x448';other='x448' if curve=='x25519' else 'x25519'
    assert 'src/runtime/xl6_crypto_'+curve+'.c' in plan['Sources']
    assert not any(Path(s).stem in tuple('xl6_crypto_'+c+suffix for c in (curve,other)
     for suffix in (('_generate','_raw') if c==curve else ('','_domain','_generate','_raw'))) for s in plan['Sources'])
   if case=='stdlib_crypto_xdh_raw_only':
    assert not any(Path(s).stem in tuple('xl6_crypto_'+c+suffix for c in ('x25519','x448')
     for suffix in ('','_domain','_generate')) for s in plan['Sources'])
   if case=='stdlib_crypto_xdh_parameter_only':
    assert 'XRT_MODULE_RANDOM_SECURE' not in plan['NativeModules']
    assert not any(Path(s).stem.endswith(('_generate','_raw')) and Path(s).stem.startswith(('xl6_crypto_x25519','xl6_crypto_x448')) for s in plan['Sources'])
   if case=='stdlib_crypto_sha256_only':
    assert not any(Path(s).stem in PROVIDERS for s in plan['Sources'])
 except Exception as error:report['Failures'].append(dict(Name='plans',Error=repr(error)));save()
 report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items()) and all(
  sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers'])
 report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in report['Rows'])
 report['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in report['Products'])
 report['Complete']=not report['Failures'] and all(report[k] for k in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'))
 save();assert report['Complete'],report['Failures']

if __name__=='__main__':main()
