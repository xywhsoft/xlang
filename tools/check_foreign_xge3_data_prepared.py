"""Continue DataObject CPU checks from already generated, pinned client C.

No frontend matrix is rerun. The previous report and every recorded source,
compiler object, SDK file and helper must be unchanged. Client C is pinned at
this continuation boundary, with its successful Cgen row retained as evidence.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from compiler_response import command_with_response
from foreign_validation_build import add_build_arguments,load_build
from check_foreign_xge3_data_object import verify_stage

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','packages','engine-build','stage-report','prepared','out'):p.add_argument('--'+name,type=Path,required=True)
 add_build_arguments(p);a=p.parse_args();root,packages,engine,stage,prepared,out=[v.resolve() for v in (a.root,a.packages,a.engine_build,a.stage_report,a.prepared,a.out)];sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest();b=load_build(a.build,a.overlay);cohort,runtime=b['cohort'],b['runtime'];private=packages/'_xge_native';windows=os.name=='nt';_,binary=verify_stage(packages,engine,stage)
 previous=prepared/'results.json';prior=json.loads(previous.read_text());assert prior['mode']=='cpu' and prior['matchedSdk'] and prior['nativeFingerprintZero']==False
 assert all(sha(f)==value for f,value in prior['inputs'].items()),'prepared inputs changed'
 assert all(prior['inputs'].get(f)==value for f,value in b['inputs'].items()),'compiler cohort changed'
 rows={v['name']:v for v in prior['rows']};scripts=('actual','drag','ui-drag','example')
 for name in scripts:assert rows[name+'-cgen-default']['exitCode']==0 and (prepared/(name+'.c')).is_file()
 out.mkdir(parents=True,exist_ok=False);inputs=dict(prior['inputs']);inputs.update((str(f),sha(f)) for f in (Path(__file__),previous,stage,binary));generated=[]
 for name in scripts:
  source=prepared/(name+'.c');inputs[str(source)]=sha(source);shutil.copy2(source,out/source.name);generated.append(dict(script=name,path=str(source),sha256=sha(source),cgenRow=rows[name+'-cgen-default']))
 sdk=out/'sdk/__xl_runtime';sdk.mkdir(parents=True)
 for f in (cohort/'src/runtime').glob('*.h'):shutil.copy2(f,sdk/f.name)
 base=Path(b['base'].get('SdkDirectory') or b['base'].get('RuntimeDirectory') or runtime.parent);includes=(private/'include',out/'sdk',cohort,root,b['basePath'].parent/'include',base,base/'include');flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-DXGE_XRT_HOST=1',*['-I'+str(f) for f in includes]]
 if windows:flags+=['-DXGE_DLL','-DXUI_DLL'];shutil.copy2(runtime,out/runtime.name);shutil.copy2(binary,out/binary.name)
 warnings=['-Wno-'+x for x in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')];systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent),'-Wl,-rpath,'+str(engine)];library=engine/('xge.lib' if windows else 'libxge.so');inputs[str(library)]=sha(library);env=dict(os.environ)
 if not windows:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+str(engine)+':'+env.get('LD_LIBRARY_PATH','')
 host=root/'test/cases/foreign_decl_module_host.c';report=dict(scope=__doc__,complete=False,passed=False,shippingDelivery=False,compiler=b['buildManifest'] or b['overlay'],fullSourceRebuild=b['fullSourceRebuild'],frontendRerun=False,frontendEvidence=str(previous),generatedClientC=generated,matchedNativeSdk=True,coldTransferRuntimeLinked=False,osClipboardDragUiNetworkGpuExecuted=False,inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 def run(name,command):
  command=list(map(str,command));actual=command_with_response(command,'gcc',out/(name+'.rsp')) if command[0]=='gcc' else command;start=time.monotonic();r=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=300);log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr);report['rows'].append(dict(name=name,exitCode=r.returncode,seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save();print(name,r.returncode,flush=True);assert not r.returncode,log
 for opt in ('O0','O2'):
  for name in scripts:
   extra=[root/'test/cases/foreign_xge3_checked_drag_native.c'] if name=='drag' else [root/'test/cases/foreign_xge3_ui_drag_native.c'] if name=='ui-drag' else []
   exe=out/(name+'-'+opt+('.exe' if windows else '.out'));run(name+'-'+opt+'-build',['gcc',*flags,*warnings,'-'+opt,out/(name+'.c'),*extra,private/'src/runtime.c',host,*b['objects'],runtime,library,*systems,'-o',exe]);run(name+'-'+opt+'-run',[exe])
 report.update(complete=True,passed=True,inputsUnchanged=all(sha(f)==h for f,h in inputs.items()));assert report['inputsUnchanged'];save()
if __name__=='__main__':main()
