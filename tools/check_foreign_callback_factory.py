"""CPU-only callback-epoch receiver producing an independent owned root.

Generated C, actual retained callbacks, strict header ABI and destructive sealed
IR validation. This gate does not certify a shipping compiler or VFS provider.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from foreign_validation_build import add_build_arguments,load_build
from compiler_response import command_with_response


def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','out'):p.add_argument('--'+name,type=Path,required=True)
 add_build_arguments(p);a=p.parse_args();root=a.root.resolve();out=a.out.resolve();b=load_build(a.build,a.overlay)
 source=b['cohort'];driver=b['driver'];runtime=b['runtime'];sha=lambda x:hashlib.sha256(Path(x).read_bytes()).hexdigest()
 fixture=root/'test/cases/foreign_callback_factory.xl';native=fixture.with_name('foreign_callback_factory_native.c');header=fixture.with_suffix('.h');host=fixture.with_name('foreign_callback_factory_host.c');ir=fixture.with_name('foreign_callback_factory_ir.c')
 inputs=dict(b['inputs']);inputs.update((str(x),sha(x)) for x in (fixture,native,header,host,ir,Path(__file__)))
 runtimeHeaders=list((source/'src/runtime').glob('*.h'));inputs.update((str(x),sha(x)) for x in runtimeHeaders)
 out.mkdir(parents=True,exist_ok=False);saved=out/'inputs/test/cases';saved.mkdir(parents=True);local=out/'sdk/__xl_runtime';local.mkdir(parents=True)
 for x in (fixture,native,header,host,ir):shutil.copy2(x,saved/x.name)
 for x in runtimeHeaders:shutil.copy2(x,local/x.name)
 env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(source));windows=os.name=='nt'
 if windows:shutil.copy2(runtime,out/runtime.name)
 else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
 report=dict(scope=__doc__,complete=False,passed=False,fullSourceRebuild=b['fullSourceRebuild'],compilerManifest=b['buildManifest'] or b['overlay'],shippingDelivery=False,tccIncluded=False,inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 def run(name,command,accept=True):
  command=list(map(str,command));actual=command_with_response(command,'gcc',out/(name+'.rsp')) if command[0] in ('gcc','g++') else command
  started=time.monotonic();z=subprocess.run(actual,cwd=source,env=env,capture_output=True,timeout=240);log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
  report['rows'].append(dict(name=name,exitCode=z.returncode,accepted=accept,seconds=round(time.monotonic()-started,3),logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert (not z.returncode)==accept,log
 generated=out/'generated.c';run('cgen',[driver,fixture,'cgen-default',generated]);run('ir',[driver,fixture,'ir-lowered',out/'lowered.json']);run('header',[driver,fixture,'emit',out/'public'])
 emitted=generated.read_text(encoding='utf-8-sig');assert 'callback_factory_share(' in emitted and 'xl6ForeignResultProbeReserveRootV1(' in emitted and 'XL6_NativeCallbackScopeV1' in emitted
 base=b['base'];sdk=Path(base.get('SdkDirectory') or base.get('RuntimeDirectory') or runtime.parent)
 includes=['-I'+str(x) for x in (out/'inputs',out/'sdk',source,root,b['basePath'].parent/'include',sdk,sdk/'include')]
 common=['-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes]
 run('header-C',['gcc','-std=c11',*common,'-x','c','-fsyntax-only',out/'public.h'])
 for std in ('c++11','c++17'):run('header-'+std,['g++','-std='+std,*common,'-x','c++','-fsyntax-only',out/'public.h'])
 warnings=['-Wno-'+x for x in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')]
 systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
 for opt in ('O0','O2'):
  exe=out/(opt+('.exe' if windows else '.out'));run(opt+'-build',['gcc','-std=c11',*common,*warnings,'-'+opt,generated,native,host,*b['objects'],runtime,*systems,'-o',exe]);run(opt+'-run',[exe])
  gate=out/('ir-'+opt+('.exe' if windows else '.out'));run('ir-'+opt+'-build',['gcc','-std=c11',*common,'-'+opt,ir,*b['objects'],runtime,*systems,'-o',gate]);run('ir-'+opt+'-run',[gate,fixture])
 original=fixture.read_text(encoding='utf-8-sig');method='extern "callback_factory_share" @no_raise @c_result("owned_view") @factory(1,0,rollback)'
 changes={
  'raw-caller-receiver':('@lifetime("callback")','@lifetime("caller")'),
  'borrow-same-epoch':(method,'extern "callback_factory_share" @no_raise @c_result("owner_view",0) @factory(1,0,rollback)'),
  'callback-result':('func share() as Node','func share() as CallbackNode'),
  'inplace-from-epoch':(method,method+' @c_inplace("CallbackFactoryNode")'),
  'raising-producer':(method,method.replace('@no_raise ','')),
  'pure-producer':(method,method+' @pure'),
  'wrong-rollback':(method,method.replace('rollback)','calls)')),
 }
 for name,(before,after) in changes.items():
  assert before in original;text=original.replace(before,after,1);f=out/(name+'.xl');f.write_text(text,encoding='utf-8');target=out/(name+'.c');run(name,[driver,f,'cgen-default',target],False);assert not target.exists() or not target.stat().st_size
 nativeSignature='int callback_factory_share(CallbackFactoryNode*,CallbackFactoryNode**);'
 badSignatures={
  'receiver':'int callback_factory_share(void*,CallbackFactoryNode**);',
  'output':'int callback_factory_share(CallbackFactoryNode*,void**);',
  'status':'CallbackFactoryNode* callback_factory_share(CallbackFactoryNode*,CallbackFactoryNode**);',
 }
 for name,signature in badSignatures.items():
  bad=out/('bad-native-'+name);target=bad/'test/cases'/header.name;target.parent.mkdir(parents=True)
  authority=header.read_text(encoding='utf-8-sig');assert nativeSignature in authority;target.write_text(authority.replace(nativeSignature,signature),encoding='utf-8')
  for compiler,std,language in (('gcc','c11','c'),('g++','c++17','c++')):
   run('bad-native-'+name+'-'+std,[compiler,'-std='+std,'-I'+str(bad),*common,'-x',language,'-fsyntax-only',out/'public.h'],False)
 report.update(complete=True,passed=True,inputsUnchanged=all(sha(path)==digest for path,digest in inputs.items()));assert report['inputsUnchanged'];save()


if __name__=='__main__':main()
