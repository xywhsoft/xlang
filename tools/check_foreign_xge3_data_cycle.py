"""Real Context -> Data -> provider capture cycle and dynamic module lifetime.

Uses two actual TinyCC modules and only a weak Native Context lifetime watch.
No OS drag, clipboard, window, rendering, browser or network operation runs.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from foreign_validation_build import add_build_arguments,load_build
from compiler_response import command_with_response
from check_foreign_xge3_data_object import verify_stage

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','packages','out','tcc-core','tcc-libraries','tcc-include','engine-build','stage-report'):p.add_argument('--'+name,type=Path,required=True)
 add_build_arguments(p);a=p.parse_args();root,packages,out,tcc,libraries,tccInclude=[v.resolve() for v in (a.root,a.packages,a.out,a.tcc_core,a.tcc_libraries,a.tcc_include)];b=load_build(a.build,a.overlay);cohort,runtime=b['cohort'],b['runtime'];windows=os.name=='nt';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();private=packages/'_xge_native';engine=private/'native'/('xge.dll' if windows else 'libxge.so');assert engine.is_file() and tcc.is_file() and libraries.is_dir() and tccInclude.is_dir()
 build,binary=verify_stage(packages,a.engine_build.resolve(),a.stage_report.resolve());assert sha(binary)==sha(engine)
 fixture=root/'test/cases/foreign_xge3_data_object_cycle.xl';header=fixture.with_suffix('.h');host=root/'test/cases/foreign_xge3_data_object_cycle_host.c';inputs=dict(b['inputs']);inputs.update((str(f),sha(f)) for f in (Path(__file__),Path(__file__).with_name('check_foreign_xge3_data_object.py'),a.stage_report,a.engine_build/'build-report.json',fixture,header,host,tcc,engine,*[x for x in packages.rglob('*') if x.is_file()],*[x for x in libraries.rglob('*') if x.is_file()],*[x for x in tccInclude.rglob('*') if x.is_file()]));out.mkdir(parents=True,exist_ok=False)
 env=dict(os.environ,XLANG_PACKAGE_PATH=str(packages),XL6_SOURCE_NATIVE_ROOT=str(cohort))
 if windows:shutil.copy2(runtime,out/runtime.name)
 else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+str(engine.parent)+':'+env.get('LD_LIBRARY_PATH','')
 sdk=Path(b['base'].get('SdkDirectory') or b['base'].get('RuntimeDirectory') or runtime.parent);flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*['-I'+str(x) for x in (tccInclude,cohort,root,root/'test/cases',sdk,sdk/'include',private/'include')]];flags+=['-DXGE_DLL','-DXUI_DLL'] if windows else [];systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
 report=dict(scope=__doc__,complete=False,passed=False,compiler=b['buildManifest'] or b['overlay'],fullSourceRebuild=b['fullSourceRebuild'],shippingDelivery=False,osUiNetworkGpuExecuted=False,inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 def run(name,command):
  command=list(map(str,command));actual=command_with_response(command,'gcc',out/(name+'.rsp'));start=time.monotonic();r=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=300);log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr);report['rows'].append(dict(name=name,exitCode=r.returncode,seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save();print(name,r.returncode,flush=True);assert not r.returncode,log
 for opt in ('O0','O2'):
  exe=out/('host-'+opt+('.exe' if windows else '.out'));run(opt+'-build',['gcc',*flags,'-'+opt,host,*b['objects'],runtime,*systems,'-o',exe]);run(opt+'-run',[exe,fixture,cohort,tccInclude,tcc,libraries,out/('jit-'+opt),sdk/'include',engine,root])
 report.update(complete=True,passed=True,inputsUnchanged=all(sha(f)==h for f,h in inputs.items()));assert report['inputsUnchanged'];save()
if __name__=='__main__':main()
