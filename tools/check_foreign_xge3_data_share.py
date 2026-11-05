"""Actual shared lazy-provider behavior after closing its registering view.

Reuses a qualified CPU cohort's exact GCC argv; one focused language fixture
is generated once. Exit 33 is an explicit pre-fix observation, not acceptance
of the desired behavior. No OS transfer operation executes.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from compiler_response import command_with_response
from foreign_validation_build import add_build_arguments,load_build
from check_foreign_xge3_data_object import verify_stage
from foreign_native_artifacts import native_artifacts

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','base','packages','engine-build','stage-report','out'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--expected',type=int,choices=(0,33),required=True);add_build_arguments(p);a=p.parse_args();root,base,packages,engine,stage,out=[v.resolve() for v in (a.root,a.base,a.packages,a.engine_build,a.stage_report,a.out)];sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest();b=load_build(a.build,a.overlay);prior=json.loads((base/'results.json').read_text());assert prior['complete'] and prior['passed'] and prior['inputsUnchanged'];assert all(sha(f)==v for f,v in prior['inputs'].items());verify_stage(packages,engine,stage);fixture=root/'test/cases/foreign_xge3_data_shared_provider.xl';inputs=dict(prior['inputs']);inputs.update((str(f),sha(f)) for f in (Path(__file__),base/'results.json',fixture,stage,*[x for x in packages.rglob('*') if x.is_file()]));out.mkdir(parents=True,exist_ok=False)
 runtime=b['runtime'];windows=os.name=='nt';oldPackages=Path(next(x for x in prior['inputs'] if x.endswith(('/_xge_native/src/runtime.c','\\_xge_native\\src\\runtime.c')))).parents[2];artifacts=native_artifacts(packages,engine);oldEngine=Path(next(x for x in prior['inputs'] if Path(x).name=='build-report.json')).parent;oldArtifacts=native_artifacts(oldPackages,oldEngine);inputs[str(Path(__file__).with_name('foreign_native_artifacts.py'))]=sha(Path(__file__).with_name('foreign_native_artifacts.py'))
 if windows:shutil.copy2(runtime,out/runtime.name);shutil.copy2(artifacts['stagedImage'],out/artifacts['loadName'])
 env=dict(os.environ,XLANG_PACKAGE_PATH=str(packages),XL6_SOURCE_NATIVE_ROOT=str(b['cohort']))
 if not windows:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+str(engine)+':'+env.get('LD_LIBRARY_PATH','')
 report=dict(scope=__doc__,complete=False,passed=False,desiredBehaviorAccepted=a.expected==0,expectedExit=a.expected,shippingDelivery=False,osUiNetworkGpuExecuted=False,compiler=b['overlay'] or b['buildManifest'],fullSourceRebuild=b['fullSourceRebuild'],inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 def run(name,cmd,expected=0):
  cmd=list(map(str,cmd));actual=command_with_response(cmd,'gcc',out/(name+'.rsp')) if cmd[0]=='gcc' else cmd;start=time.monotonic();r=subprocess.run(actual,cwd=b['cohort'],env=env,capture_output=True,timeout=300);log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr);report['rows'].append(dict(name=name,exitCode=r.returncode,expectedExit=expected,seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save();print(name,r.returncode,flush=True);assert r.returncode==expected,log
 for action,ext in (('cgen-default','.c'),('ir-lowered','.json')):run(action,[b['driver'],fixture,action,out/('actual'+ext)])
 for opt in ('O0','O2'):
  receipt=base/('example-'+opt+'-build.rsp.json');inputs[str(receipt)]=sha(receipt);record=json.loads(receipt.read_text());oldExe=base/('example-'+opt+('.exe' if windows else '.out'));exe=out/(opt+('.exe' if windows else '.out'));args=[str(artifacts['library']) if v==str(oldArtifacts['library']) else v.replace(str(oldPackages),str(packages)) for v in record['Arguments']];assert args.count(str(base/'example.c'))==1 and args.count(str(oldExe))==1;args=[str(out/'actual.c') if v==str(base/'example.c') else str(exe) if v==str(oldExe) else v for v in args];run(opt+'-build',['gcc',*args]);run(opt+'-run',[exe],a.expected)
 report.update(complete=True,passed=True,inputsUnchanged=all(sha(f)==v for f,v in inputs.items()));assert report['inputsUnchanged'];save()
if __name__=='__main__':main()
