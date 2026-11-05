"""Resident typed COM capsules over a verified parent source build, CPU only.

Only ForeignView and the new resident capsule TU are rebuilt. Other objects
retain their actual parent provenance; this is not a complete compiler build.
Windows executes fake COM, without OLE initialization or OS clipboard/drag.
Other platforms verify portable services and non-consuming unsupported wraps.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from foreign_validation_build import add_build_arguments,load_build
from compiler_response import command_with_response
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','out'):p.add_argument('--'+name,type=Path,required=True)
 add_build_arguments(p);a=p.parse_args();a.root=a.root.resolve();selected=load_build(a.build,a.overlay)
 source=selected['cohort'];base=selected['base'];sdk=Path(base.get('RuntimeDirectory') or base.get('SdkDirectory') or source/'release/win_x64')
 out=a.out.resolve();out.mkdir(exist_ok=False);cc=shutil.which('gcc');assert cc
 inputs=dict(selected['inputs']);names=['src/runtime/xl6_native_com.c','src/runtime/xl6_native_com.h','src/runtime/xnative_transfer.h',
  'src/runtime/xl6_foreign_view.c','src/runtime/xl6_foreign_view.h','test/cases/native_com_capsule.c',
  'test/cases/native_com_capsule_dll.h','test/cases/native_com_capsule_dll.c','test/cases/native_com_capsule_dll_host.c']
 inputs.update((str(a.root/name),sha(a.root/name))for name in names);inputs[str(Path(__file__).resolve())]=sha(__file__)
 runtime=selected['runtime']
 if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
 report=dict(complete=False,passed=False,fullCompilerRebuild=False,parentFullCompilerBuild=selected['fullSourceRebuild'],
  scope=__doc__,buildManifest=selected['buildManifest'],overlay=selected['overlay'],inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 def run(name,args):
  args=command_with_response(list(map(str,args)),cc,out/(name+'.rsp'));start=time.monotonic();result=subprocess.run(args,cwd=source,capture_output=True,timeout=180)
  log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
  report['rows'].append(dict(name=name,command=args,exitCode=result.returncode,seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save()
  print(name,result.returncode,flush=True);assert not result.returncode,log
 flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(a.root),'-I'+str(source),'-I'+str(sdk)]
 systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32','-luuid']if os.name=='nt'else['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
 try:
  if os.name=='nt':
   publisher=out/'capsule-publisher.dll'
   run('publisher-build',[cc,*flags,'-O2','-shared',a.root/'test/cases/native_com_capsule_dll.c','-o',publisher])
  for opt in ('O0','O2'):
   rebuilt=[]
   for name in ('xl6_foreign_view','xl6_native_com'):
    obj=out/(name+'-'+opt+'.o');run(name+'-'+opt,[cc,*flags,'-DXL6_NATIVE_COM_TEST','-'+opt,'-c',a.root/'src/runtime'/(name+'.c'),'-o',obj]);rebuilt.append(obj)
   objects=[obj for obj in selected['objects']if Path(obj).stem not in ('xl6_foreign_view','xl6_native_com')]
   target=out/('capsule-'+opt+('.exe'if os.name=='nt'else''))
   run(opt+'-build',[cc,*flags,'-'+opt,a.root/'test/cases/native_com_capsule.c',*rebuilt,*objects,runtime,*systems,'-o',target]);run(opt+'-run',[target])
   if os.name=='nt':
    production=out/('capsule-production-'+opt+'.o')
    run(opt+'-production-build',[cc,*flags,'-'+opt,'-c',a.root/'src/runtime/xl6_native_com.c','-o',production])
    if opt=='O2':
     nm=shutil.which('nm');assert nm
     run('production-symbols',[nm,production]);assert 'xl6NativeComTest' not in (out/'production-symbols.log').read_text(encoding='utf-8')
    target=out/('dll-host-'+opt+'.exe')
    run(opt+'-dll-build',[cc,*flags,'-'+opt,a.root/'test/cases/native_com_capsule_dll_host.c',rebuilt[0],production,*objects,runtime,*systems,'-o',target]);run(opt+'-dll-run',[target,publisher])
  header=out/'header.cpp';header.write_text('#include "src/runtime/xl6_native_com.h"\nstatic_assert(sizeof(xn_transfer_native_owner_v1)==48,"native owner");\nstatic_assert(sizeof(xn_transfer_services_v1)==64,"services");\nint main(){xn_transfer_services_v1 s={};return s.iSize!=0;}\n')
  cxx=shutil.which('g++');assert cxx
  for standard in ('c++11','c++17'):run(standard+'-header',[cxx,*[f for f in flags if not f.startswith('-std=')],'-std='+standard,'-fsyntax-only',header])
  report.update(complete=True,inputsUnchanged=all(sha(k)==v for k,v in inputs.items()));assert report['inputsUnchanged'];report['passed']=True
 finally:save()
if __name__=='__main__':main()
