"""Managed P256/P384: fixed public vectors, exact hash/format contracts,
selected current-source JIT/module/AOT, complete native ABI/OOM/leaf ownership.
Offline ordinary function tests only, not malformed/security or shipping gates.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess

CASES=["stdlib_crypto_p256_import_only","stdlib_crypto_p256_parameter_only","stdlib_crypto_p256_verify_only","stdlib_crypto_p384_import_only","stdlib_crypto_p384_parameter_only","stdlib_crypto_p384_verify_only","stdlib_crypto_nist_values","stdlib_crypto_nist_public_only","stdlib_crypto_nist_der_only","stdlib_crypto_nist_raw_only","stdlib_crypto_signature_hash_only","stdlib_crypto_nist_threads","stdlib_crypto_ed25519_values","stdlib_crypto_x25519_import_only","stdlib_crypto_x448_import_only","stdlib_crypto_xdh_raw_only","stdlib_crypto_sha256_only"]
PROVIDERS=["xl6_crypto_p256","xl6_crypto_p256_domain","xl6_crypto_p256_generate","xl6_crypto_p256_shared","xl6_crypto_p256_sign","xl6_crypto_p256_sign_der","xl6_crypto_p256_public","xl6_crypto_p256_verify","xl6_crypto_p256_verify_der","xl6_crypto_p256_raw","xl6_crypto_p384","xl6_crypto_p384_domain","xl6_crypto_p384_generate","xl6_crypto_p384_shared","xl6_crypto_p384_sign","xl6_crypto_p384_sign_der","xl6_crypto_p384_public","xl6_crypto_p384_verify","xl6_crypto_p384_verify_der","xl6_crypto_p384_raw","xl6_crypto_nist_der"]

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
 contract=root/'test/cases/xlir_crypto_nist_contract.c';native=root/'test/support/crypto_nist_runtime.c'
 plannerSource=root/'test/support/container_projection_artifact.c'
 files={Path(__file__).resolve(),manifest,driver,gcc,tcc,runtime,contract,native,plannerSource,
  root/'test/support/crypto_nist_plan.c',*[root/('test/cases/'+case+'.xl') for case in CASES]}
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
  expected=set(["xl6CryptoP256CreateV1","xl6CryptoP256DestroyV1","xl6CryptoP256PublicV1","xl6CryptoP256ExportPrivateV1","xl6CryptoP256GenerateV1","xl6CryptoP256SharedV1","xl6CryptoP256SignHashV1","xl6CryptoP256SignHashDerV1","xl6CryptoP256PublicFromPrivateV1","xl6CryptoP256VerifyHashV1","xl6CryptoP256VerifyHashDerV1","xl6CryptoP256ValidV1","xl6CryptoP256MultiplyV1","xl6CryptoP256AddV1","xl6CryptoP384CreateV1","xl6CryptoP384DestroyV1","xl6CryptoP384PublicV1","xl6CryptoP384ExportPrivateV1","xl6CryptoP384GenerateV1","xl6CryptoP384SharedV1","xl6CryptoP384SignHashV1","xl6CryptoP384SignHashDerV1","xl6CryptoP384PublicFromPrivateV1","xl6CryptoP384VerifyHashV1","xl6CryptoP384VerifyHashDerV1","xl6CryptoP384ValidV1","xl6CryptoP384MultiplyV1","xl6CryptoP384AddV1","xl6CryptoEcdsaP256ToDerV1","xl6CryptoEcdsaP256FromDerV1","xl6CryptoEcdsaP384ToDerV1","xl6CryptoEcdsaP384FromDerV1"])
  exported=set(re.findall(r'XL6_TEXT_EXPORT\((xl6Crypto(?:P256|P384|EcdsaP256|EcdsaP384)\w+),',(root/'src/runtime/xl6_crypto_symbols.inc').read_text()))
  declared=set(re.findall(r'XL6_TEXT_API [^;\n]*?\b(xl6Crypto(?:P256|P384|EcdsaP256|EcdsaP384)\w+)\(',(root/'src/runtime/xl6_crypto_abi.h').read_text()))
  contracted=set(re.findall(r'\{"(xl6Crypto(?:P256|P384|EcdsaP256|EcdsaP384)\w+)"',(root/'src/ir/xl6_ir_crypto_contracts.inc').read_text()))
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
   assert not re.search(r'xl6_xrt_(?:p256|p384|ecdsa)',text)
   assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Crypto(?:P256|P384|EcdsaP256|EcdsaP384)\w*\([^;]*\)\s*\{',text,re.M)
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
 negatives={"private":"func bad(P256Key key)\n auto value=key.handle\nend\n","default":"auto key=P384Key()\n","string-key":"auto key=P256Key(\"not raw private key bytes\")\n","string-digest":"func bad(P256Key key)\n key.signHash(\"not digest bytes\")\nend\n","readonly":"func bad(P384Key key)\n key.publicKey=b\"data\"\nend\n","old-keypair":"auto key=crypto.p256Keypair()\n","old-shared":"auto value=crypto.p384SharedSecret(\"private\",\"public\")\n","old-verify":"auto value=crypto.ecdsaVerify(\"public\",\"hash\",\"signature\")\n","integer-hash":"func bad(P256Key key,bytes digest)\n key.signHash(digest,3)\nend\n","wrong-enum":"enum Other as int\n sha256 = 3\nend\nfunc bad(P256Key key,bytes digest)\n key.signHash(digest,Other.sha256)\nend\n"}
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
   assert 'NIST XLIR:' in run('contract.'+opt+'.run',[binary,root/'test/cases/stdlib_crypto_nist_values.xl'])
  except Exception as error:report['Failures'].append(dict(Name='contract.'+opt,Error=repr(error)));save()
 try:
  nativePlanner=out/('native-planner'+suffix)
  run('native-planner.build',[gcc,*flags,*strict,'-O0','-Wno-missing-field-initializers',
   '-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables',
   root/'test/support/crypto_nist_plan.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',nativePlanner])
  product(nativePlanner)
  selected=run('native-plan.sources',[nativePlanner,'--sources']).splitlines()
  modules=run('native-plan.modules',[nativePlanner,'--modules'])
  assert len(selected)==len(set(selected)) and not any('net_' in s or 'executor' in s for s in selected)
  report['NativeAdapterClosure']=dict(Sources=selected,Modules=modules);save()
  moduleHeader=out/'nist-native-modules.h';moduleHeader.write_text(modules,encoding='utf8');product(moduleHeader)
  implementation=out/'nist-native.c'
  body='#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n' if '#define XRUNTIME_MODULE_' in modules else '#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n'
  implementation.write_text('#include "nist-native-modules.h"\n#define XRT_MODULE_MEMORY_DEBUG\n'+body,encoding='utf8');product(implementation)
  modes=[('gcc.O0',gcc,['-O0']),('gcc.O2',gcc,['-O2']),('tcc',tcc,[])]
  if os.name!='nt':modes.append(('native.asan',gcc,['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
  nativeLibraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
  for name,cc,opts in modes:
   try:
    binary=out/(name+suffix)
    run(name+'.build',[cc,*flags,'-I'+str(out),*opts,*([] if cc==tcc else strict),
     native,implementation,*[root/s for s in selected],*nativeLibraries,'-o',binary]);product(binary)
    assert 'managed NIST:' in run(name+'.run',[binary])
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
   sources={Path(s).stem for s in plan['Sources']};modules=set(plan['NativeModules'])
   managed_import=case in ('stdlib_crypto_p256_import_only','stdlib_crypto_p384_import_only')
   if managed_import:
    assert 'XRT_MODULE_RANDOM_SECURE' not in modules
    assert not any(m.startswith('XRT_MODULE_CRYPTO_ECDSA_') for m in modules)
    assert not any(s.endswith(('_sign','_sign_der','_generate','_shared','_verify','_verify_der','_raw')) for s in sources if s in PROVIDERS)
   if case.endswith(('_public_only','_verify_only','_der_only','_raw_only')) and case.startswith(('stdlib_crypto_p','stdlib_crypto_nist')):
    assert 'xl6_native_handle' not in sources and 'xl6_crypto_p256' not in sources and 'xl6_crypto_p384' not in sources
    assert 'XRT_MODULE_RANDOM_SECURE' not in modules
   if case.endswith('_verify_only'):
    assert not any(m.endswith(('_SIGN','_SIGN_DER')) for m in modules)
   if case in ('stdlib_crypto_p256_parameter_only','stdlib_crypto_p384_parameter_only'):
    assert 'XRT_MODULE_RANDOM_SECURE' not in modules and 'xl6_buffer_core' not in sources
    assert not any(m.endswith(('_DER','_KEYPAIR')) for m in modules)
   if case.startswith('stdlib_crypto_p256_'):
    assert not any('P384' in m for m in modules) and not any('p384' in s for s in sources)
   if case.startswith('stdlib_crypto_p384_'):
    assert not any('P256' in m for m in modules) and not any('p256' in s for s in sources)
   if case=='stdlib_crypto_nist_der_only':
    assert not any(m in modules for m in ('XRT_MODULE_CRYPTO_P256','XRT_MODULE_CRYPTO_P384'))
   if case in ('stdlib_crypto_ed25519_values','stdlib_crypto_x25519_import_only','stdlib_crypto_x448_import_only','stdlib_crypto_xdh_raw_only','stdlib_crypto_sha256_only','stdlib_crypto_signature_hash_only'):
    assert not any(s in sources for s in PROVIDERS)
 except Exception as error:report['Failures'].append(dict(Name='plans',Error=repr(error)));save()
 report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items()) and all(
  sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers'])
 report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in report['Rows'])
 report['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in report['Products'])
 report['Complete']=not report['Failures'] and all(report[k] for k in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'))
 save();assert report['Complete'],report['Failures']

if __name__=='__main__':main()
