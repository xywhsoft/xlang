"""Managed RSA: owned components and exact hash/MGF/salt contracts,
current-source JIT/module/AOT, selected native ABI/OOM/leaf ownership.
Offline positive fixed data and ordinary mismatch only, not security/shipping.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess

CASES=["stdlib_crypto_rsa_values","stdlib_crypto_rsa_public_import_only","stdlib_crypto_rsa_private_import_only","stdlib_crypto_rsa_public_parameter_only","stdlib_crypto_rsa_private_parameter_only","stdlib_crypto_rsa_public_projection_only","stdlib_crypto_rsa_enums_only","stdlib_crypto_rsa_raw_only","stdlib_crypto_rsa_pss_verify_only","stdlib_crypto_rsa_pkcs1_verify_only","stdlib_crypto_rsa_pss_sign_only","stdlib_crypto_rsa_pss_salt_only","stdlib_crypto_rsa_threads","stdlib_crypto_nist_values","stdlib_crypto_ed25519_values","stdlib_crypto_x25519_import_only","stdlib_crypto_x448_import_only","stdlib_crypto_xdh_raw_only","stdlib_crypto_sha256_only"]
PROVIDERS=["xl6_crypto_rsa_public","xl6_crypto_rsa_public_domain","xl6_crypto_rsa_private","xl6_crypto_rsa_private_domain","xl6_crypto_rsa_public_raw","xl6_crypto_rsa_private_raw","xl6_crypto_rsa_pss_verify","xl6_crypto_rsa_pkcs1_verify","xl6_crypto_rsa_pss_sign","xl6_crypto_rsa_pss_sign_salt","xl6_crypto_rsa_pkcs1_sign"]

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
 contract=root/'test/cases/xlir_crypto_rsa_contract.c';native=root/'test/support/crypto_rsa_runtime.c'
 plannerSource=root/'test/support/container_projection_artifact.c'
 files={Path(__file__).resolve(),manifest,driver,gcc,tcc,runtime,contract,native,plannerSource,
  root/'test/support/crypto_rsa_plan.c',*[root/('test/cases/'+case+'.xl') for case in CASES]}
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
  expected=set(["xl6CryptoRsaPublicCreateV1","xl6CryptoRsaPublicDestroyV1","xl6CryptoRsaPublicModulusV1","xl6CryptoRsaPublicExponentV1","xl6CryptoRsaPublicSizeV1","xl6CryptoRsaPublicBitsV1","xl6CryptoRsaPrivateCreateV1","xl6CryptoRsaPrivateCreateCrtV1","xl6CryptoRsaPrivateDestroyV1","xl6CryptoRsaPrivatePublicV1","xl6CryptoRsaPrivateExportPartV1","xl6CryptoRsaPrivateHasCrtV1","xl6CryptoRsaPublicRawV1","xl6CryptoRsaPrivateRawV1","xl6CryptoRsaVerifyPssHashV1","xl6CryptoRsaVerifyPssHashWithMaskV1","xl6CryptoRsaVerifyPssHashAnySaltV1","xl6CryptoRsaVerifyPssHashAnySaltWithMaskV1","xl6CryptoRsaVerifyPkcs1HashV1","xl6CryptoRsaSignPssHashV1","xl6CryptoRsaSignPssHashWithMaskV1","xl6CryptoRsaSignPssHashSaltV1","xl6CryptoRsaSignPssHashSaltWithMaskV1","xl6CryptoRsaSignPkcs1HashV1"])
  exported=set(re.findall(r'XL6_TEXT_EXPORT\((xl6CryptoRsa\w+),',(root/'src/runtime/xl6_crypto_symbols.inc').read_text()))
  declared=set(re.findall(r'XL6_TEXT_API [^;\n]*?\b(xl6CryptoRsa\w+)\(',(root/'src/runtime/xl6_crypto_abi.h').read_text()))
  contracted=set(re.findall(r'\{"(xl6CryptoRsa\w+)"',(root/'src/ir/xl6_ir_crypto_contracts.inc').read_text()))
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
   assert not re.search(r'xl6_xrt_rsa_',text)
   assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6CryptoRsa\w*\([^;]*\)\s*\{',text,re.M)
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
 negatives={"private":"func bad(RsaPrivateKey key)\n auto x=key.handle\nend\n","default":"auto key=RsaPrivateKey()\n","string-key":"auto key=RsaPublicKey(\"modulus\",\"exponent\")\n","readonly":"func bad(RsaPublicKey key)\n key.modulus=b\"data\"\nend\n","wrong-digest":"func bad(RsaPrivateKey key)\n key.signPkcs1Hash(\"text\")\nend\n","integer-hash":"func bad(RsaPrivateKey key,bytes digest)\n key.signPkcs1Hash(digest,3)\nend\n","wrong-enum":"func bad(RsaPrivateKey key,bytes digest)\n key.signPkcs1Hash(digest,SignatureHash.sha256)\nend\n","wrong-part":"func bad(RsaPrivateKey key)\n key.exportPart(RsaHash.sha256)\nend\n","private-as-public":"func bad(RsaPrivateKey key,bytes value)\n cryptoAdvanced.rsaPublic(key,value)\nend\n","public-as-private":"func bad(RsaPublicKey key,bytes value)\n cryptoAdvanced.rsaPrivate(key,value)\nend\n","old-pss":"auto value=crypto.rsaPssVerify(\"hash\",\"signature\",\"modulus\",\"exponent\")\n","old-pkcs1":"auto value=crypto.rsaPkcs1Verify(\"hash\",\"signature\",\"modulus\",\"exponent\")\n"}
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
   assert 'RSA XLIR:' in run('contract.'+opt+'.run',[binary,root/'test/cases/stdlib_crypto_rsa_values.xl'])
  except Exception as error:report['Failures'].append(dict(Name='contract.'+opt,Error=repr(error)));save()
 try:
  nativePlanner=out/('native-planner'+suffix)
  run('native-planner.build',[gcc,*flags,*strict,'-O0','-Wno-missing-field-initializers',
   '-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables',
   root/'test/support/crypto_rsa_plan.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',nativePlanner])
  product(nativePlanner)
  selected=run('native-plan.sources',[nativePlanner,'--sources']).splitlines()
  modules=run('native-plan.modules',[nativePlanner,'--modules'])
  assert len(selected)==len(set(selected)) and not any('net_' in s or 'executor' in s for s in selected)
  report['NativeAdapterClosure']=dict(Sources=selected,Modules=modules);save()
  moduleHeader=out/'rsa-native-modules.h';moduleHeader.write_text(modules,encoding='utf8');product(moduleHeader)
  implementation=out/'rsa-native.c'
  body='#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n' if '#define XRUNTIME_MODULE_' in modules else '#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n'
  implementation.write_text('#include "rsa-native-modules.h"\n#define XRT_MODULE_MEMORY_DEBUG\n'+body,encoding='utf8');product(implementation)
  modes=[('gcc.O0',gcc,['-O0']),('gcc.O2',gcc,['-O2']),('tcc',tcc,[])]
  if os.name!='nt':modes.append(('native.asan',gcc,['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
  nativeLibraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
  for name,cc,opts in modes:
   try:
    binary=out/(name+suffix)
    run(name+'.build',[cc,*flags,'-I'+str(out),*opts,*([] if cc==tcc else strict),
     native,implementation,*[root/s for s in selected],*nativeLibraries,'-o',binary]);product(binary)
    assert 'managed RSA:' in run(name+'.run',[binary])
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
   if case in ('stdlib_crypto_rsa_public_import_only','stdlib_crypto_rsa_pkcs1_verify_only','stdlib_crypto_rsa_pss_verify_only','stdlib_crypto_rsa_public_parameter_only'):
    assert 'xl6_crypto_rsa_private' not in sources and 'XRT_MODULE_CRYPTO_RSA_PRIVATE' not in modules
    assert 'XRT_MODULE_RANDOM_SECURE' not in modules
    assert not any(m.endswith('_SIGN') for m in modules)
   if case in ('stdlib_crypto_rsa_public_import_only','stdlib_crypto_rsa_private_import_only','stdlib_crypto_rsa_public_projection_only','stdlib_crypto_rsa_raw_only'):
    assert not any(m in modules for m in ('XRT_MODULE_CRYPTO_RSA_PSS','XRT_MODULE_CRYPTO_RSA_PSS_SIGN','XRT_MODULE_CRYPTO_RSA_PKCS1','XRT_MODULE_CRYPTO_RSA_PKCS1_SIGN'))
   if case in ('stdlib_crypto_rsa_pkcs1_verify_only','stdlib_crypto_rsa_public_parameter_only'):
    assert not any(m.startswith('XRT_MODULE_CRYPTO_SHA') for m in modules)
   if case in ('stdlib_crypto_rsa_pss_sign_only','stdlib_crypto_rsa_pss_salt_only','stdlib_crypto_rsa_pss_verify_only'):
    assert not any('PKCS1' in m for m in modules)
   if case in ('stdlib_crypto_rsa_private_parameter_only',):
    assert not any('PSS' in m for m in modules)
   if case in ('stdlib_crypto_nist_values','stdlib_crypto_ed25519_values','stdlib_crypto_x25519_import_only','stdlib_crypto_x448_import_only','stdlib_crypto_xdh_raw_only','stdlib_crypto_sha256_only','stdlib_crypto_rsa_enums_only'):
    assert not any(s in sources for s in PROVIDERS)
 except Exception as error:report['Failures'].append(dict(Name='plans',Error=repr(error)));save()
 report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items()) and all(
  sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers'])
 report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in report['Rows'])
 report['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in report['Products'])
 report['Complete']=not report['Failures'] and all(report[k] for k in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'))
 save();assert report['Complete'],report['Failures']

if __name__=='__main__':main()
