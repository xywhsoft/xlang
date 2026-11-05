"""Document UI direct declarations, authoritative ABI and CPU-only execution.

Browser/image/clipboard/device entry points are compiled only. The actual
script builds a logical View/Editor/MessageList graph and CPU object layout.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from compiler_response import command_with_response
from foreign_validation_build import add_build_arguments,load_build
from check_xge3_bindings import foreign_method_declaration,split

def reachable(packages):
 manifest=json.loads((packages/'xui/xlang.package.json').read_text());source='\n'.join((packages/'xui'/x).read_text() for x in manifest['artifact']['xlSources']);methods=[];owner=None;methodsSource=[]
 for line in source.splitlines():
  cls=re.match(r'export extern "[^\"]+" class (\w+)',line)
  if cls:owner=cls[1];continue
  if line=='end':owner=None;continue
  call=foreign_method_declaration(line)
  if not call:continue
  native,kind,name,parameters=call
  if not (native.startswith(('xuiDocumentRenderer','xuiDocumentView','xuiDocumentEditor','xuiDocumentWebProvider','xuiDocumentHtmlInteraction','xuiDocumentImageResource','xuiDocumentFontOutput')) or (owner=='MessageList' and ('Document' in native or native=='xuiMessageListAddNode')) or (owner and owner.startswith('CallbackDocument'))):continue
  if not owner and not line.startswith('export extern'):continue
  params=([owner+' receiver'] if owner else []);args=[];locals=[]
  for i,p in enumerate(split(parameters)):
   ty=p.split('=',1)[0].strip().rsplit(' ',1)[0];v='arg'+str(i)
   if ty.endswith('?'):locals.append(ty+' '+v+'=null');args.append(v)
   else:params.append(ty+' '+v);args.append(v)
  expression=('receiver.' if owner else '')+name
  if kind=='get':expression=expression
  elif kind=='let':expression+='='+args[0]
  else:expression+='('+','.join(args)+')'
  methods.append(dict(native=native,logical=(owner+'.' if owner else '')+name));methodsSource.append('export "document_ui_probe_'+str(len(methods))+'" func probe'+str(len(methods))+'('+','.join(params)+')\n '+'\n '.join(locals)+'\n '+expression+'\nend')
 recordSource=(packages/'xui/src/document_ui_records.xl').read_text();proof='\n'.join('export "document_ui_record_'+name+'" func record'+name+'('+name+' value) as '+name+'\n return value\nend' for name in re.findall(r'export extern "[^"]+" struct (\w+)',recordSource))
 return source+'\n'+'\n'.join(methodsSource)+'\n'+proof+'\nfunc main(...) as int\n return 0\nend\n',methods

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','packages','engine-build','out'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--actual-only',action='store_true');add_build_arguments(p);a=p.parse_args();root,packages,engine,out=[x.resolve() for x in (a.root,a.packages,a.engine_build,a.out)];b=load_build(a.build,a.overlay);source=b['cohort'];driver=b['driver'];runtime=b['runtime'];sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();windows=os.name=='nt'
 metadata=engine/'build-report.json';native=json.loads(metadata.read_text());assert native['linked'] and native['xrt_mode']=='host';binary=engine/('xge.dll' if windows else 'libxge.so');library=engine/('xge.lib' if windows else 'libxge.so');private=packages/'_xge_native';fixture=root/'test/cases/foreign_xge3_package_document_ui.xl';host=root/'test/cases/foreign_decl_module_host.c';inputs=dict(b['inputs']);inputs.update((str(x),sha(x)) for x in [Path(__file__),fixture,host,metadata,binary,library,*[p for p in packages.rglob('*') if p.is_file()]])
 out.mkdir(parents=True,exist_ok=False);sdk=out/'sdk/__xl_runtime';sdk.mkdir(parents=True)
 for h in (source/'src/runtime').glob('*.h'):shutil.copy2(h,sdk/h.name);inputs[str(h)]=sha(h)
 env=dict(os.environ,XLANG_PACKAGE_PATH=str(packages),XL6_SOURCE_NATIVE_ROOT=str(source))
 if windows:shutil.copy2(runtime,out/runtime.name);shutil.copy2(binary,out/binary.name)
 else:env['LD_LIBRARY_PATH']=str(engine)+':'+str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
 report=dict(scope=__doc__,complete=False,passed=False,fullSourceRebuild=b['fullSourceRebuild'],compilerManifest=b['buildManifest'] or b['overlay'],shippingDelivery=False,uiDeviceOrNetworkExecuted=False,inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 def run(name,cmd):
  cmd=list(map(str,cmd));actual=command_with_response(cmd,'gcc',out/(name+'.rsp')) if cmd[0] in ('gcc','g++') else cmd;t=time.monotonic();z=subprocess.run(actual,cwd=source,env=env,capture_output=True,timeout=300);log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr);report['rows'].append(dict(name=name,exitCode=z.returncode,seconds=round(time.monotonic()-t,3),logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert not z.returncode,log
 baseSdk=Path(b['base'].get('SdkDirectory') or b['base'].get('RuntimeDirectory') or runtime.parent);flags=['-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-DXGE_XRT_HOST=1',*['-I'+str(x) for x in (private/'include',out/'sdk',source,root,b['basePath'].parent/'include',baseSdk,baseSdk/'include')]];flags+=['-DXGE_DLL','-DXUI_DLL'] if windows else [];warnings=['-Wno-'+x for x in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')];systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent),'-Wl,-rpath,'+str(engine)]
 if not a.actual_only:
  text,methods=reachable(packages);probe=out/'all-declared.xl';probe.write_text(text);report['declaredCalls']=methods
  for action,dest in [('cgen-default','all.c'),('ir-lowered','all.json'),('emit','public')]:run('all-'+action,[driver,probe,action,out/dest])
  for std,cc,lang in [('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')]:run('header-'+std,[cc,'-std='+std,*flags,'-x',lang,'-fsyntax-only',out/'public.h'])
  for opt in ('O0','O2'):run('all-'+opt+'-compile',['gcc','-std=c11',*flags,*warnings,'-'+opt,'-c',out/'all.c','-o',out/('all-'+opt+'.o')])
 generated=out/'actual.c';run('actual-cgen',[driver,fixture,'cgen-default',generated]);run('actual-ir',[driver,fixture,'ir-lowered',out/'actual.json']);code=generated.read_text();assert 'sizeof(xui_doc_view_desc_t)' in code and 'sizeof(xui_doc_renderer_desc_t)' in code and 'xl6Xge3RuntimeBindV1(' in code
 for opt in ('O0','O2'):
  exe=out/(opt+('.exe' if windows else '.out'));run(opt+'-build',['gcc','-std=c11',*flags,*warnings,'-'+opt,generated,private/'src/runtime.c',host,*b['objects'],runtime,library,*systems,'-o',exe]);run(opt+'-run',[exe])
 report.update(complete=True,passed=True,inputsUnchanged=all(sha(x)==y for x,y in inputs.items()));assert report['inputsUnchanged'];save()
if __name__=='__main__':main()
