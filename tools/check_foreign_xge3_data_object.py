"""Direct DataObject/DataOutput declarations and CPU-only public behavior.

headers-only snapshots real Native headers with fingerprint zero explicitly;
it is not a matched SDK or runtime test. cpu requires a normally staged matched
package and real Native build. Clipboard/XGE OS drag calls are compile-only.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_validation_build import add_build_arguments,load_build
from compiler_response import command_with_response
from check_xge3_bindings import foreign_method_declaration,split
from foreign_native_artifacts import native_artifacts

def qualify(ty,family):
 return re.sub(r'\b(DataFormat|DataOutput|DataObject|CallbackDataObject|CallbackDragData|DataProvider|Context|Widget)\b',lambda m:family+'.'+m[0],ty)

def probes(packages):
 lines=['import xge','import xui'];methods=[]
 for family in ('xge','xui'):
  owner=None
  for line in (packages/family/'src/data_object.xl').read_text().splitlines():
   cls=re.match(r'export extern "[^\"]+" class (\w+)',line)
   if cls:owner=cls[1];continue
   if line=='end':owner=None;continue
   found=foreign_method_declaration(line)
   if not found or (not owner and not line.startswith('export ')):continue
   native,kind,name,parameters=found;params=[family+'.'+owner+' receiver'] if owner else [];args=[];locals=[]
   for i,p in enumerate(split(parameters)):
    ty=qualify(p.split('=',1)[0].strip().rsplit(' ',1)[0],family);v='arg'+str(i)
    if ty==family+'.DataProvider':locals.append(ty+' '+v+'=func('+family+'.DataFormat format,'+family+'.DataOutput output) as c_int\n return c_int(0)\nend')
    elif ty.endswith('?'):locals.append(ty+' '+v+'=null')
    else:params.append(ty+' '+v)
    args.append(v)
   expression=('receiver.' if owner else family+'.')+name
   if kind!='get':expression+='('+','.join(args)+')'
   methods.append(dict(family=family,native=native,logical=(owner+'.' if owner else '')+name,parameters=parameters))
   lines+=['export "data_object_probe_'+str(len(methods))+'" func probe'+str(len(methods))+'('+','.join(params)+')',' '+'\n '.join(locals),' '+expression,'end']
 context=(packages/'xui/src/managed.xl').read_text();start=context.index('class Context ');end=context.index('\nend',start)
 names={'xl6Xui3CopyToClipboard','xuiDragBegin','xuiDragAccept','xuiDragCancel','xuiDragIsActive','xuiDragGetEffect'}
 for line in context[start:end].splitlines():
  found=foreign_method_declaration(line)
  if not found or found[0] not in names:continue
  native,kind,name,parameters=found;params=['xui.Context receiver'];args=[]
  for i,p in enumerate(split(parameters)):
   ty=qualify(p.split('=',1)[0].strip().rsplit(' ',1)[0],'xui');v='arg'+str(i);params.append(ty+' '+v);args.append(v)
  methods.append(dict(family='xui',native=native,logical='Context.'+name,parameters=parameters));lines+=['export "data_object_probe_'+str(len(methods))+'" func probe'+str(len(methods))+'('+','.join(params)+')',' receiver.'+name+'('+','.join(args)+')','end']
 for line in context.splitlines():
  found=foreign_method_declaration(line)
  if not found or found[0]!='xuiEventDragData':continue
  methods.append(dict(family='xui',native=found[0],logical='CallbackConstEvent.'+found[2],parameters=found[3]));lines+=['export "data_object_probe_'+str(len(methods))+'" func probe'+str(len(methods))+'(xui.CallbackConstEvent receiver)',' receiver.dragData()','end']
 owner=None
 for line in (packages/'xge/src/callbacks.xl').read_text().splitlines():
  cls=re.match(r'export extern "[^\"]+" class (\w+)',line)
  if cls:owner=cls[1];continue
  if line=='end':owner=None;continue
  found=foreign_method_declaration(line)
  if not found or (owner!='DragEffectOutput' and not (owner=='DragEvent' and found[2]=='data') and found[0]!='xgeDragEventCallbackSetCheckedOwned'):continue
  native,kind,name,parameters=found;params=['xge.'+owner+' receiver'] if owner else [];args=[];locals=[]
  for i,parameter in enumerate(split(parameters)):
   ty=parameter.split('=',1)[0].strip().rsplit(' ',1)[0];ty='xge.'+ty if ty.startswith('CheckedDragHandler') else ty;v='arg'+str(i)
   if ty.endswith('?'):locals.append(ty+' '+v+'=null')
   else:params.append(ty+' '+v)
   args.append(v)
  expression=('receiver.' if owner else 'xge.')+name
  expression+='='+args[0] if kind=='let' else '' if kind=='get' else '('+','.join(args)+')'
  methods.append(dict(family='xge',native=native,logical=(owner+'.' if owner else '')+name,parameters=parameters));lines+=['export "data_object_probe_'+str(len(methods))+'" func probe'+str(len(methods))+'('+','.join(params)+')',' '+'\n '.join(locals),' '+expression,'end']
 return '\n'.join(lines)+'\nfunc main(...) as int\n return 0\nend\n',methods

def verify_stage(packages,engine,stageReport):
 sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();stage=json.loads(stageReport.read_text());metadata=engine/'build-report.json';build=json.loads(metadata.read_text());assert stage['complete'] and stage['inputsUnchanged'] and build['linked'] and build['xrt_mode']=='host' and build['public_abi_schema']==2
 assert Path(stage['engineBuild']).resolve()==metadata.resolve()
 assert all(sha(packages/f)==value for f,value in stage['outputs'].items())
 binary=native_artifacts(packages,engine)['image']
 return build,binary

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','out'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--mode',choices=('headers-only','cpu'),required=True);p.add_argument('--native-root',type=Path);p.add_argument('--packages',type=Path);p.add_argument('--engine-build',type=Path);p.add_argument('--stage-report',type=Path);add_build_arguments(p);a=p.parse_args();root,out=a.root.resolve(),a.out.resolve();b=load_build(a.build,a.overlay);cohort,driver,runtime=b['cohort'],b['driver'],b['runtime'];windows=os.name=='nt';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();out.mkdir(parents=True,exist_ok=False);inputs=dict(b['inputs']);inputs[str(Path(__file__).resolve())]=sha(__file__)
 if a.mode=='headers-only':
  assert a.native_root and not a.packages and not a.engine_build and not a.stage_report;native=a.native_root.resolve();packages=out/'packages';packages.mkdir();origins={}
  for name in ('_xge_native','xge','xui'):
   for f in (root/'lib/bindings'/name).rglob('*'):
    if f.is_file():origins[str(f)]=sha(f)
   shutil.copytree(root/'lib/bindings'/name,packages/name)
  private=packages/'_xge_native';inc=private/'include'
  for f in (native/'include').glob('*.h'):origins[str(f)]=sha(f);shutil.copy2(f,inc/f.name)
  (inc/'xge_build_config.h').write_text('/* HEADER-ONLY: not a generated SDK and never executed. */\n#define XGE_NATIVE_SDK_FINGERPRINT UINT64_C(0)\n')
  xrt=inc/'extlibs/xrt';xrt.mkdir(parents=True,exist_ok=True);config=native/'extlibs/xrt/xrt_config.h';origins[str(config)]=sha(config);shutil.copy2(config,xrt/config.name);(xrt/'xrt.h').write_text('#include <xrt_decl.h>\n')
  assert all(sha(f)==h for f,h in origins.items()),'source changed while snapshotting'
  (out/'header-only-snapshot.json').write_text(json.dumps(dict(scope='Real Native headers and public source snapshot, FP0, no SDK or runtime qualification',origins=origins,sourceUnchangedDuringCopy=True),indent=2)+'\n');engine=None;library=None
 else:
  assert a.packages and a.engine_build and a.stage_report and not a.native_root;packages=a.packages.resolve();private=packages/'_xge_native';engine=a.engine_build.resolve();metadata=engine/'build-report.json';stageReport=a.stage_report.resolve();build,binary=verify_stage(packages,engine,stageReport);artifacts=native_artifacts(packages,engine);library=artifacts['library'];inputs.update((str(f),sha(f)) for f in (metadata,binary,library,stageReport))
 inputs[str(Path(__file__).with_name('foreign_native_artifacts.py'))]=sha(Path(__file__).with_name('foreign_native_artifacts.py'))
 for f in packages.rglob('*'):
  if f.is_file():inputs[str(f)]=sha(f)
 fixture=root/'test/cases/foreign_xge3_package_data_object.xl';cycle=root/'test/cases/foreign_xge3_data_object_cycle.xl';cycleHeader=cycle.with_suffix('.h');drag=root/'test/cases/foreign_xge3_checked_drag.xl';dragHeader=drag.with_suffix('.h');dragNative=root/'test/cases/foreign_xge3_checked_drag_native.c';uiDrag=root/'test/cases/foreign_xge3_ui_drag.xl';uiDragHeader=uiDrag.with_suffix('.h');uiDragNative=root/'test/cases/foreign_xge3_ui_drag_native.c';example=root/'lib/bindings/xui/examples/data_object.xl';host=root/'test/cases/foreign_decl_module_host.c';inputs.update((str(f),sha(f)) for f in (fixture,cycle,cycleHeader,drag,dragHeader,dragNative,uiDrag,uiDragHeader,uiDragNative,example,host))
 sdk=out/'sdk/__xl_runtime';sdk.mkdir(parents=True)
 for f in (cohort/'src/runtime').glob('*.h'):shutil.copy2(f,sdk/f.name);inputs[str(f)]=sha(f)
 base=Path(b['base'].get('SdkDirectory') or b['base'].get('RuntimeDirectory') or runtime.parent);includes=[private/'include',out/'sdk',cohort,root,b['basePath'].parent/'include',base,base/'include'];flags=['-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-DXGE_XRT_HOST=1',*['-I'+str(f) for f in includes]];flags+=['-DXGE_DLL','-DXUI_DLL'] if windows else [];warnings=['-Wno-'+x for x in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')];systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]+(['-Wl,-rpath,'+str(engine)] if engine else [])
 env=dict(os.environ,XLANG_PACKAGE_PATH=str(packages),XL6_SOURCE_NATIVE_ROOT=str(cohort))
 if windows:shutil.copy2(runtime,out/runtime.name)
 else:env['LD_LIBRARY_PATH']=str(runtime.parent)+(':'+str(engine) if engine else '')+':'+env.get('LD_LIBRARY_PATH','')
 if engine and windows:shutil.copy2(binary,out/binary.name)
 report=dict(scope=__doc__,mode=a.mode,nativeExecuted=False,nativeFingerprintZero=a.mode=='headers-only',matchedSdk=a.mode=='cpu',shippingDelivery=False,coldTransferRuntimeLinked=False,uiOsClipboardNetworkOrGpuExecuted=False,compiler=b['buildManifest'] or b['overlay'],fullSourceRebuild=b['fullSourceRebuild'],complete=False,passed=False,inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 def run(name,cmd):
  cmd=list(map(str,cmd));actual=command_with_response(cmd,'gcc',out/(name+'.rsp')) if cmd[0] in ('gcc','g++') else cmd;t=time.monotonic();z=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=300);log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr);report['rows'].append(dict(name=name,exitCode=z.returncode,seconds=round(time.monotonic()-t,3),logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert not z.returncode,log
 text,methods=probes(packages);probe=out/'all.xl';probe.write_text(text);report['declaredCalls']=methods
 for action,dest in (('cgen-default','all.c'),('ir-lowered','all.json'),('emit','public')):run('all-'+action,[driver,probe,action,out/dest])
 for std,cc,lang in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):run('header-'+std,[cc,'-std='+std,*flags,'-x',lang,'-fsyntax-only',out/'public.h'])
 for opt in ('O0','O2'):run('all-'+opt+'-compile',['gcc','-std=c11',*flags,*warnings,'-'+opt,'-c',out/'all.c','-o',out/('all-'+opt+'.o')])
 for action,dest in (('cgen-default','actual.c'),('ir-lowered','actual.json')):run('actual-'+action,[driver,fixture,action,out/dest])
 for action,dest in (('cgen-default','cycle.c'),('ir-lowered','cycle.json')):run('cycle-'+action,[driver,cycle,action,out/dest])
 run('cycle-C11-compile',['gcc','-std=c11',*flags,*warnings,'-O2','-c',out/'cycle.c','-o',out/'cycle.o'])
 for action,dest in (('cgen-default','drag.c'),('ir-lowered','drag.json')):run('drag-'+action,[driver,drag,action,out/dest])
 run('drag-native-C11-compile',['gcc','-std=c11',*flags,'-O2','-c',dragNative,'-o',out/'drag-native.o'])
 for name,source in (('ui-drag',uiDrag),('example',example)):
  for action,ext in (('cgen-default','.c'),('ir-lowered','.json')):run(name+'-'+action,[driver,source,action,out/(name+ext)])
 run('ui-drag-native-C11-compile',['gcc','-std=c11',*flags,'-O2','-c',uiDragNative,'-o',out/'ui-drag-native.o'])
 for opt in ('O0','O2'):
  for name,extra in (('actual',[]),('drag',[dragNative]),('ui-drag',[uiDragNative]),('example',[])):
   if a.mode=='headers-only':run(name+'-'+opt+'-compile',['gcc','-std=c11',*flags,*warnings,'-'+opt,'-c',out/(name+'.c'),'-o',out/(name+'-'+opt+'.o')])
   else:
    # These CPU scripts do not reference the OS-transfer cold helpers. Their
    # public declarations are covered above; COM linkage needs its own cohort.
    exe=out/(name+'-'+opt+('.exe' if windows else '.out'));run(name+'-'+opt+'-build',['gcc','-std=c11',*flags,*warnings,'-'+opt,out/(name+'.c'),*extra,private/'src/runtime.c',host,*b['objects'],runtime,library,*systems,'-o',exe]);run(name+'-'+opt+'-run',[exe]);report['nativeExecuted']=True
 report.update(complete=True,passed=True,inputsUnchanged=all(sha(f)==h for f,h in inputs.items()));assert report['inputsUnchanged'];save()
if __name__=='__main__':main()
