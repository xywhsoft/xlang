"""Nullable protected Buffer spans and exact readonly byte-family overloads.

Runs only CPU Native fixture targets. C/C++ authority proof, O0/O2 execution,
reentrant source lifetime/mutation, malformed declarations and sealed IR gates.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from foreign_validation_build import add_build_arguments,load_build
from compiler_response import command_with_response

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','out'):p.add_argument('--'+name,type=Path,required=True)
 add_build_arguments(p);a=p.parse_args();root,out=a.root.resolve(),a.out.resolve();b=load_build(a.build,a.overlay);cohort,driver,runtime=b['cohort'],b['driver'],b['runtime'];sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
 stem='foreign_nullable_span';fixture=root/'test/cases'/(stem+'.xl');header=fixture.with_suffix('.h');native=fixture.with_name(stem+'_native.c');gate=fixture.with_name(stem+'_ir.c');host=root/'test/cases/foreign_decl_module_host.c'
 inputs=dict(b['inputs']);inputs.update((str(x),sha(x)) for x in (Path(__file__),fixture,header,native,gate,host));out.mkdir(parents=True,exist_ok=False)
 env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort));windows=os.name=='nt'
 if windows:shutil.copy2(runtime,out/runtime.name)
 else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
 report=dict(scope=__doc__,complete=False,passed=False,fullSourceRebuild=b['fullSourceRebuild'],buildManifest=b['buildManifest'],overlay=b['overlay'],inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 def run(name,cmd,accepted=True):
  cmd=list(map(str,cmd));actual=command_with_response(cmd,'gcc',out/(name+'.rsp')) if cmd[0] in ('gcc','g++') else cmd;t=time.monotonic();z=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=300);log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr);report['rows'].append(dict(name=name,exitCode=z.returncode,accepted=accepted,seconds=round(time.monotonic()-t,3),logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert (not z.returncode)==accepted,log
 generated=out/'generated.c';run('cgen',[driver,fixture,'cgen-default',generated]);run('ir',[driver,fixture,'ir-lowered',out/'lowered.json']);run('public',[driver,fixture,'emit',out/'public'])
 text=generated.read_text();assert 'native_span_data_' in text and '&& xrtValueType(' in text and 'xl6BufferSpanEndV1' in text
 inc=out/'sdk/__xl_runtime';inc.mkdir(parents=True)
 for source in (cohort/'src/runtime').glob('*.h'):shutil.copy2(source,inc/source.name);inputs[str(source)]=sha(source)
 case=out/'fixture-inputs/test/cases';case.mkdir(parents=True);shutil.copy2(header,case/header.name)
 sdk=Path(b['base'].get('SdkDirectory') or b['base'].get('RuntimeDirectory') or runtime.parent);includes=['-I'+str(x) for x in (out/'fixture-inputs',out/'sdk',cohort,root,b['basePath'].parent/'include',sdk,sdk/'include')];flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes];warnings=['-Wno-'+x for x in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')];systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
 run('header-C',['gcc',*flags,'-x','c','-fsyntax-only',out/'public.h'])
 for std in ('c++11','c++17'):run('header-'+std,['g++','-std='+std,'-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes,'-x','c++','-fsyntax-only',out/'public.h'])
 code=fixture.read_text();reverseLines=code.splitlines()
 for method in ('read','pack'):
  indices=[i for i,line in enumerate(reverseLines) if 'extern ' in line and 'func '+method+'(' in line];assert len(indices)==4
  values=[reverseLines[i] for i in indices][::-1]
  for i,value in zip(indices,values):reverseLines[i]=value
 reverse=out/'reverse.xl';reverse.write_text('\n'.join(reverseLines)+'\n');reverseC=out/'reverse.c';run('reverse-cgen',[driver,reverse,'cgen-default',reverseC]);run('reverse-ir',[driver,reverse,'ir-lowered',out/'reverse.json'])
 for opt in ('O0','O2'):
  exe=out/(opt+('.exe' if windows else '.out'));run(opt+'-build',['gcc',*flags,*warnings,'-'+opt,generated,native,host,*b['objects'],runtime,*systems,'-o',exe]);run(opt+'-run',[exe])
  exe=out/('ir-'+opt+('.exe' if windows else '.out'));run('ir-'+opt+'-build',['gcc',*flags,'-'+opt,gate,*b['objects'],runtime,*systems,'-o',exe]);run('ir-'+opt+'-run',[exe,fixture])
  exe=out/('reverse-'+opt+('.exe' if windows else '.out'));run('reverse-'+opt+'-build',['gcc',*flags,*warnings,'-'+opt,reverseC,native,host,*b['objects'],runtime,*systems,'-o',exe]);run('reverse-'+opt+'-run',[exe])
 probe=next(x for x in code.splitlines() if 'func probe(' in x);write=next(x for x in code.splitlines() if 'func write(' in x);read=next(x for x in code.splitlines() if 'func read(string' in x)
 negatives={
  'nullable-string':code.replace(probe,probe.replace('Buffer? data','string? data')),
  'nullable-bytes':code.replace(probe,probe.replace('Buffer? data','bytes? data')),
  'nullable-scalar':code.replace(probe,probe.replace('Buffer? data','int? data')),
  'nullable-record':code.replace(probe,probe.replace('Buffer? data','Pod? data')),
  'writable-string':code.replace(write,write.replace('Buffer? data','string data')),
  'writable-bytes':code.replace(write,write.replace('Buffer? data','bytes data')),
  'different-read-count':code.replace(read,read.replace('"size_t"','"int"')),
  'different-read-element':code.replace(read,read.replace('"const void*"','"const char*"')),
  'different-read-effect':code.replace(read,read.replace('@no_raise','@pure @no_raise')),
  'different-read-header':code.replace(read,read.replace('"size_t")','"size_t","other.h")'))}
 negatives['pod-is-not-byte-family']=code.replace(read,read+'\n'+read.replace('string data','Pod data'))
 for name,bad in negatives.items():
  assert bad!=code;f=out/(name+'.xl');f.write_text(bad);target=out/(name+'.c');run('reject-'+name,[driver,f,'cgen-default',target],False);assert not target.exists()
 original=header.read_text();changes={
  'native-read-count':('const void*,size_t,int*','const void*,int,int*'),
  'native-write-const':('void*,int,int,int*','const void*,int,int,int*'),
  'native-pack-count':('const void* data;size_t count;','const void* data;int count;')}
 for name,(old,new) in changes.items():
  assert old in original;bad=out/('bad-'+name+'/test/cases');bad.mkdir(parents=True);(bad/header.name).write_text(original.replace(old,new));priority=['-I'+str(bad.parents[1])]
  run('reject-'+name,['gcc',*priority,*flags,*warnings,'-fsyntax-only',generated],False)
  run('reject-header-'+name,['g++','-std=c++17','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*priority,*includes,'-x','c++','-fsyntax-only',out/'public.h'],False)
 report.update(complete=True,passed=True,inputsUnchanged=all(sha(x)==y for x,y in inputs.items()));assert report['inputsUnchanged'];save()
if __name__=='__main__':main()
