"""Run actual public Shape NativeSequence projections using a CPU-only backend.

This source-build gate executes generated C at O0/O2. It pins a matching
Native SDK separately from the compiler/runtime object cohort. No window,
device, GPU, browser, shipping provider or dynamic-backend unload is tested.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from compiler_response import command_with_response
from foreign_validation_build import add_build_arguments,load_build


def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','packages','engine-build','out'):p.add_argument('--'+name,type=Path,required=True)
 add_build_arguments(p);a=p.parse_args();root,packages,engine,out=[x.resolve() for x in (a.root,a.packages,a.engine_build,a.out)]
 b=load_build(a.build,a.overlay);source=b['cohort'];driver=b['driver'];runtime=b['runtime'];base=b['base'];private=packages/'_xge_native'
 sha=lambda x:hashlib.sha256(Path(x).read_bytes()).hexdigest();windows=os.name=='nt'
 native=engine/'build-report.json';metadata=json.loads(native.read_text(encoding='utf-8-sig'))
 assert metadata['linked'] and metadata['xrt_mode']=='host'
 binary=engine/('xge.dll' if windows else 'libxge.so');library=engine/('xge.lib' if windows else 'libxge.so')
 stem=root/'test/cases/foreign_xge3_package_native_sequence'
 fixture=stem.with_suffix('.xl');header=stem.with_suffix('.h');sidecar=stem.with_name(stem.name+'_native.c');host=stem.with_name(stem.name+'_host.c')
 cc=shutil.which('gcc');cxx=shutil.which('g++');assert cc and cxx
 inputs=dict(b['inputs']);inputs.update((str(x),sha(x)) for x in (Path(__file__),Path(cc),Path(cxx),fixture,header,sidecar,host,native,binary,library,engine/'xge_build_config.h'))
 inputs.update((str(x),sha(x)) for x in packages.rglob('*') if x.is_file())
 runtime_headers=list((source/'src/runtime').glob('*.h'));assert runtime_headers
 inputs.update((str(x),sha(x)) for x in runtime_headers)
 out.mkdir(parents=True,exist_ok=False);(out/'tmp').mkdir();local=out/'sdk/__xl_runtime';local.mkdir(parents=True)
 snapshots=out/'inputs/test/cases';snapshots.mkdir(parents=True)
 for x in (fixture,header,sidecar,host):shutil.copy2(x,snapshots/x.name)
 shutil.copy2(__file__,out/'inputs'/Path(__file__).name)
 for x in runtime_headers:shutil.copy2(x,local/x.name)
 env=dict(os.environ,XLANG_PACKAGE_PATH=str(packages),XL6_SOURCE_NATIVE_ROOT=str(source),TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
 if windows:
  shutil.copy2(runtime,out/runtime.name);shutil.copy2(binary,out/binary.name)
 else:env['LD_LIBRARY_PATH']=str(engine)+':'+str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
 report=dict(scope=__doc__,complete=False,passed=False,executed=True,shippingDelivery=False,tccIncluded=False,
  fullSourceRebuild=b['fullSourceRebuild'],compilerManifest=b['buildManifest'] or b['overlay'],nativeBuild=str(native),packages=str(packages),inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 def run(name,command):
  command=list(map(str,command));actual=command_with_response(command,cc,out/(name+'.rsp')) if command[0] in (cc,cxx) else command
  start=time.monotonic();z=subprocess.run(actual,cwd=source,env=env,capture_output=True,timeout=300);log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
  report['rows'].append(dict(name=name,command=actual,exitCode=z.returncode,logSha256=sha(log),seconds=round(time.monotonic()-start,3)));save();print(name,z.returncode,flush=True);assert not z.returncode,log
 generated=out/'generated.c';run('cgen',[driver,fixture,'cgen-default',generated]);run('ir',[driver,fixture,'ir-lowered',out/'lowered.json']);run('header',[driver,fixture,'emit',out/'public'])
 emitted=generated.read_text(encoding='utf-8-sig')
 for symbol in ('xuiTextShapeGetClusters','xuiTextShapeGetCarets','xuiTextShapeAddRef','xuiTextShapeCreateOwned'):
  assert symbol+'(' in emitted
 # Element access and iteration must not use the indexed Native query APIs.
 assert 'xuiTextShapeGetCluster(' not in emitted and 'xuiTextShapeGetCaret(' not in emitted
 sdk=Path(base.get('SdkDirectory') or base.get('RuntimeDirectory') or runtime.parent)
 includes=['-I'+str(x) for x in (private/'include',out/'sdk',source,root,b['basePath'].parent/'include',sdk,sdk/'include')]
 flags=['-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-DXGE_XRT_HOST=1',*includes]
 if windows:flags+=['-DXGE_DLL','-DXUI_DLL']
 run('header-C',[cc,'-std=c11',*flags,'-x','c','-fsyntax-only',out/'public.h'])
 for std in ('c++11','c++17'):run('header-'+std,[cxx,'-std='+std,*flags,'-x','c++','-fsyntax-only',out/'public.h'])
 warning=['-Wno-'+x for x in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')]
 systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent),'-Wl,-rpath,'+str(engine)]
 for opt in ('O0','O2'):
  executable=out/(opt+('.exe' if windows else '.out'))
  run(opt+'-build',[cc,'-std=c11',*flags,*warning,'-'+opt,generated,sidecar,private/'src/runtime.c',host,*b['objects'],runtime,library,*systems,'-o',executable])
  run(opt+'-run',[executable])
 report.update(complete=True,passed=True,inputsUnchanged=all(sha(path)==digest for path,digest in inputs.items()))
 assert report['inputsUnchanged'];save()


if __name__=='__main__':main()
