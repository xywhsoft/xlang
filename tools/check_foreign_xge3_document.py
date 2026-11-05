"""Actual default Document models and complete declared-call ABI, CPU only.

The compiler cohort, Native engine, package initializer and private ownership
policies are pinned independently. No GUI, file I/O, browser or network tests.
No C forwarding function is added to the package. This is not shipping proof.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from compiler_response import command_with_response
from foreign_native_artifacts import native_artifacts
from foreign_validation_build import add_build_arguments,load_build
from check_xge3_bindings import foreign_method_declaration,split


def reachable(package):
 source='\n'.join((package/'src'/n).read_text(encoding='utf-8-sig') for n in ('document_records.xl','document.xl'))
 methods=[];owner=None
 for line in source.splitlines():
  cls=re.match(r'export extern "[^\"]+" class (\w+)',line)
  if cls:owner=cls[1];continue
  if line=='end':owner=None;continue
  call=foreign_method_declaration(line)
  if not call or (not owner and not line.startswith('export extern')) or (owner and owner.startswith('Callback')):continue
  native,kind,name,parameters=call;params=[];arguments=[];locals=[]
  if owner:params.append(owner+' receiver')
  for index,parameter in enumerate(split(parameters)):
   type=parameter.split('=',1)[0].strip().rsplit(' ',1)[0];variable='arg'+str(index)
   if type=='DocumentChangeProc':arguments.append('func(CallbackDocument document,CallbackDocumentChangeSet changes) as c_int\n document.share()\n document.identity()\n document.revision()\n document.profile()\n document.markdownDialect()\n changes.share()\n changes.info()\n return c_int(0)\nend')
   elif type=='DocumentSourceBindingProc':arguments.append('func(uint64 ordinal,CallbackDocumentSourceBinding binding) as c_int\n auto copied=binding.value\n assert copied.iSize>c_uint(0)\n return c_int(0)\nend')
   elif type.endswith('?'):locals.append(type+' '+variable+'=null');arguments.append(variable)
   else:params.append(type+' '+variable);arguments.append(variable)
  expression=(owner and 'receiver.' or '')+name+'('+','.join(arguments)+')'
  method=dict(native=native,logical=(owner+'.' if owner else '')+name)
  method['source']='export "document_abi_'+str(len(methods))+'" func probe'+str(len(methods))+'('+','.join(params)+')\n '+'\n '.join(locals)+'\n '+expression+'\nend\n'
  methods.append(method)
 records=re.findall(r'export extern "[^"]+" struct (\w+)',source)
 proof='\n'.join('export "document_record_'+name+'" func record'+name+'('+name+' value) as '+name+'\n return value\nend' for name in records)
 return source+'\n'+'\n'.join(x['source'] for x in methods)+'\n'+proof+'\nfunc main(...) as int\n return 0\nend\n',methods


def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','packages','engine-build','out'):p.add_argument('--'+name,type=Path,required=True)
 add_build_arguments(p);a=p.parse_args();root,packages,engine,out=[x.resolve() for x in (a.root,a.packages,a.engine_build,a.out)]
 b=load_build(a.build,a.overlay);source=b['cohort'];driver=b['driver'];runtime=b['runtime'];windows=os.name=='nt';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
 metadata=engine/'build-report.json';native=json.loads(metadata.read_text(encoding='utf-8-sig'));assert native['linked'] and native['xrt_mode']=='host'
 artifacts=native_artifacts(packages,engine);binary=artifacts['image'];library=artifacts['library'];private=packages/'_xge_native'
 fixture=root/'test/cases/foreign_xge3_package_document.xl';host=fixture.with_name('foreign_xge3_package_document_host.c')
 example=root/'lib/bindings/xui/examples/document_model.xl'
 inputs=dict(b['inputs']);inputs.update((str(p),sha(p)) for p in (fixture,host,example,Path(__file__),Path(__file__).with_name('foreign_native_artifacts.py'),metadata,binary,library,engine/'xge_build_config.h'))
 inputs.update((str(p),sha(p)) for p in packages.rglob('*') if p.is_file());headers=list((source/'src/runtime').glob('*.h'));inputs.update((str(p),sha(p)) for p in headers)
 out.mkdir(parents=True,exist_ok=False);local=out/'sdk/__xl_runtime';local.mkdir(parents=True)
 for p in headers:shutil.copy2(p,local/p.name)
 shutil.copy2(fixture,out/fixture.name);shutil.copy2(host,out/host.name)
 env=dict(os.environ,XLANG_PACKAGE_PATH=str(packages),XL6_SOURCE_NATIVE_ROOT=str(source))
 if windows:shutil.copy2(runtime,out/runtime.name);shutil.copy2(binary,out/binary.name)
 else:env['LD_LIBRARY_PATH']=str(engine)+':'+str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
 report=dict(scope=__doc__,complete=False,passed=False,fullSourceRebuild=b['fullSourceRebuild'],compilerManifest=b['buildManifest'] or b['overlay'],nativeBuild=str(metadata),executed=True,shippingDelivery=False,tccIncluded=False,inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 def run(name,command):
  command=list(map(str,command));actual=command_with_response(command,'gcc',out/(name+'.rsp')) if command[0] in ('gcc','g++') else command
  t=time.monotonic();z=subprocess.run(actual,cwd=source,env=env,capture_output=True,timeout=300);log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
  report['rows'].append(dict(name=name,command=actual,exitCode=z.returncode,seconds=round(time.monotonic()-t,3),logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert not z.returncode,log
 base=b['base'];sdk=Path(base.get('SdkDirectory') or base.get('RuntimeDirectory') or runtime.parent)
 includes=['-I'+str(p) for p in (private/'include',out/'sdk',source,root,b['basePath'].parent/'include',sdk,sdk/'include')]
 common=['-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-DXGE_XRT_HOST=1',*includes]
 if windows:common+=['-DXGE_DLL','-DXUI_DLL']
 warning=['-Wno-'+x for x in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')]
 # Every declaration is reachable through an exported compile-only probe.
 text,methods=reachable(packages/'xui');probe=out/'all-declared.xl';probe.write_text(text,encoding='utf-8');report['declaredCalls']=[{k:v for k,v in x.items() if k!='source'} for x in methods]
 run('all-cgen',[driver,probe,'cgen-default',out/'all-declared.c']);run('all-ir',[driver,probe,'ir-lowered',out/'all-declared.json']);run('all-header',[driver,probe,'emit',out/'all-public'])
 for std,cc,language in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):run('all-header-'+std,[cc,'-std='+std,*common,'-x',language,'-fsyntax-only',out/'all-public.h'])
 for opt in ('O0','O2'):run('all-'+opt+'-compile',['gcc','-std=c11',*common,*warning,'-'+opt,'-c',out/'all-declared.c','-o',out/('all-'+opt+'.o')])
 constants=re.findall(r'export const (?:c_int|uint64) (\w+) = (-?\d+)',text)
 proof=out/'constant-proof.h'
 proof.write_text('#include "xge3_runtime.h"\n#ifdef __cplusplus\n#define DOCUMENT_ASSERT static_assert\n#else\n#define DOCUMENT_ASSERT _Static_assert\n#endif\n'+
  '\n'.join('DOCUMENT_ASSERT(((uint64_t)'+name+')==((uint64_t)('+value+('ULL' if not value.startswith('-') else '')+')), "document constant '+name+'");' for name,value in constants)+'\n',encoding='utf-8')
 for std,cc,language in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):run('constants-'+std,[cc,'-std='+std,*common,'-x',language,'-fsyntax-only',proof])
 run('example-cgen',[driver,example,'cgen-default',out/'example.c']);run('example-ir',[driver,example,'ir-lowered',out/'example.json'])
 for opt in ('O0','O2'):run('example-'+opt+'-compile',['gcc','-std=c11',*common,*warning,'-'+opt,'-c',out/'example.c','-o',out/('example-'+opt+'.o')])
 generated=out/'actual.c';run('actual-cgen',[driver,fixture,'cgen-default',generated]);run('actual-ir',[driver,fixture,'ir-lowered',out/'actual-ir.json'])
 code=generated.read_text(encoding='utf-8-sig')
 assert 'xl6Xge3RuntimeBindV1(' in code and 'native_raw.iSize = sizeof(native_raw);' in code
 for symbol in ('xuiDocumentCreateOwned','xuiDocumentShareOwned','xuiDocumentSnapshotCreateSourceIndex','xuiDocumentPrepareStreamSource','xuiDocumentFragmentImportHtmlOwned','xuiDocumentTxnCommit'):assert symbol+'(' in code
 systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent),'-Wl,-rpath,'+str(engine)]
 for opt in ('O0','O2'):
  executable=out/(opt+('.exe' if windows else '.out'));run(opt+'-build',['gcc','-std=c11',*common,*warning,'-'+opt,generated,private/'src/runtime.c',host,*b['objects'],runtime,library,*systems,'-o',executable]);run(opt+'-run',[executable])
 report.update(complete=True,passed=True,declaredCallCount=len(methods),constantProofCount=len(constants),PODRecordProofCount=len(re.findall(r'export extern "[^"]+" struct (\w+)',text)),exampleExecuted=False,inputsUnchanged=all(sha(path)==digest for path,digest in inputs.items()));assert report['inputsUnchanged'];save()


if __name__=='__main__':main()
