"""Closed nested descriptor packs and typed fixed NULL slots, CPU only.

Records Cgen/IR, exact C/C++ headers, O0/O2 execution, malformed declarations,
sealed-IR mutations and poisoned Native headers. No UI/provider/device runs.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from foreign_validation_build import add_build_arguments,load_build
from compiler_response import command_with_response

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','out'):p.add_argument('--'+n,type=Path,required=True)
 add_build_arguments(p);a=p.parse_args();root,out=a.root.resolve(),a.out.resolve();b=load_build(a.build,a.overlay);cohort=b['cohort'];driver=b['driver'];runtime=b['runtime'];sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
 stem='foreign_nested_pack_null';fixture=root/'test/cases'/(stem+'.xl');header=fixture.with_suffix('.h');native=fixture.with_name(stem+'_native.c');gate=fixture.with_name(stem+'_ir.c');host=root/'test/cases/foreign_decl_module_host.c'
 inputs=dict(b['inputs']);inputs.update((str(x),sha(x)) for x in [Path(__file__),fixture,header,native,gate,host]);out.mkdir(parents=True,exist_ok=False)
 env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort));windows=os.name=='nt'
 if windows:shutil.copy2(runtime,out/runtime.name)
 else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
 report=dict(scope=__doc__,complete=False,passed=False,fullSourceRebuild=b['fullSourceRebuild'],buildManifest=b['buildManifest'],overlay=b['overlay'],inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 def run(name,cmd,accepted=True):
  cmd=list(map(str,cmd));actual=command_with_response(cmd,'gcc',out/(name+'.rsp')) if cmd[0] in ('gcc','g++') else cmd;t=time.monotonic();z=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=300);log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr);report['rows'].append(dict(name=name,exitCode=z.returncode,accepted=accepted,seconds=round(time.monotonic()-t,3),logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert (not z.returncode)==accepted,log
 generated=out/'generated.c';run('cgen',[driver,fixture,'cgen-default',generated]);run('ir',[driver,fixture,'ir-lowered',out/'lowered.json']);run('public',[driver,fixture,'emit',out/'public'])
 text=generated.read_text();assert 'native nested descriptor tagged struct' in text and 'sizeof(NestedView)' in text and 'sizeof(NestedRenderer)' in text and '(const int*)0' in text
 case=out/'fixture-inputs/test/cases';case.mkdir(parents=True);shutil.copy2(header,case/header.name)
 sdk=Path(b['base'].get('SdkDirectory') or b['base'].get('RuntimeDirectory') or runtime.parent);includes=['-I'+str(x) for x in (out/'fixture-inputs',cohort,root,b['basePath'].parent/'include',sdk,sdk/'include')];flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes];warnings=['-Wno-'+x for x in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')];systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
 run('header-C',['gcc',*flags,'-x','c','-fsyntax-only',out/'public.h'])
 for std in ('c++11','c++17'):run('header-'+std,['g++','-std='+std,'-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes,'-x','c++','-fsyntax-only',out/'public.h'])
 for opt in ('O0','O2'):
  exe=out/(opt+('.exe' if windows else '.out'));run(opt+'-build',['gcc',*flags,*warnings,'-'+opt,generated,native,host,*b['objects'],runtime,*systems,'-o',exe]);run(opt+'-run',[exe])
  exe=out/('ir-'+opt+('.exe' if windows else '.out'));run('ir-'+opt+'-build',['gcc',*flags,'-'+opt,gate,*b['objects'],runtime,*systems,'-o',exe]);run('ir-'+opt+'-run',[exe,fixture])
 code=fixture.read_text();negatives={
  'no-parent':code.replace('@c_pack_record(1,"view","NestedView")','',1),
  'missing-middle':code.replace('@c_pack_record(1,"view.renderer","NestedRenderer")','',1),
  'path-injection':code.replace('"view.renderer.text"','"view->renderer.text"',1),
  'empty-member':code.replace('"view.renderer.text"','"view..renderer.text"',1),
  'wrong-size-target':code.replace('"view.renderer.size","uint32_t","view.renderer"','"view.renderer.size","uint32_t","view"',1),
  'missing-size-target':code.replace('"view.size","uint32_t","view"','"view.size","uint32_t"',1),
  'pointer-witness':code.replace('"view.renderer","NestedRenderer"','"view.renderer","NestedRenderer*"',1),
  'duplicate-witness':code.replace('@c_pack_record(1,"view","NestedView")','@c_pack_record(1,"view","NestedView") @c_pack_record(1,"view","NestedView")',1),
  'receiver-null':code.replace('@c_null(2,"const int*")','@c_null(0,"const int*")',1),
  'output-null':code.replace('@c_null(2,"const int*")','@c_null(3,"int*")',1),
  'descriptor-null':code.replace('@c_null(2,"const int*")','@c_null(1,"const NestedOuter*")',1),
  'duplicate-null':code.replace('@c_null(2,"const int*")','@c_null(2,"const int*") @c_null(2,"const int*")',1),
  'unreachable-null':code.replace('@c_null(2,"const int*")','@c_null(63,"const int*")',1),
  'nonpointer-null':code.replace('@c_null(2,"const int*")','@c_null(2,"int")',1),
  'function-null':code.replace('@c_null(2,"const int*")','@c_null(2,"int(*)(int)")',1),
  'expression-null':code.replace('@c_null(2,"const int*")','@c_null(2,"int*);evil()")',1),
  'missing-header':code.replace('@header("test/cases/foreign_nested_pack_null.h")',''),
  'span-split':code.replace('@c_output(3,0) @c_null(2,"const int*") func spanNull','@c_output(3,0) @c_null(1,"const int*") func spanNull',1),
  'no-effect-contract':code.replace('extern "nested_null_only" @header("test/cases/foreign_nested_pack_null.h") @no_raise','extern "nested_null_only" @header("test/cases/foreign_nested_pack_null.h")',1)}
 for name,bad in negatives.items():
  assert bad!=code;f=out/(name+'.xl');f.write_text(bad);target=out/(name+'.c');run('reject-'+name,[driver,f,'cgen-default',target],False);assert not target.exists()
 original=header.read_text();changes={
  'union-intermediate':('typedef struct NestedRenderer {','typedef union NestedRenderer {'),
  'pointer-intermediate':('NestedRenderer renderer;','NestedRenderer* renderer;'),
  'size-type':('uint32_t size;NestedRenderer','float size;NestedRenderer'),
  'view-field-type':('NestedRoot* source;','void* source;'),
  'null-physical-type':('const int*,int*','const float*,int*')}
 for name,(old,new) in changes.items():
  assert old in original;bad=out/('bad-'+name+'/test/cases');bad.mkdir(parents=True);(bad/header.name).write_text(original.replace(old,new));priority=['-I'+str(bad.parents[1])]
  run('reject-native-'+name,['gcc',*priority,*flags,*warnings,'-fsyntax-only',generated],False)
  run('reject-header-'+name,['g++','-std=c++17','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*priority,*includes,'-x','c++','-fsyntax-only',out/'public.h'],False)
 bad=out/'bad-nontrivial/test/cases';bad.mkdir(parents=True);(bad/header.name).write_text(original.replace('typedef struct NestedFonts {','typedef struct NestedFonts {\n#ifdef __cplusplus\n~NestedFonts() {}\n#endif\n'));run('reject-nontrivial',['g++','-std=c++17','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(bad.parents[1]),*includes,'-x','c++','-fsyntax-only',out/'public.h'],False)
 report.update(complete=True,passed=True,inputsUnchanged=all(sha(x)==y for x,y in inputs.items()));assert report['inputsUnchanged'];save()
if __name__=='__main__':main()
