"""Cold metadata selection and actual TinyCC JIT/bound-call acceptance.

The CPU provider is a source package, not an engine or OS transfer provider.
No matched VFS/AOT or Native SDK delivery is inferred from this focused gate.
"""
from pathlib import Path
import argparse,copy,hashlib,json,os,shutil,subprocess,time
from foreign_validation_build import add_build_arguments,load_build
from compiler_response import command_with_response

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','out'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--packages',type=Path,help='Optional existing matched XGE baseline; only group metadata/unused transfer source added')
 add_build_arguments(p);a=p.parse_args();root=a.root.resolve();out=a.out.resolve();out.mkdir(exist_ok=False)
 b=load_build(a.build,a.overlay);source=b['cohort'];runtime=b['runtime'];description=json.loads(Path(a.overlay or a.build).read_text(encoding='utf-8-sig'))
 sdk=Path(description.get('SdkDirectory')or b['base'].get('RuntimeDirectory'));headers=Path(b['includeOverlayRoot'])
 sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
 report=dict(complete=False,passed=False,scope=__doc__,fullRebuild=b['fullSourceRebuild'],inputs=dict(b['inputs']),rows=[])
 fixture=root/'test/cases/package_c_source_groups.c';report['inputs'][str(fixture)]=sha(fixture)
 if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
 env=dict(os.environ)
 if os.name!='nt':
  env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
  env['XL6_SOURCE_NATIVE_ROOT']=str(b['base']['NativeRoot'])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 def run(name,args,expected=0,cwd=source):
  actual=command_with_response(list(map(str,args)),'gcc',out/(name+'.rsp'))
  t=time.monotonic();r=subprocess.run(actual,cwd=cwd,env=env,capture_output=True,timeout=180)
  log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
  report['rows'].append(dict(name=name,command=actual,exitCode=r.returncode,expected=expected,seconds=round(time.monotonic()-t,3),logSha256=sha(log)));save();print(name,r.returncode,flush=True)
  assert (r.returncode==0)==(expected==0),log
  return r
 valid={'format':'xlang-package-1','name':'conditional','version':'1.0.0','artifact':{'cSources':['base.c'],'xlSources':['api.xl'],'includeDirs':['.'],'xrtModules':[],
  'runtimeSymbols':['xl6ErrorScopeTryPropagateFailureV1'],'cSourceGroups':[{'source':'cold.c','providedSymbols':['coldBegin','coldTwo','coldThree','coldFour'],'requiredSymbols':['xl6TransferServicesFromLeaseV1','xl6TransferServicesProcessResidentV1']}]}}
 cases=out/'metadata';cases.mkdir();(cases/'valid.json').write_text(json.dumps(valid))
 unknown=copy.deepcopy(valid);unknown['artifact']['cSourceGroups'][0]['requiredSymbols']=['xl6UnknownResidentSymbol']
 (cases/'unknown.json').write_text(json.dumps(unknown))
 invalid=[]
 def bad(name,change):
  data=copy.deepcopy(valid);change(data['artifact']);path=cases/(name+'.json');path.write_text(json.dumps(data));invalid.append(path)
 bad('groups-not-array',lambda v:v.update(cSourceGroups={}))
 bad('group-not-object',lambda v:v.update(cSourceGroups=[1]))
 for name,value in [('absolute','/tmp/cold.c'),('drive','C:/cold.c'),('escape','../cold.c'),('dot','src/./cold.c'),('wrong-extension','cold.h'),('nul-source','cold.c\0bad')]:
  bad(name,lambda v,value=value:v['cSourceGroups'][0].update(source=value))
 for field in ('providedSymbols','requiredSymbols'):
  for name,value in [('missing',None),('empty',[]),('non-string',[1]),('invalid',['bad-name']),('nul',['good\0bad']),('duplicate',['coldBegin','coldBegin'])]:
   bad(field+'-'+name,lambda v,field=field,value=value:v['cSourceGroups'][0].pop(field)if value is None else v['cSourceGroups'][0].update({field:value}))
 bad('duplicate-group',lambda v:v['cSourceGroups'].append(copy.deepcopy(v['cSourceGroups'][0])))
 bad('overlap-always',lambda v:v['cSources'].append('cold.c'))
 bad('overlap-provided',lambda v:v['cSourceGroups'].append(dict(source='other.c',providedSymbols=['coldBegin'],requiredSymbols=['xl6TransferServicesProcessResidentV1'])))
 system=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32','-luuid']if os.name=='nt'else['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
 flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*['-I'+str(v)for v in(source,headers,sdk,sdk/'include')]]
 try:
  kernels=[]
  for opt in ('O0','O2'):
   kernel=out/(opt+('.exe'if os.name=='nt'else''));kernels.append(kernel)
   run(opt+'-build',['gcc',*flags,'-'+opt,fixture,*b['objects'],runtime,*([description['TccCore']]if os.name=='nt'else[]),*system,'-o',kernel])
   run(opt+'-select',[kernel,'select',cases/'valid.json','1'])
   run(opt+'-valid',[kernel,'parse',cases/'valid.json','1'])
   run(opt+'-unknown-unselected',[kernel,'unknown',cases/'unknown.json','0'])
   for path in invalid:run(opt+'-'+path.stem,[kernel,'parse',path,'0'])
  api='''extern "group_receiver_t*" class Receiver @header("group.h") @lifetime("caller")
 native point handle
 extern "coldFour" @no_raise @nonblocking func cold(c_int value) as c_int
end
extern "groupReceiver" @header("group.h") @no_raise @nonblocking func receiver() as Receiver
extern "groupWarm" @header("group.h") @no_raise @nonblocking func warm() as c_int
extern "coldBegin" @header("group.h") @no_raise @nonblocking func one() as c_int
extern "coldTwo" @header("group.h") @no_raise @nonblocking func two() as c_int
extern "coldThree" @header("group.h") @no_raise @nonblocking func three() as c_int
'''
  api='export '+api.replace('\nextern ', '\nexport extern ')
  header='''#ifndef GROUP_CPU_H
#define GROUP_CPU_H
typedef struct group_receiver_t { int value; } group_receiver_t;
group_receiver_t* groupReceiver(void);
int groupWarm(void); int coldBegin(void); int coldTwo(void); int coldThree(void); int coldFour(group_receiver_t*,int);
#endif
'''
  cold='''#include "group.h"
#include <__xl_runtime/xl6_native_com.h>
static int checked(void){ xn_transfer_services_v1 s={0}; return xl6TransferServicesProcessResidentV1(0,&s) && s.wrapDataObject && s.wrapDropSource && s.wrapDropTarget; }
int coldBegin(void){return checked()?11:-1;}
int coldTwo(void){return checked()?12:-1;}
int coldThree(void){return checked()?13:-1;}
int coldFour(group_receiver_t* receiver,int value){return checked()?receiver->value+value:-1;}
'''
  packages={}
  for package in ('conditional','idle'):
   folder=out/package;folder.mkdir();packages[package]=folder
   data=copy.deepcopy(valid);data['name']=package;(folder/'xlang.package.json').write_text(json.dumps(data))
   (folder/'api.xl').write_text(api);(folder/'group.h').write_text(header)
   (folder/'base.c').write_text('#include "group.h"\nstatic group_receiver_t receiver={17};\ngroup_receiver_t* groupReceiver(void){return &receiver;}\nint groupWarm(void){return 7;}\n')
   (folder/'cold.c').write_text(cold if package=='conditional'else'#error UNSELECTED_C_SOURCE_MUST_NOT_COMPILE\n')
  programs={'unused':('idle','assert idle.warm()==c_int(7)',False),'one':('conditional','assert conditional.one()==c_int(11)',True),
   'two':('conditional','assert conditional.two()==c_int(12)',True),'three':('conditional','assert conditional.three()==c_int(13)',True),
   'bound':('conditional','auto value=conditional.receiver()\n func(c_int) as c_int callback=value.cold\n assert callback(c_int(5))==c_int(22)',True)}
  for name,(package,body,selected)in programs.items():
   path=out/(name+'.xl');path.write_text('import "./'+package+'"\nfunc main(...) as int\n '+body+'\n return 0\nend\n')
   # A same-stem .c beside the .xl is an explicit existing sidecar feature;
   # generated C must have a different basename to avoid importing itself.
   run(name+'-emit',[b['driver'],path,'emit',out/('generated-'+name)])
   dist=source/'res/tcc/win64/x86_64'if os.name=='nt'else Path(b['base']['NativeRoot'])/'build/linux/tcc-dist'
   if os.name=='nt':
    run(name+'-run',[b['driver'],path,'run',out/(name+'-run')])
    run(name+'-module',[b['driver'],path,'module',out/(name+'-module')])
   else:
    # The historical stock Linux driver hardcodes the old NativeRoot provider.
    # Use public API options to name the already qualified Only-TCC provider;
    # do not create a flat release alias or silently load another DLL.
    run(name+'-run',[kernels[-1],'jit',path,source,out/(name+'-temps'),description['TccCore'],dist,headers])
    run(name+'-module',[kernels[-1],'module',path,source,out/(name+'-module-temps'),description['TccCore'],dist,headers])
   report['rows'][-1]['selectedExpected']=selected
   run(name+'-artifact-plan',[kernels[-1],'program-plan',path,'1'if selected else'0'])
   environment=out/(name+'-environment.json')
   run(name+'-environment',[kernels[-1],'bridge',path,source,environment,description['TccCore'],dist])
   data=json.loads(environment.read_text(encoding='utf-8-sig'))['cBridge']['nativeEnvironment']
   assert len(data['cSourceGroups'])==1 and data['cSourceGroups'][0]['selected']==selected
   assert any(v.endswith('cold.c')for v in data['selectedCSources'])==selected
   assert ('xl6TransferServicesProcessResidentV1'in data['requiredRuntimeSymbols'])==selected
   report['rows'][-1]['selectionReportVerified']=True
  if a.packages:
   original=a.packages.resolve();public=out/'public-packages';shutil.copytree(original,public)
   report['inputs'].update((str(path),sha(path))for path in original.rglob('*')if path.is_file())
   manifest=root/'lib/bindings/_xge_native/xlang.package.json';transfer=root/'lib/bindings/_xge_native/src/transfer.c'
   for file in(manifest,transfer):report['inputs'][str(file)]=sha(file)
   target=public/'_xge_native/xlang.package.json';old=json.loads(target.read_text(encoding='utf-8-sig'));current=json.loads(manifest.read_text(encoding='utf-8-sig'))
   old['artifact']['cSourceGroups']=current['artifact']['cSourceGroups'];target.write_text(json.dumps(old,indent=2)+'\n')
   shutil.copy2(transfer,public/'_xge_native/src/transfer.c');env['XLANG_PACKAGE_PATH']=str(public)
   path=out/'public-xge.xl';path.write_text('import xge\nfunc main(...) as int\n auto color=xge.colorRGBA(c_int(1),c_int(2),c_int(3),c_int(4))\n assert color!=c_uint(0)\n return 0\nend\n')
   run('public-xge-emit',[b['driver'],path,'emit',out/'generated-public-xge'])
   if os.name=='nt':
    run('public-xge-run',[b['driver'],path,'run',out/'public-xge-run'])
    run('public-xge-module',[b['driver'],path,'module',out/'public-xge-module'])
   else:
    run('public-xge-run',[kernels[-1],'jit',path,source,out/'public-xge-temps',description['TccCore'],dist,headers])
    run('public-xge-module',[kernels[-1],'module',path,source,out/'public-xge-module-temps',description['TccCore'],dist,headers])
   run('public-xge-no-com-plan',[kernels[-1],'program-plan',path,'0'])
   result=out/'public-xge-environment.json';run('public-xge-report',[kernels[-1],'bridge',path,source,result,description['TccCore'],dist])
   data=json.loads(result.read_text(encoding='utf-8-sig'))['cBridge']['nativeEnvironment']
   assert not data['cSourceGroups'][0]['selected'] and not any(p.endswith('transfer.c')for p in data['selectedCSources'])
   assert not any(p.startswith('xl6TransferServices')for p in data['requiredRuntimeSymbols'])
   report['publicBaseline']=dict(source=str(original),scope='Existing matched Native/public baseline plus group metadata only; pure CPU color call, not a new full Native SDK qualification')
  report.update(complete=True,passed=True)
 except Exception as error:report['error']=str(error);raise
 finally:
  report['inputsUnchanged']=all(sha(path)==digest for path,digest in report['inputs'].items());save()
if __name__=='__main__':main()
