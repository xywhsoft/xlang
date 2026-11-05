"""CPU-only typed native Array projection, read-pin and sealed IR acceptance.

Records an actual full compiler or explicitly partial development overlay.
No GUI/device/network execution. Producer calls are native direct calls;
projection allocations, backing pins and element scopes are audited separately.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time
from foreign_validation_build import add_build_arguments,load_build

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for key in ('root','out'):p.add_argument('--'+key,type=Path,required=True)
 add_build_arguments(p);a=p.parse_args();root,out=a.root.resolve(),a.out.resolve();b=load_build(a.build,a.overlay)
 cohort,driver,runtime,basePath,base,objects=[b[k] for k in ('cohort','driver','runtime','basePath','base','objects')]
 sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
 case=root/'test/cases/foreign_array_projection.xl';header=root/'test/cases/foreign_array_projection.h'
 native=root/'test/cases/foreign_array_projection_native.c';ir=root/'test/cases/foreign_array_projection_ir.c'
 pin=root/'test/cases/foreign_array_read_pin_runtime.c';host=root/'test/cases/foreign_decl_module_host.c'
 inputs=dict(b['inputs']);inputs.update((str(x),sha(x)) for x in (driver,runtime,case,header,native,ir,pin,host,Path(__file__)))
 out.mkdir(parents=True,exist_ok=False);report=dict(scope=__doc__,complete=False,passed=False,fullSourceRebuild=b['fullSourceRebuild'],buildManifest=b['buildManifest'],overlay=b['overlay'],inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
 env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort))
 if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
 else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
 from compiler_response import command_with_response
 def run(name,command,ok=True):
  command=list(map(str,command));actual=command_with_response(command,'gcc',out/(name+'.rsp')) if command[0] in ('gcc','g++') else command
  start=time.monotonic();z=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=240)
  log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr);report['rows'].append(dict(name=name,exitCode=z.returncode,expectedSuccess=ok,logSha256=sha(log),seconds=round(time.monotonic()-start,3)));save();print(name,z.returncode,flush=True)
  assert (z.returncode==0)==ok,log
 generated=out/'generated.c';run('cgen',[driver,case,'cgen-default',generated]);run('ir',[driver,case,'ir-lowered',out/'lowered.json']);run('header',[driver,case,'emit',out/'public'])
 code=generated.read_text(encoding='utf-8-sig');assert 'xl6SequenceReadPinBeginV1' in code and 'xrtTypedArrayConstData' in code and 'native_array_views_' in code
 assert 'array_projection_roots(' in code and 'native Array pointee ABI' in code
 sdk=Path(base.get('SdkDirectory') or base.get('RuntimeDirectory'))
 # New runtime declarations come from the same frozen compiler source; the
 # generic baseline SDK supplies all unchanged public private-header inputs.
 local=out/'sdk/__xl_runtime';local.mkdir(parents=True)
 shutil.copy2(cohort/'src/runtime/xl6_sequence.h',local/'xl6_sequence.h')
 includes=['-I'+str(x) for x in (out/'sdk',cohort/'src/runtime',cohort,basePath.parent/'include',sdk,sdk/'include',root)]
 common=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes]
 run('header-C',['gcc',*common,'-x','c','-fsyntax-only',out/'public.h'])
 for std in ('c++11','c++17'):run('header-'+std,['g++','-std='+std,'-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes,'-x','c++','-fsyntax-only',out/'public.h'])
 warnings=['-Wno-'+x for x in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')]
 systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
 for opt in ('O0','O2'):
  exe=out/(opt+('.exe' if os.name=='nt' else '.out'))
  run(opt+'-build',['gcc',*common,*warnings,'-'+opt,generated,native,host,*objects,runtime,*systems,'-o',exe]);run(opt+'-run',[exe])
  for name,fixture in (('ir',ir),('pin',pin)):
   exe=out/(name+'-'+opt+('.exe' if os.name=='nt' else '.out'))
   run(name+'-'+opt+'-build',['gcc',*common,'-'+opt,fixture,*objects,runtime,*systems,'-o',exe])
   run(name+'-'+opt+'-run',[exe,*([case] if name=='ir' else [])])
 original=case.read_text(encoding='utf-8-sig')
 negatives={
  'nullable-source':original.replace('array<c_int> values,int size','array<c_int>? values,int size',1),
  'nullable-pod':original.replace('array<Pod> values,int size','array<Pod?> values,int size',1),
  'missing-column':original.replace('@c_array_input(0,1,"const int*","value")','',1),
  'duplicate-column':original.replace('@c_array_input(0,1,"const int*","value")','@c_array_input(0,0,"const int*","value")',1),
  'size-position':original.replace('@c_array_group(0,0,2,"int",2)','@c_array_group(0,0,2,"int",1)',1),
  'field-injection':original.replace('@c_array_field(0,"title",0,"c_string")','@c_array_field(0,"title;",0,"c_string")',1),
  'pod-as-view':original.replace('@c_array_input(0,0,"const ArrayPod*","value")','@c_array_input(0,0,"const ArrayPod*","view_raw")',1),
  'source-const-overlap':original.replace('@c_array_group(0,0,1,"int",1)','@c_const(0) @c_array_group(0,0,1,"int",1)',1),
  'size-const-overlap':original.replace('@c_array_group(0,0,1,"int",1)','@c_const(1) @c_array_group(0,0,1,"int",1)',1),
  'unknown-count':original.replace('@c_array_group(0,0,1,"int",1)','@c_array_group(0,0,1,"long",1)',1),
  'unknown-element-mode':original.replace('"const int*","value"','"const int*","mutable"',1)}
 for name,text in negatives.items():
  fixture=out/(name+'.xl');fixture.write_text(text,encoding='utf8');target=out/(name+'.c');run('reject-'+name,[driver,fixture,'cgen-default',target],False);assert not target.exists()
 original=header.read_text(encoding='utf-8-sig')
 for name,before,after in (('pointer','int array_projection_sum(const int*,int);','int array_projection_sum(const float*,int);'),('count','int array_projection_pair(const int*,const int*,int);','int array_projection_pair(const int*,const int*,size_t);'),('field','const char* title; int result,semantic;','const char* title; float result,semantic;'),('union','typedef struct ArrayItem {','typedef union ArrayItem {')):
  bad=out/('bad-'+name+'/test/cases');bad.mkdir(parents=True);assert before in original;(bad/header.name).write_text(original.replace(before,after),encoding='utf8')
  run('reject-native-'+name,['gcc','-I'+str(bad.parents[1]),*common,*warnings,'-fsyntax-only',generated],False)
  run('reject-native-public-'+name,['g++','-std=c++17','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(bad.parents[1]),*includes,'-x','c++','-fsyntax-only',out/'public.h'],False)
 report['inputsUnchanged']=all(sha(path)==expected for path,expected in inputs.items());assert report['inputsUnchanged'];report.update(complete=True,passed=True);save()

if __name__=='__main__':main()
