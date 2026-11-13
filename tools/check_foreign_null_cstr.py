"""Typed NULL aliases for exact explicitly borrowed const-char C-string inputs.

CPU only: direct, bound, protocol and global calls; both declaration orders;
strict authority-header proofs; malformed declarations and sealed IR failures.
The physical business C target is called once; no C forwarding adapters.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from foreign_validation_build import add_build_arguments,load_build
from compiler_response import command_with_response

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','out'):p.add_argument('--'+name,type=Path,required=True)
 add_build_arguments(p);a=p.parse_args();root,out=a.root.resolve(),a.out.resolve();b=load_build(a.build,a.overlay);cohort,driver,runtime=b['cohort'],b['driver'],b['runtime'];sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
 stem='foreign_null_cstr';fixture=root/'test/cases'/(stem+'.xl');header=fixture.with_suffix('.h');native=fixture.with_name(stem+'_native.c');gate=fixture.with_name(stem+'_ir.c');host=root/'test/cases/foreign_decl_module_host.c'
 inputs=dict(b['inputs']);inputs.update((str(x),sha(x)) for x in (Path(__file__),fixture,header,native,gate,host));out.mkdir(parents=True,exist_ok=False)
 env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort));windows=os.name=='nt'
 if windows:shutil.copy2(runtime,out/runtime.name)
 else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
 report=dict(scope=__doc__,complete=False,passed=False,fullSourceRebuild=b['fullSourceRebuild'],buildManifest=b['buildManifest'],overlay=b['overlay'],inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 def run(name,cmd,accepted=True):
  cmd=list(map(str,cmd));actual=command_with_response(cmd,'gcc',out/(name+'.rsp')) if cmd[0] in ('gcc','g++') else cmd;t=time.monotonic();z=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=300);log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr);report['rows'].append(dict(name=name,exitCode=z.returncode,accepted=accepted,seconds=round(time.monotonic()-t,3),logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert (not z.returncode)==accepted,log
 generated=out/'generated.c';run('cgen',[driver,fixture,'cgen-default',generated]);run('ir',[driver,fixture,'ir-lowered',out/'lowered.json']);run('public',[driver,fixture,'emit',out/'public'])
 text=generated.read_text();assert '(const char*)0' in text and 'native_callee_receiver->apply' in text
 inc=out/'sdk/__xl_runtime';inc.mkdir(parents=True)
 for source in (cohort/'src/runtime').glob('*.h'):
  shutil.copy2(source,inc/source.name);inputs[str(source)]=sha(source)
 case=out/'fixture-inputs/test/cases';case.mkdir(parents=True);shutil.copy2(header,case/header.name)
 sdk=Path(b['base'].get('SdkDirectory') or b['base'].get('RuntimeDirectory') or runtime.parent);includes=['-I'+str(x) for x in (out/'fixture-inputs',out/'sdk',cohort,root,b['basePath'].parent/'include',sdk,sdk/'include')];flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes];warnings=['-Wno-'+x for x in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')];systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
 run('header-C',['gcc',*flags,'-x','c','-fsyntax-only',out/'public.h'])
 for std in ('c++11','c++17'):run('header-'+std,['g++','-std='+std,'-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes,'-x','c++','-fsyntax-only',out/'public.h'])
 code=fixture.read_text();lines=code.splitlines();pairs=[i for i,x in enumerate(lines)if 'func text('in x or 'func apply('in x]
 assert len(pairs)==3
 for i in pairs:lines[i],lines[i+1]=lines[i+1],lines[i]
 reverse=out/'reverse.xl';reverse.write_text('\n'.join(lines)+'\n');reverseC=out/'reverse.c';run('reverse-cgen',[driver,reverse,'cgen-default',reverseC]);run('reverse-ir',[driver,reverse,'ir-lowered',out/'reverse.json'])
 for opt in ('O0','O2'):
  for kind,c in (('main',generated),('reverse',reverseC)):
   exe=out/(kind+'-'+opt+('.exe' if windows else '.out'));run(kind+'-'+opt+'-build',['gcc',*flags,*warnings,'-'+opt,c,native,host,*b['objects'],runtime,*systems,'-o',exe]);run(kind+'-'+opt+'-run',[exe])
  exe=out/('ir-'+opt+('.exe' if windows else '.out'));run('ir-'+opt+'-build',['gcc',*flags,'-'+opt,gate,*b['objects'],runtime,*systems,'-o',exe]);run('ir-'+opt+'-run',[exe,fixture])
 negatives={
  'mutable-null':code.replace('"const char*"','"char*"',1),
  'void-null':code.replace('"const char*"','"const void*"',1),
  'unsigned-char-null':code.replace('"const char*"','"const unsigned char*"',1),
  'nullable-string':code.replace('func text(c_int prefix,string value','func text(c_int prefix,string? value',1),
  'view-instead-of-cstr':code.replace('@c_cstr(1)','@c_text_view(1)',1),
  'changed-effect':code.replace('@c_null(2,"const char*")','@pure @c_null(2,"const char*")',1),
  'changed-blocking':code.replace('@c_null(2,"const char*")','@nonblocking @c_null(2,"const char*")',1),
  'changed-header':code.replace('@c_null(2,"const char*")','@header("other.h") @c_null(2,"const char*")',1),
  'changed-output':code.replace('@c_null(2,"const char*") @c_output(4,0)','@c_null(2,"const char*") @c_output(4,7)',1),
  'changed-scalar':code.replace('func clear(c_int prefix,c_int suffix)','func clear(c_float prefix,c_int suffix)',1),
  'receiver-null':code.replace('@c_null(2,"const char*")','@c_null(0,"const char*")',1),
  'output-null':code.replace('@c_null(2,"const char*")','@c_null(4,"const char*")',1),
  'capability-not-null':code.replace('@c_null(2,"const char*")','@c_code_owner(2)',1)}
 for name,bad in negatives.items():
  assert bad!=code;f=out/(name+'.xl');f.write_text(bad);target=out/(name+'.c');run('reject-'+name,[driver,f,'cgen-default',target],False);assert not target.exists()
 original=header.read_text();changes={
  'native-mutable-pointer':('int cstrFree(int prefix,const char* text','int cstrFree(int prefix,char* text'),
  'native-output':('int suffix,int* output','int suffix,float* output')}
 for name,(old,new) in changes.items():
  assert old in original;bad=out/('bad-'+name+'/test/cases');bad.mkdir(parents=True);(bad/header.name).write_text(original.replace(old,new));priority=['-I'+str(bad.parents[1])]
  run('reject-'+name,['gcc',*priority,*flags,*warnings,'-fsyntax-only',generated],False)
  run('reject-header-'+name,['g++','-std=c++17','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*priority,*includes,'-x','c++','-fsyntax-only',out/'public.h'],False)
 report.update(complete=True,passed=True,inputsUnchanged=all(sha(x)==y for x,y in inputs.items()));assert report['inputsUnchanged'];save()
if __name__=='__main__':main()
