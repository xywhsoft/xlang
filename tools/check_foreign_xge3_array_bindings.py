"""Compile-only XUI typed Array bulk declarations against a matched staged SDK.

Exercises four original Native functions. Does not execute the resulting GUI
module, bootstrap, engine, window, GPU or device code. Compiler qualification
is kept separate from the staged Native SDK and package input qualification.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from foreign_validation_build import add_build_arguments,load_build
from compiler_response import command_with_response

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','packages','engine-build','out'):p.add_argument('--'+name,type=Path,required=True)
 add_build_arguments(p);a=p.parse_args();root,packages,engine,out=[x.resolve() for x in (a.root,a.packages,a.engine_build,a.out)];b=load_build(a.build,a.overlay)
 sha=lambda x:hashlib.sha256(Path(x).read_bytes()).hexdigest();source=b['cohort'];driver=b['driver'];basePath=b['basePath'];base=b['base']
 fixture=root/'test/cases/foreign_xge3_binding_arrays.xl';native=engine/'build-report.json';assert native.is_file()
 inputs=dict(b['inputs']);inputs.update((str(x),sha(x)) for x in (fixture,native,Path(__file__)))
 inputs.update((str(x),sha(x)) for x in packages.rglob('*') if x.is_file());out.mkdir(parents=True,exist_ok=False)
 report=dict(scope=__doc__,complete=False,passed=False,executed=False,shippingDelivery=False,fullSourceRebuild=b['fullSourceRebuild'],compilerManifest=b['buildManifest'] or b['overlay'],nativeBuild=str(native),packages=str(packages),inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 env=dict(os.environ,XLANG_PACKAGE_PATH=str(packages),XL6_SOURCE_NATIVE_ROOT=str(source))
 def run(name,command):
  command=list(map(str,command));actual=command_with_response(command,'gcc',out/(name+'.rsp')) if command[0] in ('gcc','g++') else command
  start=time.monotonic();z=subprocess.run(actual,cwd=source,env=env,capture_output=True,timeout=240);log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
  report['rows'].append(dict(name=name,exitCode=z.returncode,logSha256=sha(log),seconds=round(time.monotonic()-start,3)));save();print(name,z.returncode,flush=True);assert not z.returncode,log
 generated=out/'generated.c';run('cgen',[driver,fixture,'cgen-default',generated]);run('ir',[driver,fixture,'ir-lowered',out/'lowered.json']);run('header',[driver,fixture,'emit',out/'public'])
 code=generated.read_text(encoding='utf-8-sig');symbols=('xuiTabsSetItems','xuiTabsSetIcons','xuiToolbarSetItems','xuiMsgBoxSetCustomButtons')
 for name in symbols:assert name+'(' in code
 assert 'native_array_record_' in code and 'xl6SequenceReadPinBeginV1' in code and 'native_array_views_' in code
 # Exact compiler private headers precede any unrelated live source headers.
 sdk=Path(base.get('SdkDirectory') or base.get('RuntimeDirectory'));local=out/'sdk/__xl_runtime';local.mkdir(parents=True);shutil.copy2(source/'src/runtime/xl6_sequence.h',local/'xl6_sequence.h')
 includes=['-I'+str(x) for x in (packages/'_xge_native/include',out/'sdk',source/'src/runtime',source,basePath.parent/'include',sdk,sdk/'include')]
 common=['-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes]
 run('header-C',['gcc','-std=c11',*common,'-x','c','-fsyntax-only',out/'public.h'])
 for std in ('c++11','c++17'):run('header-'+std,['g++','-std='+std,*common,'-x','c++','-fsyntax-only',out/'public.h'])
 warning=['-Wno-'+name for name in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')]
 for opt in ('O0','O2'):run(opt+'-compile',['gcc','-std=c11',*common,*warning,'-'+opt,'-c',generated,'-o',out/(opt+'.o')])
 report.update(complete=True,passed=True,originalNativeFunctions=list(symbols),inputsUnchanged=all(sha(path)==value for path,value in inputs.items()));assert report['inputsUnchanged'];save()

if __name__=='__main__':main()
