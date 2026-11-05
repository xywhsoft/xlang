"""CPU-only NativeSequence source, sealed IR and exact ABI acceptance.

Runs an actual full compiler or an explicitly pinned development overlay.
Two small independent declarations supply int/size_t producer rows to the
mutation gate. Actual Native Shape execution has its own dedicated runner.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from compiler_response import command_with_response
from foreign_validation_build import add_build_arguments,load_build

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for key in ('root','packages','out'):p.add_argument('--'+key,type=Path,required=True)
 add_build_arguments(p);a=p.parse_args();root,packages,out=[x.resolve() for x in (a.root,a.packages,a.out)]
 build=load_build(a.build,a.overlay);source=build['cohort'];runtime=build['runtime'];base=build['base']
 sha=lambda value:hashlib.sha256(Path(value).read_bytes()).hexdigest()
 case=root/'test/cases/native_sequence.xl';host=root/'test/cases/foreign_decl_module_host.c'
 ir=root/'test/cases/native_sequence_ir.c';shape=root/'test/cases/native_sequence_contract.xl'
 cc=shutil.which('gcc');cxx=shutil.which('g++');assert cc and cxx
 inputs=dict(build['inputs']);inputs.update((str(x),sha(x)) for x in (case,host,ir,shape,root/'test/cases/native_sequence_contract.h',Path(__file__),Path(cc),Path(cxx)))
 inputs.update((str(x),sha(x)) for x in packages.rglob('*') if x.is_file())
 out.mkdir(parents=True,exist_ok=False);(out/'tmp').mkdir();headers=out/'sdk/__xl_runtime';headers.mkdir(parents=True)
 for header in (source/'src/runtime').glob('*.h'):inputs[str(header)]=sha(header);shutil.copy2(header,headers/header.name)
 env=dict(os.environ,XLANG_PACKAGE_PATH=str(packages),XL6_SOURCE_NATIVE_ROOT=str(source),TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
 if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
 else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
 report=dict(complete=False,passed=False,scope=__doc__,fullSourceRebuild=build['fullSourceRebuild'],buildManifest=build['buildManifest'],overlay=build['overlay'],inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 def run(name,command,success=True):
  command=list(map(str,command));actual=command_with_response(command,cc,out/(name+'.rsp')) if command[0] in (cc,cxx) else command
  start=time.monotonic();z=subprocess.run(actual,cwd=source,env=env,capture_output=True,timeout=300);log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
  report['rows'].append(dict(name=name,command=actual,exitCode=z.returncode,expectedSuccess=success,logSha256=sha(log),seconds=round(time.monotonic()-start,3)));save();print(name,z.returncode,flush=True)
  assert (z.returncode==0)==success,log
 generated=out/'generated.c';run('cgen',[build['driver'],case,'cgen-default',generated]);run('ir',[build['driver'],case,'ir-lowered',out/'lowered.json']);run('header',[build['driver'],case,'emit',out/'public'])
 run('contract-cgen',[build['driver'],shape,'cgen-default',out/'contract.c']);run('contract-ir',[build['driver'],shape,'ir-lowered',out/'contract.json']);run('contract-header',[build['driver'],shape,'emit',out/'contract-public'])
 sdk=Path(base.get('SdkDirectory') or base.get('RuntimeDirectory') or runtime.parent)
 includes=['-I'+str(x) for x in (out/'sdk',source,build['basePath'].parent/'include',sdk,sdk/'include',root)]
 flags=['-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes]
 run('header-C',[cc,'-std=c11',*flags,'-x','c','-fsyntax-only',out/'public.h'])
 for std in ('c++11','c++17'):run('header-'+std,[cxx,'-std='+std,*flags,'-x','c++','-fsyntax-only',out/'public.h'])
 warning=['-Wno-'+name for name in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')]
 run('contract-C',[cc,'-std=c11',*flags,*warning,'-fsyntax-only',out/'contract.c'])
 for std in ('c++11','c++17'):run('contract-'+std,[cxx,'-std='+std,*flags,'-x','c++','-fsyntax-only',out/'contract-public.h'])
 native=(root/'test/cases/native_sequence_contract.h').read_text()
 for name,before,after in (('int-count','const int**,int*','const int**,size_t*'),('size-count','const SequenceContractItem**,size_t*','const SequenceContractItem**,int*'),('pointee','const int**,int*','const double**,int*')):
  assert before in native;bad=out/('bad-'+name+'/test/cases');bad.mkdir(parents=True)
  (bad/'native_sequence_contract.h').write_text(native.replace(before,after))
  run('reject-header-'+name,[cc,'-I'+str(bad.parents[1]),'-std=c11',*flags,*warning,'-fsyntax-only',out/'contract.c'],False)
 systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
 for opt in ('O0','O2'):
  exe=out/(opt+('.exe' if os.name=='nt' else '.out'))
  run(opt+'-build',[cc,'-std=c11',*flags,*warning,'-'+opt,generated,host,*build['objects'],runtime,*systems,'-o',exe]);run(opt+'-run',[exe])
  exe=out/('sealed-'+opt+('.exe' if os.name=='nt' else '.out'))
  run(opt+'-sealed-build',[cc,'-std=c11',*flags,'-'+opt,ir,*build['objects'],runtime,*systems,'-o',exe]);run(opt+'-sealed-run',[exe,shape])
 negatives={}
 for element in ('int','char','string','point','c_int?'):
  negatives['element-'+element.replace('?','nullable')]='func main(...) as int\n auto seq=NativeSequence::<'+element+'>()\n return 0\nend\n'
 negatives['write-index']='func main(...) as int\n auto seq=NativeSequence::<c_int>()\n seq[0]=c_int(1)\n return 0\nend\n'
 for family in ('array','list','dict','set','Stack','SlotMap','channel','future','generator'):
  negatives['mutable-'+family]='func consume('+family+'<NativeSequence<c_int>> values)\nend\nfunc main(...) as int\n return 0\nend\n'
  negatives['mutable-ctor-'+family]='func main(...) as int\n auto values='+family+'::<NativeSequence<c_int>>()\n return 0\nend\n'
 negatives['mutable-nested']='func consume(array<list<NativeSequence<c_int>>> values)\nend\nfunc main(...) as int\n return 0\nend\n'
 negatives['future-result']='async func produce() as NativeSequence<c_int>\n return NativeSequence::<c_int>()\nend\nfunc main(...) as int\n return 0\nend\n'
 for suffix in ('=c_int(1)','+=c_int(1)'):
  negatives['write-field-'+('add' if suffix[0]=='+' else 'set')]=shape.read_text().replace('copied.value=c_int(99)','second[0].value'+suffix)
 negatives['await-live']='async func one() as int\n return 1\nend\nasync func run() as int\n auto seq=NativeSequence::<c_int>()\n for value in seq\n  int n=await one()\n end\n return 0\nend\nfunc main(...) as int\n return 0\nend\n'
 negatives['yield-live']='func run() as generator<c_int>\n auto seq=NativeSequence::<c_int>()\n for value in seq\n  yield value\n end\nend\nfunc main(...) as int\n return 0\nend\n'
 for name,text in negatives.items():
  fixture=out/(name+'.xl');fixture.write_text(text);run('reject-'+name,[build['driver'],fixture,'cgen-default',out/(name+'.c')],False)
 report['inputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items());assert report['inputsUnchanged'];report.update(complete=True,passed=True);save()

if __name__=='__main__':main()
