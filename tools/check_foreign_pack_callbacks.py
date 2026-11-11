"""Complete-compiler packed synchronous callback acceptance.

No device, window, audio device or network. Tests field ABI, independent callback
contexts, existing failure rules, scoped resources/epochs, OOM, nested compiler
and the real XGE CPU decoder against matched dynamic/static SDKs.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
from full_compiler_qualification import verify_full_compiler

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','compiler','compiler-report','dynamic-packages','static-packages','out'):
  p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--ir-checker',type=Path);p.add_argument('--ir-report',type=Path);a=p.parse_args()
 assert (a.ir_checker is None)==(a.ir_report is None)
 root,compiler,meta,dynamic,static,out=[v.resolve() for v in (a.root,a.compiler,a.compiler_report,a.dynamic_packages,a.static_packages,a.out)]
 sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest();metadata=verify_full_compiler(meta,compiler);sdk=compiler.parent/'include'
 frozen=json.loads(Path(metadata['sourceFreeze']).read_text());assert all(sha(root/name)==digest for name,digest in frozen['overrides'].items())
 cc=Path(shutil.which('gcc')).resolve();cpp=Path(shutil.which('g++')).resolve();objdump=Path(shutil.which('objdump')).resolve()
 manifest=json.loads((dynamic/'_xge_native/xlang.package.json').read_text());image=dynamic/'_xge_native'/manifest['links'][0]
 assert sha(image)==manifest['native']['sha256']
 case=root/'test/cases/foreign_pack_callback.xl';clip=root/'test/cases/foreign_pack_clip.xl';nested=root/'test/cases/foreign_pack_callback_nested.xl'
 files=[Path(__file__).resolve(),Path(__file__).with_name('full_compiler_qualification.py'),Path(metadata['sourceFreeze']),meta,compiler,cc,cpp,objdump]
 for file in (case,clip,nested):
  files.append(file)
  for ext in ('.c','.h'):
   if file.with_suffix(ext).is_file():files.append(file.with_suffix(ext))
 files.append(root/'test/cases/foreign_pack_callback_ir.c')
 files.append(root/'test/cases/foreign_callback_span.h')
 files.append(root/'test/cases/foreign_callback_span_native.c')
 regressions={
  'foreign_pack':['foreign_pack_native.c'],
  'foreign_sync_callback':['foreign_sync_callback_native.c'],
  'foreign_nested_pack_null':['foreign_nested_pack_null_native.c'],
  'foreign_pack_pod':['foreign_pack_pod_native.c']}
 for name,providers in regressions.items():files.extend([root/'test/cases'/(name+'.xl'),root/'test/cases'/(name+'.h'),*[root/'test/cases'/v for v in providers]])
 for name in ('xl.dll','xlc.dll') if os.name=='nt' else ('libxl.so','libxlc.so'):files.append(compiler.parent/name)
 for folder in (sdk,dynamic,static):files.extend(v for v in folder.rglob('*') if v.is_file())
 if a.ir_checker:
  ir,irReport=a.ir_checker.resolve(),a.ir_report.resolve();gate=json.loads(irReport.read_text());assert gate['passed']
  frozen=json.loads(Path(metadata['sourceFreeze']).read_text())
  assert all(gate['overrides'][name]==digest for name,digest in frozen['overrides'].items())
  files.extend([ir,irReport])
 inputs={str(v):sha(v) for v in files};out.mkdir(parents=True,exist_ok=False)
 report=dict(passed=False,scope=__doc__,fullCompilerBuild=True,shippingDelivery=False,networkTests=False,graphicsDevice=False,audioDevice=False,inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 def run(name,args,*,success=True,packages=None,working=None):
  args=list(map(str,args));env=dict(os.environ,XLANG_PACK_CALLBACK_ROOT=str(root));env.pop('XLANG_PACKAGE_PATH',None)
  if packages:env['XLANG_PACKAGE_PATH']=str(packages)
  if os.name=='nt':env['PATH']=str(image.parent)+';'+str(compiler.parent)+';'+env.get('PATH','')
  else:env['LD_LIBRARY_PATH']=str(image.parent)+':'+str(compiler.parent)+':'+env.get('LD_LIBRARY_PATH','')
  z=subprocess.run(args,cwd=working or compiler.parent,env=env,capture_output=True,timeout=240)
  log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
  report['rows'].append(dict(name=name,command=args,expectedSuccess=success,workingDirectory=str(working or compiler.parent),exitCode=z.returncode,logSha256=sha(log)))
  save();print(name,z.returncode,flush=True);assert (not z.returncode)==success,log;return z
 def imports(name,binary,*,standalone=True):
  z=run(name+'-imports',[objdump,'-p',binary]);deps=re.findall(rb'DLL Name:\s*(\S+)',z.stdout) if os.name=='nt' else re.findall(rb'NEEDED\s+(\S+)',z.stdout)
  forbidden={b'xl.dll',b'xlc.dll',b'libtcc.dll',b'libxl.so',b'libxlc.so'}
  if standalone:forbidden|={b'xge3_host.dll',b'libxge3_host.so'}
  assert not forbidden.intersection(v.lower() for v in deps)
  report.setdefault('binaries',[]).append(dict(path=str(binary),sha256=sha(binary),bytes=binary.stat().st_size,imports=[v.decode('ascii') for v in deps]));save()
 include=['--inc-path',root,'--inc-path',sdk]
 for file in (case,clip,nested):
  name=file.stem;packages=dynamic if file==clip else None;work=root if file==nested else compiler.parent
  generated,header=out/(name+'.c'),out/(name+'.h')
  run(name+'-emit',[compiler,'cgen',file,'--emit',generated,'--emit-header',header],packages=packages,working=work)
  z=run(name+'-jit',[compiler,'run',file,*include],packages=packages,working=work)
  if file==case:assert b'packed scalar callback hot allocations: 0' in z.stdout
  binary=out/(name+('.exe' if os.name=='nt' else ''))
  run(name+'-static-build',[compiler,'build',file,'-o',binary,*include],packages=static if file==clip else None,working=work)
  run(name+'-static-run',[binary],working=work);imports(name,binary)
  if file==clip:
   binary=out/(name+'-dynamic'+('.exe' if os.name=='nt' else ''))
   run(name+'-dynamic-build',[compiler,'build',file,'--native-linkage','dynamic','-o',binary,*include],packages=dynamic,working=work)
   run(name+'-dynamic-run',[binary],working=work);imports(name+'-dynamic',binary,standalone=False)
  text=generated.read_text(encoding='utf8')
  if file!=nested:
   assert 'xl6SyncCallbackBeginV1' in text and '.invocation.self ?' in text and 'xl6NativeCallbackRuntimeV1Get()->Create(' not in text
  if file==clip:assert re.search(r'\bxgeAudioClipDecodeMemory\(',text) and '.cancelled=' in text and '.user=' in text
  if file!=nested:
   probe=out/(name+'-probe.c');probe.write_text('#include "'+header.as_posix()+'"\nint main(void){return 0;}\n')
   flags=['-Wall','-Wextra','-Werror','-Wno-unused-function','-I'+str(root),'-I'+str(sdk)]
   variants=[('dynamic',dynamic),('static',static)] if file==clip else [('',None)]
   for variant,packages in variants:
    extra=['-I'+str(packages/'_xge_native/include'),'-I'+str(packages/'xge/include')] if packages else []
    if variant=='static':extra+=['-DXL6_NATIVE_LINK_SCHEMA=1']
    for lang,std,tool in [('c','c11',cc),('c++','c++11',cpp),('c++','c++17',cpp)]:run(name+'-header-'+variant+'-'+std,[tool,'-x',lang,'-std='+std,*flags,*extra,'-fsyntax-only',probe])
 for name,providers in regressions.items():
  file=root/'test/cases'/(name+'.xl');extra=[root/'test/cases'/v for v in providers]
  run('regression-'+name,[compiler,'run',file,*extra,*include])
  binary=out/(name+('.exe' if os.name=='nt' else ''))
  run('regression-'+name+'-build',[compiler,'build',file,*extra,'-o',binary,*include])
  run('regression-'+name+'-run',[binary]);imports(name,binary)
 original=case.read_text(encoding='utf8');marker='@c_sync_callback(2,-1,-19)';user='@c_pack_field(0,"user",2,"callback_user")';function='@c_pack_field(0,"callback",2,"callback")'
 negatives={
  'missing-user':original.replace(user,'',1),
  'missing-function':original.replace(function,'',1),
  'duplicate-user':original.replace(user,user+' '+user,1),
  'duplicate-function':original.replace(function,function+' '+function,1),
  'missing-scope':original.replace(marker,'',1),
  'escaping-callback':original.replace(marker,'@c_owned_callback(2,-1,"FakeOwner",-19)',1),
  'mismatched-user-source':original.replace(user,'@c_pack_field(0,"user",1,"callback_user")',1),
  'source-outside-group':original.replace(user,'@c_pack_field(0,"user",63,"callback_user")',1),
  'unknown-mode':original.replace(function,'@c_pack_field(0,"callback",2,"callback_unknown")',1),
  'scalar-as-callback':original.replace('Handler? callback) as c_int','c_int callback) as c_int',1),
  'pure-callback':original.replace(marker,marker+' @pure',1),
  'nonblocking-callback':original.replace(marker,marker+' @nonblocking',1)}
 for name,text in negatives.items():
  file=out/(name+'.xl');file.write_text(text);z=run('reject-'+name,[compiler,'cgen',file,'--emit',out/(name+'.c')],success=False)
  assert b'[sem]' in z.stdout+z.stderr or b'[parse]' in z.stdout+z.stderr
  assert not (out/(name+'.c')).exists()
 native=case.with_suffix('.h').read_text();header=out/(case.stem+'.h')
 for name,bad in [('callback-order',native.replace('int (*callback)(int,void*)','int (*callback)(void*,int)',1)),('userdata-type',native.replace('void* user;} PcDesc','const void* user;} PcDesc',1))]:
  assert bad!=native;folder=out/name;target=folder/'test/cases/foreign_pack_callback.h';target.parent.mkdir(parents=True);target.write_text(bad)
  z=run('reject-header-'+name,[cc,'-std=c11','-I'+str(folder),'-I'+str(root),'-I'+str(sdk),'-x','c','-fsyntax-only',header],success=False)
  assert b'native descriptor field type' in z.stdout+z.stderr
 if a.ir_checker:
  z=run('sealed-ir',[ir,case],working=root);match=re.search(rb'Packed callback IR: .*? (\d+) mutations',z.stdout);assert match and int(match[1])>=349
  report['irMutations']=int(match[1])
 report.update(passed=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()));assert report['inputsUnchanged'];save()

if __name__=='__main__':main()
