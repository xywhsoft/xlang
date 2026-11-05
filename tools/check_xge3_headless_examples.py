"""Run six existing CPU/headless XGE/XUI examples against a matching engine.

This is actual generated-C execution, separately qualified from the GUI examples.
No init/window/device/browser/network/clipboard operation is added by the harness.
Source compiler overlays and package/Native builds are pinned independently.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from foreign_validation_build import add_build_arguments,load_build
from compiler_response import command_with_response
EXAMPLES=(('xge','headless_frame'),('xge','headless_shape'),('xui','headless_path'),('xui','headless_cell'),('xui','context_graph'),('xui','manager_lifetime'))

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','packages','engine-build','out'):p.add_argument('--'+n,type=Path,required=True)
 add_build_arguments(p);a=p.parse_args();root,packages,engine,out=[x.resolve() for x in (a.root,a.packages,a.engine_build,a.out)];b=load_build(a.build,a.overlay);source=b['cohort'];driver=b['driver'];runtime=b['runtime'];sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();windows=os.name=='nt';metadata=engine/'build-report.json';native=json.loads(metadata.read_text(encoding='utf-8-sig'));assert native['linked'] and native['xrt_mode']=='host'
 binary=engine/('xge.dll' if windows else 'libxge.so');library=engine/('xge.lib' if windows else 'libxge.so');private=packages/'_xge_native';host=root/'test/cases/foreign_decl_module_host.c';inputs=dict(b['inputs']);inputs.update((str(x),sha(x)) for x in [Path(__file__),host,metadata,binary,library,*[p for p in packages.rglob('*') if p.is_file()]])
 for package,name in EXAMPLES:
  f=root/'lib/bindings'/package/'examples'/(name+'.xl');assert f.read_bytes()==(packages/package/'examples'/f.name).read_bytes();inputs[str(f)]=sha(f)
 out.mkdir(parents=True,exist_ok=False);local=out/'sdk/__xl_runtime';local.mkdir(parents=True)
 for h in (source/'src/runtime').glob('*.h'):inputs[str(h)]=sha(h);shutil.copy2(h,local/h.name)
 env=dict(os.environ,XLANG_PACKAGE_PATH=str(packages),XL6_SOURCE_NATIVE_ROOT=str(source))
 if windows:shutil.copy2(runtime,out/runtime.name);shutil.copy2(binary,out/binary.name)
 else:env['LD_LIBRARY_PATH']=str(engine)+':'+str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
 report=dict(complete=False,passed=False,scope=__doc__,compilerManifest=b['buildManifest'] or b['overlay'],fullSourceRebuild=b['fullSourceRebuild'],shippingDelivery=False,examples=[dict(package=p,name=n,executed=False) for p,n in EXAMPLES],inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 def run(name,cmd,timeout=240):
  cmd=list(map(str,cmd));actual=command_with_response(cmd,'gcc',out/(name+'.rsp')) if cmd[0] in ('gcc','g++') else cmd;t=time.monotonic();z=subprocess.run(actual,cwd=source,env=env,capture_output=True,timeout=timeout);log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr);report['rows'].append(dict(name=name,command=actual,exitCode=z.returncode,seconds=round(time.monotonic()-t,3),logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert not z.returncode,log
 base=b['base'];sdk=Path(base.get('SdkDirectory') or base.get('RuntimeDirectory') or runtime.parent);flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-DXGE_XRT_HOST=1',*['-I'+str(x) for x in (private/'include',out/'sdk',source,root,b['basePath'].parent/'include',sdk,sdk/'include')]];flags+=['-DXGE_DLL','-DXUI_DLL'] if windows else [];warning=['-Wno-'+x for x in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')];system=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent),'-Wl,-rpath,'+str(engine)]
 for package,name in EXAMPLES:
  tag=package+'-'+name;f=root/'lib/bindings'/package/'examples'/(name+'.xl');generated=out/(tag+'.c');run(tag+'-cgen',[driver,f,'cgen-default',generated]);run(tag+'-ir',[driver,f,'ir-lowered',out/(tag+'.json')])
  for opt in ('O0','O2'):
   exe=out/(tag+'-'+opt+('.exe' if windows else '.out'));run(tag+'-'+opt+'-build',['gcc',*flags,*warning,'-'+opt,generated,private/'src/runtime.c',host,*b['objects'],runtime,library,*system,'-o',exe]);run(tag+'-'+opt+'-run',[exe],30)
  report['examples'][EXAMPLES.index((package,name))]['executed']=True;save()
 report.update(complete=True,passed=True,inputsUnchanged=all(sha(x)==y for x,y in inputs.items()));assert report['inputsUnchanged'];save()

if __name__=='__main__':main()
