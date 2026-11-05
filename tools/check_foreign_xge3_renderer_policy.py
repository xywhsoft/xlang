"""Real Native Renderer pure-DEFERRED policy gate; CPU paint only."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from foreign_validation_build import add_build_arguments,load_build
from compiler_response import command_with_response
from foreign_native_artifacts import native_artifacts
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','native-root','packages','engine-build','out'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--negative-private-runtime',type=Path);add_build_arguments(p);a=p.parse_args();root,native,packages,engine,out=[x.resolve() for x in (a.root,a.native_root,a.packages,a.engine_build,a.out)];b=load_build(a.build,a.overlay);source=b['cohort'];runtime=b['runtime'];sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();windows=os.name=='nt';private=packages/'_xge_native';fixture=root/'test/cases/foreign_xge3_document_renderer_policy.c';artifacts=native_artifacts(packages,engine);library=artifacts['library'];binary=artifacts['image'];inputs=dict(b['inputs']);inputs.update((str(x),sha(x)) for x in [Path(__file__),Path(__file__).with_name('foreign_native_artifacts.py'),fixture,library,binary,engine/'build-report.json',*[p for p in private.rglob('*') if p.is_file()],*[native/'tests/xui'/n for n in ('xui_owned_renderer_test.c','xui_owned_layout_test.c','xui_owned_fonts_test.c','xui_test_proxy.c','xui_test_proxy.h')]])
 if a.negative_private_runtime:inputs[str(a.negative_private_runtime.resolve())]=sha(a.negative_private_runtime)
 out.mkdir(parents=True,exist_ok=False);sdk=out/'sdk/__xl_runtime';sdk.mkdir(parents=True)
 for h in (source/'src/runtime').glob('*.h'):shutil.copy2(h,sdk/h.name);inputs[str(h)]=sha(h)
 shutil.copy2(source/'src/api/xl_foreign_error.h',sdk/'xl_foreign_error.h');inputs[str(source/'src/api/xl_foreign_error.h')]=sha(source/'src/api/xl_foreign_error.h')
 env=dict(os.environ)
 if windows:shutil.copy2(runtime,out/runtime.name);shutil.copy2(binary,out/binary.name)
 else:env['LD_LIBRARY_PATH']=str(engine)+':'+str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
 report=dict(scope=__doc__,complete=False,passed=False,shippingDelivery=False,fullSourceRebuild=b['fullSourceRebuild'],compilerManifest=b['buildManifest'] or b['overlay'],inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 def run(name,cmd,ok=True):
  cmd=list(map(str,cmd));actual=command_with_response(cmd,'gcc',out/(name+'.rsp')) if cmd[0]=='gcc' else cmd;t=time.monotonic();z=subprocess.run(actual,cwd=source,env=env,capture_output=True,timeout=300);log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr);report['rows'].append(dict(name=name,exitCode=z.returncode,accepted=ok,seconds=round(time.monotonic()-t,3),logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert (not z.returncode)==ok,log
 baseSdk=Path(b['base'].get('SdkDirectory') or b['base'].get('RuntimeDirectory') or runtime.parent);flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-DXGE_XRT_HOST=1',*['-I'+str(x) for x in (private/'include',out/'sdk',native/'tests/xui',native/'src/xui',native/'include',native,source,root,b['basePath'].parent/'include',baseSdk,baseSdk/'include')]];flags+=['-DXGE_DLL','-DXUI_DLL'] if windows else [];flags+=['-Wno-'+x for x in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')];system=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent),'-Wl,-rpath,'+str(engine)]
 for opt in ('O0','O2'):
  exe=out/(opt+('.exe' if windows else '.out'));run(opt+'-build',['gcc',*flags,'-'+opt,fixture,private/'src/runtime.c',native/'tests/xui/xui_test_proxy.c',*b['objects'],runtime,library,*system,'-o',exe]);run(opt+'-run',[exe])
 if a.negative_private_runtime:
  exe=out/('old-policy'+('.exe' if windows else '.out'));run('old-policy-build',['gcc',*flags,'-O0',fixture,a.negative_private_runtime,native/'tests/xui/xui_test_proxy.c',*b['objects'],runtime,library,*system,'-o',exe]);run('old-policy-rejected',[exe],False)
 report.update(complete=True,passed=True,inputsUnchanged=all(sha(x)==y for x,y in inputs.items()));assert report['inputsUnchanged'];save()
if __name__=='__main__':main()
