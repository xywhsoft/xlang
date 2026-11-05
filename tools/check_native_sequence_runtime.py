"""Owning readonly native sequence CPU primitives using a verified source build.

An explicit development overlay is also accepted and identified as partial.
This gate does not itself qualify a VFS/provider or shipping delivery.
"""
from pathlib import Path
import argparse,json,hashlib,os,shutil,subprocess,time
from foreign_validation_build import add_build_arguments,load_build
from compiler_response import command_with_response
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','out'):p.add_argument('--'+name,type=Path,required=True)
 add_build_arguments(p);a=p.parse_args();selected=load_build(a.build,a.overlay)
 source=selected['cohort'];base=selected['base'];sdk=Path(base.get('RuntimeDirectory') or base.get('SdkDirectory') or source/'release/win_x64')
 out=a.out.resolve();out.mkdir(exist_ok=False)
 objects=selected['objects'];pins={Path(__file__),a.root/'test/cases/native_sequence_runtime.c',a.root/'src/runtime/xl6_native_sequence.c',a.root/'src/runtime/xl6_native_sequence.h',a.root/'src/runtime/xl6_foreign_view.c',a.root/'src/runtime/xl6_foreign_view.h',a.root/'test/cases/xlir_ownership_finalization_view_runtime.c'}
 cc=shutil.which('gcc');assert cc;pins.add(Path(cc));runtime=selected['runtime'];pins.add(runtime)
 if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
 pins.update((source/'src/runtime').glob('xl6_ownership*.h'));pins.update([source/'src/runtime/xl6_foreign_view.h',sdk/'include/xrt_decl.h'])
 inputs=dict(selected['inputs']);inputs.update((str(p),sha(p)) for p in pins)
 report=dict(complete=False,passed=False,fullCompilerRebuild=selected['fullSourceRebuild'],buildManifest=selected['buildManifest'],overlay=selected['overlay'],scope=__doc__,inputs=inputs,rows=[],shippingDelivery=False)
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(a.root),'-I'+str(source),'-I'+str(sdk)]
 systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
 def run(name,command):
  command=command_with_response(list(map(str,command)),cc,out/(name+'.rsp'));start=time.monotonic();r=subprocess.run(command,cwd=source,capture_output=True,timeout=180);log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
  report['rows'].append(dict(name=name,command=command,exitCode=r.returncode,seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save();print(name,r.returncode,flush=True);assert not r.returncode,log
 try:
  for opt in ('O0','O2'):
   rebuilt=[]
   for name in ('xl6_native_sequence','xl6_foreign_view'):
    obj=out/(name+'-'+opt+'.o');run(name+'-'+opt,[cc,*flags,'-I'+str(a.root),'-I'+str(a.root/'release/win_x64'),'-'+opt,'-c',a.root/'src/runtime'/(name+'.c'),'-o',obj]);rebuilt.append(obj)
   current=[v for v in objects if Path(v).stem not in ('xl6_foreign_view','xl6_native_sequence')]
   target=out/('sequence-'+opt+('.exe' if os.name=='nt' else ''));args=[cc,*flags,'-I'+str(a.root),'-I'+str(a.root/'release/win_x64'),'-'+opt,a.root/'test/cases/native_sequence_runtime.c',*rebuilt,*current,runtime,*systems,'-o',target]
   run(opt+'-build',args);run(opt+'-run',[target])
   scoped=out/('no-scope-'+opt+('.exe' if os.name=='nt' else ''))
   args=[cc,*flags,'-I'+str(a.root),'-'+opt,a.root/'test/cases/xlir_ownership_finalization_view_runtime.c',*rebuilt,*current,runtime,*systems,'-o',scoped]
   run(opt+'-no-scope-build',args);run(opt+'-no-scope-run',[scoped])
  header=out/'header.cpp';header.write_text('#include "src/runtime/xl6_native_sequence.h"\nstatic_assert(sizeof(XL6_NativeSequenceSchemaV1)==56,"schema ABI");\nint main(){ XL6_NativeSequenceResultV1 p={}; XL6_NativeSequenceReadV1 s={};return p.self!=nullptr || s.self!=nullptr;}\n')
  cxx=shutil.which('g++');assert cxx
  for standard in ('c++11','c++17'):run(standard+'-header',[cxx,*[v for v in flags if not v.startswith('-std=')],'-std='+standard,'-fsyntax-only',header])
  report['inputsUnchanged']=all(sha(p)==h for p,h in inputs.items());assert report['inputsUnchanged'];report.update(complete=True,passed=True)
 finally:save()
if __name__=='__main__':main()
