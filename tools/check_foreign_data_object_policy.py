"""Actual private DataObject policies against standalone Native C components.

FP0 explicitly disables bootstrap. The canonical host-contract type is used
only to parse unreachable binder code; no matching SDK/DLL claim is made.
The full private TU is compiled with function/data sections, retaining only
the two DataObject policies and their real TransferDomain. No Widget, engine,
COM, clipboard or device implementation is substituted or executed.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_validation_build import add_build_arguments,load_build
from compiler_response import command_with_response

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','private-root','native-root','out'):p.add_argument('--'+name,type=Path,required=True)
 add_build_arguments(p);a=p.parse_args();selected=load_build(a.build,a.overlay);root=a.root.resolve();native=a.native_root.resolve();private=a.private_root.resolve();out=a.out.resolve();out.mkdir(exist_ok=False)
 source=selected['cohort'];base=selected['base'];runtime=selected['runtime'];sdk=Path(base.get('RuntimeDirectory')or base.get('SdkDirectory')or source/'release/win_x64')
 sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();fixture=root/'test/cases/foreign_xge3_data_object_policy.c'
 inputs=dict(selected['inputs']);paths=[fixture,private/'src/runtime.c',private/'include/xge3_runtime.h',Path(__file__).resolve(),
  native/'src/xge/input/xge_data_object.c',native/'src/xui/core/xui_data_object.c',native/'src/xge/core/xge_data_lifetime.inl',native/'src/xge/core/xge_data_lifetime_internal.h']
 paths.extend(p for folder in (native/'include',native/'extlibs/xrt')for p in folder.rglob('*.h'));inputs.update((str(p),sha(p))for p in paths)
 includes=out/'include';includes.mkdir();public=out/'sdk/__xl_runtime';public.mkdir(parents=True)
 for h in (source/'src/runtime').glob('*.h'):shutil.copy2(h,public/h.name);inputs[str(h)]=sha(h)
 shutil.copy2(source/'src/api/xl_foreign_error.h',public/'xl_foreign_error.h')
 (includes/'xge_build_config.h').write_text('#ifndef XGE_BUILD_CONFIG_H\n#define XGE_BUILD_CONFIG_H\n#include <stdint.h>\n#include <xge_config.h>\n/* Explicitly unsupported full-engine bootstrap, pure policy CPU subset. */\n#define XGE_BUILD_ABI_FINGERPRINT UINT64_C(0)\n#endif\n')
 (includes/'xge_host_xrt_contract.h').write_text('#ifndef XGE_HOST_XRT_CONTRACT_H\n#define XGE_HOST_XRT_CONTRACT_H\n#include <xge_host_xrt.h>\n/* Canonical type, unsupported zero contract; binder is not linked/called. */\n#define XGE_HOST_XRT_LOCAL_CONTRACT_INIT {0}\n#endif\n')
 if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
 report=dict(complete=False,passed=False,scope=__doc__,fullCompilerRebuild=False,bootstrapValidated=False,matchingEngineSdk=False,inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 cc=shutil.which('gcc');assert cc
 def run(name,args,success=True):
  args=command_with_response(list(map(str,args)),cc,out/(name+'.rsp'));start=time.monotonic();r=subprocess.run(args,cwd=native,capture_output=True,timeout=180)
  log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr);report['rows'].append(dict(name=name,command=args,exitCode=r.returncode,expectedSuccess=success,seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save()
  print(name,r.returncode,flush=True);assert (r.returncode==0)==success,log
 flags=['-std=c11','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-fno-asynchronous-unwind-tables','-fno-unwind-tables','-fno-jump-tables','-DXL_STATIC_EMBED','-DXRT_MODULE_ALL=',
  '-Wno-unused-function','-Wno-cast-function-type',*['-I'+str(p)for p in (includes,private/'include',out/'sdk',native/'include',native/'src/xge/core',native,source,root,sdk,sdk/'include')]]
 if os.name=='nt':flags.extend(['-DXGE_DLL','-DXGE_BUILD_DLL','-DXUI_DLL','-DXUI_BUILD_DLL'])
 systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']if os.name=='nt'else['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
 try:
  for opt in ('O0','O2'):
   objects=[]
   for name,path in [('private',private/'src/runtime.c'),('xge-data',native/'src/xge/input/xge_data_object.c'),('xui-data',native/'src/xui/core/xui_data_object.c')]:
    obj=out/(name+'-'+opt+'.o');run(name+'-'+opt+'-compile',[cc,*flags,'-'+opt,'-c',path,'-o',obj]);objects.append(obj)
   target=out/(opt+('.exe'if os.name=='nt'else'.out'))
   run(opt+'-build',[cc,*flags,'-'+opt,fixture,*objects,*selected['objects'],runtime,*systems,'-Wl,--gc-sections','-o',target]);run(opt+'-run',[target])
  header=out/'header.cpp';header.write_text('#include <xge3_runtime.h>\nstatic_assert(sizeof(XL6_OwnershipDomainV2)==48,"domain ABI");\n')
  cxx=shutil.which('g++');assert cxx
  for standard in ('c++11','c++17'):run(standard+'-header',[cxx,*[f for f in flags if not f.startswith('-std=')],'-std='+standard,'-fsyntax-only',header])
  text=(private/'src/runtime.c').read_text(encoding='utf-8-sig');needle='report_resource_failure(status,#Prefix "ReleaseOwnerChecked");'
  start=text.index('#define XN_DATA_OBJECT_POLICY(');end=text.index('#undef XN_DATA_OBJECT_POLICY',start)
  block=text[start:end];assert block.count(needle)==1
  mutant=out/'lost-failure.c';mutant.write_text(text[:start]+block.replace(needle,'(void)status;',1)+text[end:]);bad=out/'lost-failure.o'
  run('negative-policy-compile',[cc,*flags,'-O0','-c',mutant,'-o',bad])
  target=out/('lost-failure'+('.exe'if os.name=='nt'else'.out'))
  run('negative-policy-build',[cc,*flags,'-O0',fixture,bad,out/'xge-data-O0.o',out/'xui-data-O0.o',*selected['objects'],runtime,*systems,'-Wl,--gc-sections','-o',target])
  run('negative-swallowed-null-failure',[target],False)
  nm=shutil.which('nm');assert nm
  run('reachable-private-symbols',[nm,'-g','--defined-only',out/('O2.exe'if os.name=='nt'else'O2.out')])
  names=re.findall(r'\b[Tt]\s+(xl6X(?:ge|ui)3\w+)\s*$',(out/'reachable-private-symbols.log').read_text(encoding='utf-8'),re.M)
  assert set(names)=={'xl6Xge3DataObjectPolicyGet','xl6Xui3DataObjectPolicyGet','xl6Xge3TransferDomainV1Get'}
  report['reachablePrivateExports']=names
  report['inputsUnchanged']=all(sha(k)==v for k,v in inputs.items());assert report['inputsUnchanged'];report.update(complete=True,passed=True)
 finally:save()
if __name__=='__main__':main()
