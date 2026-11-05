"""Verify the opt-in resident COM component over an honest source successor.

Rebuild every actual consumer of the changed component-count ABI (API, plan,
trim and Cgen), the API registry, plus one new capsule TU. All other compiler
and resident objects retain their verified parent provenance. No complete
compiler rebuild, fresh VFS/provider or COM device run is claimed.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_validation_build import load_build
from compiler_response import command_with_response

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','parent-overlay','fixture','out'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--compiled-overlay',type=Path,help='Reuse the already linked six-object successor; rerun only independent wiring assertions')
 a=p.parse_args();root=a.root.resolve();out=a.out.resolve();out.mkdir(exist_ok=False)
 selected=load_build(overlay=a.parent_overlay);parent=json.loads(a.parent_overlay.read_text(encoding='utf-8-sig'));base=selected['base']
 reused=json.loads(a.compiled_overlay.read_text(encoding='utf-8-sig'))if a.compiled_overlay else None
 if reused:
  checked=load_build(overlay=a.compiled_overlay);assert checked['cohort']==root
  prior=json.loads((a.compiled_overlay.parent/'results.json').read_text(encoding='utf-8-sig'))
  for path,value in prior['inputs'].items():
   if Path(path).is_relative_to(root):assert sha(path)==value,path
 originalRoot=next(Path(r['Source']).parents[2]for r in base['Objects']if Path(r['Source']).name.startswith('xlir_public_source'))
 cc=shutil.which('gcc');nm=shutil.which('nm');assert cc and nm
 names=('src/api/xl_api.c','src/compile/xl6_compile_unit.c','src/compile/xl6_xrt_trim.c','src/cgen/xl6_ir_cgen.c','src/api/xl_api_symbols.c')
 mapping={r['ParentObject']:r for r in parent['Objects']};extra=[]
 files=[root/name for name in names];files.extend([root/'src/runtime/xl6_native_com.c',root/'src/runtime/xl6_native_com.h',root/'src/runtime/xnative_transfer.h',
  root/'src/compile/xl6_artifact_runtime.h',root/'src/compile/xl6_artifact_runtime_symbols.inc',root/'tools/compiler_dll.rsp.in',root/'tools/runtime_dll.rsp.in',
  root/'tools/build_xlir_public_source_driver.ps1',a.fixture.resolve(),Path(__file__).resolve()])
 inputs=dict(selected['inputs']);inputs.update((str(f),sha(f))for f in files)
 report=dict(complete=False,passed=False,scope=__doc__,fullCompilerRebuild=False,parentOverlay=str(a.parent_overlay),inputs=inputs,rows=[])
 if reused:report['reusedCompiledOverlay']=dict(path=str(a.compiled_overlay),sha256=sha(a.compiled_overlay),qualification='Six existing actual objects reused unchanged; zero new compilations in this verification run')
 env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(base.get('NativeRoot')or root))
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 def run(name,command,stdin=None):
  command=list(map(str,command));actual=command_with_response(command,cc,out/(name+'.rsp'))if command[0]==cc else command
  started=time.monotonic();result=subprocess.run(actual,cwd=root,env=env,capture_output=True,timeout=300,input=stdin)
  log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
  report['rows'].append(dict(name=name,command=actual,exitCode=result.returncode,logSha256=sha(log),seconds=round(time.monotonic()-started,3)));save()
  print(name,result.returncode,flush=True);assert not result.returncode,log
  return result.stdout.decode('utf-8',errors='strict')
 def flagsFor(item):
  flags=item.get('Flags')
  if flags is None:
   command=next(r['Command']for r in base['Rows']if '-c'in r['Command']and item['Source']in r['Command']);flags=command[1:command.index('-c')]
  return [s.replace(str(originalRoot),str(root)).replace(originalRoot.as_posix(),root.as_posix())for s in flags]
 try:
  for name in names:
   if reused:break
   item=next(r for r in base['Objects']if Path(r['Source']).relative_to(originalRoot).as_posix()==name)
   flags=flagsFor(item);obj=out/(Path(name).stem+'.o');run(Path(name).stem+'-compile',[cc,*flags,'-c',root/name,'-o',obj])
   mapping[item['Path']]=dict(Source=str(root/name),Path=str(obj),ParentObject=item['Path'],ParentSource=item['Source'],Flags=flags,Sha256=sha(obj))
  template=next(r for r in base['Objects']if Path(r['Source']).stem=='xl6_foreign_view');flags=flagsFor(template);obj=out/'xl6_native_com.o'
  if reused:
   mapping={r['ParentObject']:r for r in reused['Objects']};extra=reused['ExtraObjects'];assert len(extra)==1
   obj=Path(extra[0]['Path'])
  else:
   run('capsule-compile',[cc,*flags,'-c',root/'src/runtime/xl6_native_com.c','-o',obj])
   extra.append(dict(Source=str(root/'src/runtime/xl6_native_com.c'),Path=str(obj),Flags=flags,Sha256=sha(obj)))
  if 'LinkResponse'in base:args=[s.strip('"')for s in Path(base['LinkResponse']).read_text(encoding='utf-8-sig').splitlines()]
  else:args=next(r['Command'][1:]for r in base['Rows']if r['Name']=='link')
  substituted=0
  for i,arg in enumerate(args):
   key=str(Path(arg))
   if key in mapping:args[i]=mapping[key]['Path'];substituted+=1
  assert substituted==len(mapping);driver=out/('public_source.exe'if os.name=='nt'else'public_source');args[args.index('-o')+1]=str(driver)
  args.insert(0,str(obj))
  if reused:driver=Path(reused['Driver'])
  else:run('driver-link',[cc,*args])
  runtime=Path(reused['RuntimeLibrary'])if reused else selected['runtime'];tcc=Path((reused or parent)['TccCore'])
  if os.name=='nt':
   for file in (runtime,tcc):shutil.copy2(file,out/file.name)
   runtime=out/runtime.name;tcc=out/tcc.name
  overlay=dict(complete=True,passed=True,fullRebuild=False,scope=__doc__,sourceRoot=str(root),parentBuild=parent['parentBuild'],parentBuildSha256=parent['parentBuildSha256'],
   predecessorOverlay=str(a.parent_overlay),predecessorOverlaySha256=sha(a.parent_overlay),Driver=str(driver),DriverSha256=sha(driver),RuntimeLibrary=str(runtime),TccCore=str(tcc),
   Objects=list(mapping.values()),ExtraObjects=extra,newlyCompiledExistingObjects=5,newlyCompiledExtraObjects=1,reusedExistingObjects=len(base['Objects'])-5)
  if reused:overlay['compilationReuseManifest']=str(a.compiled_overlay);overlay['compilationReuseManifestSha256']=sha(a.compiled_overlay)
  for key in ('SdkDirectory','RuntimeHeaders'):
   if key in parent:overlay[key]=parent[key]
  manifest=out/'overlay.json';manifest.write_text(json.dumps(overlay,indent=2)+'\n')
  current=load_build(overlay=manifest);objects=current['objects'];sdk=Path(base.get('RuntimeDirectory')or base.get('SdkDirectory')or originalRoot/'release/win_x64')
  flags=['-std=c11','-Wall','-Wextra','-Werror','-O2','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(sdk)]
  systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']if os.name=='nt'else['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
  oracle=out/('wiring.exe'if os.name=='nt'else'wiring');run('oracle-build',[cc,*flags,a.fixture,*objects,runtime,*systems,'-o',oracle])
  text=run('oracle',[oracle,root/'test/cases/cgen_basic.xl']);closure=[dict(id=int(v[1]),source=v[2])for v in (s.split('\t')for s in text.splitlines())if v[0]=='component']
  report['selectedClosure']=closure;assert any(r['source']=='src/runtime/xl6_native_com.c'for r in closure)
  # Audit actual selected object definitions, not a fresh 1375-provider rebuild.
  response=out/'nm-inputs.rsp';response.write_text('\n'.join('"'+str(v).replace('\\','/')+'"'for v in objects)+'\n')
  definitions=run('actual-objects-defined',[nm,'-A','-g','--defined-only','@'+str(response)])
  for symbol in ('xl6TransferServicesFromLeaseV1','xl6TransferServicesProcessResidentV1'):
   rows=[line for line in definitions.splitlines()if re.search(r'\s[Tt]\s+'+symbol+r'$',line)]
   assert len(rows)==1 and 'xl6_native_com.o:'in rows[0],rows
  undefined=run('capsule-undefined',[nm,'-u',obj]);required=re.findall(r'\bU\s+(xl\w+)\s*$',undefined,re.M)
  report['capsuleResidentImports']=required
  sourceObjects={Path(r['Source']).relative_to(originalRoot).as_posix():mapping.get(r['Path'],r)['Path']for r in base['Objects']}
  sourceObjects['src/runtime/xl6_native_com.c']=str(obj)
  # The actual full host combines ForeignError into its XRT implementation TU.
  # Discover an exact included-source provider rather than pretending it had
  # an independent object or weakening the resident closure assertion.
  included=[]
  for row in closure:
   if row['source']in sourceObjects:continue
   providers=[]
   for original in base['Objects']:
    relative=Path(original['Source']).relative_to(originalRoot);path=root/relative
    if path.is_file() and re.search(r'^\s*#include\s+"'+re.escape(row['source'])+r'"',path.read_text(encoding='utf-8-sig',errors='replace'),re.M):
     assert sha(path)==sha(original['Source'])
     assert sha(root/row['source'])==sha(originalRoot/row['source'])
     providers.append(mapping.get(original['Path'],original)['Path'])
     inputs[str(path)]=sha(path);inputs[str(root/row['source'])]=sha(root/row['source'])
   assert len(providers)==1,(row,providers)
   sourceObjects[row['source']]=providers[0];included.append(dict(source=row['source'],physicalProvider=providers[0]))
  report['includedSourceProviders']=included
  closedObjects={str(Path(sourceObjects[r['source']])).replace('\\','/')for r in closure if r['source']in sourceObjects}
  assert len(closedObjects)==len(closure),(closedObjects,closure)
  for symbol in required:
   rows=[line for line in definitions.splitlines()if re.search(r'\s[A-Za-z]\s+'+symbol+r'$',line)]
   assert any(any(line.replace('\\','/').startswith(path+':')for path in closedObjects)for line in rows),(symbol,rows)
  # Materialize precisely the selected resident closure, using the matched
  # host for XRT rather than pulling the full compiler/XRT implementation TU.
  # Only its embedded ForeignError provider needs a fresh separate object.
  error=out/'artifact-foreign-error.o'
  if os.name=='nt':
   run('artifact-error-compile',[cc,*flags,'-c',root/'src/api/xl_foreign_error.c','-o',error])
   report['artifactNativeProvider']=dict(kind='Matched resident host XRT DLL',path=str(runtime),sha256=sha(runtime))
  else:
   # Linux's parent RuntimeLibrary is intentionally Only-TCC, without XRT.
   # Its already verified full native TU supplies the one actual XRT body and
   # embedded ForeignError. Reuse it unchanged; do not invent a DLL export or
   # claim this separate connection check proves native module size trimming.
   error=Path(next(row['physicalProvider']for row in included if row['source']=='src/api/xl_foreign_error.c'))
   report['artifactNativeProvider']=dict(kind='Unchanged full native XRT parent object, including ForeignError; no second XRT or new compilation',path=str(error),sha256=sha(error))
  artifactSource=out/'artifact.c';artifactSource.write_text('#include "src/runtime/xl6_native_com.h"\n#include "src/common/xl6_xrt.h"\n#include <assert.h>\nint main(void){xn_transfer_services_v1 s={0};assert(!xl6TransferServicesFromLeaseV1(NULL,NULL,&s));xrtClearError();assert(xl6TransferServicesProcessResidentV1(NULL,&s));assert(s.iSize==sizeof(s)&&s.wrapDataObject&&s.wrapDropSource&&s.wrapDropTarget);return 0;}\n')
  artifactObjects=[error if row['source']=='src/api/xl_foreign_error.c'else sourceObjects[row['source']]for row in closure]
  artifact=out/('artifact.exe'if os.name=='nt'else'artifact')
  run('artifact-closure-link',[cc,*flags,artifactSource,*artifactObjects,runtime,*systems,'-o',artifact]);run('artifact-closure-run',[artifact])
  report['artifactResidentSources']=[row['source']for row in closure]
  report['artifactContainsCompilerObjects']=False
  report['sourceListOccurrences']={}
  for name in ('tools/compiler_dll.rsp.in','tools/runtime_dll.rsp.in','tools/build_xlir_public_source_driver.ps1'):
   body=(root/name).read_text(encoding='utf-8-sig');count=body.count('src/runtime/xl6_native_com.c');assert count==1;report['sourceListOccurrences'][name]=count
  run('non-com-cgen',[driver,root/'test/cases/cgen_basic.xl','cgen-default',out/'non-com.c'])
  assert 'xl6TransferServices'not in (out/'non-com.c').read_text(encoding='utf-8-sig')
  report['inputsUnchanged']=all(sha(k)==v for k,v in inputs.items());assert report['inputsUnchanged'];report.update(complete=True,passed=True,compilerOverlay=str(manifest),compilerOverlaySha256=sha(manifest))
 finally:save()
if __name__=='__main__':main()
