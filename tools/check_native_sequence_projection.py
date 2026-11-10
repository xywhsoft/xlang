"""Field-only immutable record projection: layout, ownership, C ABI and rejection.

No device, window, audio or network. --diagnostic labels a partial compiler
explicitly; it can discover failures but never qualifies the full compiler.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
from full_compiler_qualification import verify_full_compiler

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','compiler','compiler-report','out'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--sdk',type=Path);p.add_argument('--diagnostic',action='store_true')
 p.add_argument('--ir-checker',type=Path);p.add_argument('--ir-report',type=Path);a=p.parse_args()
 assert (a.ir_checker is None)==(a.ir_report is None)
 root,compiler,meta,out=[v.resolve() for v in (a.root,a.compiler,a.compiler_report,a.out)];sdk=a.sdk.resolve() if a.sdk else compiler.parent/'include'
 sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest();metadata=json.loads(meta.read_text());assert metadata['passed']
 full=bool(metadata.get('complete') and metadata.get('fullTccXrtAndVfs'));assert full or a.diagnostic
 if full:metadata=verify_full_compiler(meta,compiler)
 cc=Path(shutil.which('gcc')).resolve();cpp=Path(shutil.which('g++')).resolve();objdump=Path(shutil.which('objdump')).resolve()
 fixture=root/'test/cases/native_sequence_projection.xl';source=fixture.read_text(encoding='utf8');files=[Path(__file__).resolve(),fixture,fixture.with_suffix('.c'),fixture.with_suffix('.h'),meta,compiler,cc,cpp,objdump]
 files.append(Path(__file__).with_name('full_compiler_qualification.py'))
 if full:files.append(Path(metadata['sourceFreeze']))
 regressions=('native_sequence','native_sequence_contract','foreign_pack','foreign_pack_pod','foreign_array_projection','foreign_value_projection','foreign_decl_static_callback','foreign_nested_pack_null','foreign_null_compat','foreign_nullable_span')
 files.extend(root/'test/cases'/(name+'.xl') for name in regressions)
 files.append(root/'test/cases/native_sequence_contract.h')
 files.extend(root/'test/cases'/('native_sequence_projection_nested'+v) for v in ('.xl','.h'))
 if a.ir_checker:
  ir=a.ir_checker.resolve();ir_report=a.ir_report.resolve();gate=json.loads(ir_report.read_text())
  assert gate['passed'] and gate['inputsUnchanged']
  freeze=json.loads(Path(metadata['sourceFreeze']).read_text())
  for name,digest in freeze['overrides'].items():assert gate['inputs'][str(ir.parent/'override'/name)]==digest,name
  files.extend([ir,ir_report,root/'test/cases/native_sequence_projection_ir.c'])
 files.extend(v for v in sdk.rglob('*') if v.is_file())
 for name in ('xl.dll','xlc.dll') if os.name=='nt' else ('libxl.so','libxlc.so'):files.append(compiler.parent/name)
 inputs={str(v):sha(v) for v in files};out.mkdir(parents=True,exist_ok=False)
 report=dict(passed=False,scope=__doc__,fullCompilerBuild=full,diagnostic=not full,shippingDelivery=False,networkTests=False,inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 def run(name,args,success=True,working=None):
  args=list(map(str,args));env=dict(os.environ,XLANG_PROJECTION_ROOT=str(root));env.pop('XLANG_PACKAGE_PATH',None)
  result=subprocess.run(args,cwd=working or compiler.parent,env=env,capture_output=True,timeout=180);log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
  report['rows'].append(dict(name=name,command=args,workingDirectory=str(working or compiler.parent),expectedSuccess=success,exitCode=result.returncode,logSha256=sha(log)));save();print(name,result.returncode,flush=True)
  assert (not result.returncode)==success,log;return result
 cfile,header=out/'generated.c',out/'generated.h';includes=['--inc-path',root,'--inc-path',sdk]
 run('emit',[compiler,'cgen',fixture,'--emit',cfile,'--emit-header',header]);c=cfile.read_text(encoding='utf8')
 assert 'sizeof(NativeProjectionRow)' in c and 'sizeof(NativeProjectionPacked)' in c and 'XL_Projection_' in c
 assert '.actualValue' in c and '.inner.count' in c and not re.search(r'\.(?:secret|callback)\b',c)
 run('jit',[compiler,'run',fixture,*includes]);binary=out/('projection.exe' if os.name=='nt' else 'projection')
 run('static-build',[compiler,'build',fixture,'-o',binary,*includes]);r=run('static-run',[binary]);assert b'native record projection passed' in r.stdout
 if os.name=='nt':
  r=run('imports',[objdump,'-p',binary]);names=re.findall(rb'DLL Name:\s*(\S+)',r.stdout)
  assert names and not {b'xl.dll',b'xlc.dll',b'libtcc.dll'}.intersection(v.lower() for v in names)
 probe=out/'probe.c';probe.write_text('#include "'+header.as_posix()+'"\nint main(void){return 0;}\n')
 flags=['-Wall','-Wextra','-Werror','-Wno-unused-function','-I'+str(root),'-I'+str(sdk)]
 for lang,std,tool in [('c','c11',cc),('c++','c++11',cpp),('c++','c++17',cpp)]:run('header-'+std,[tool,'-x',lang,'-std='+std,*flags,'-fsyntax-only',probe])
 if a.ir_checker:
  r=run('sealed-ir',[ir,fixture]);match=re.search(rb'Read-only record projection IR: (\d+) mutations',r.stdout)
  assert match and int(match[1])>=24;report['irMutations']=int(match[1])
 # The exact record/sequence path and the pre-existing descriptor/array
 # contracts remain separate representations, not implicit projections.
 for name in regressions:run('regression-'+name,[compiler,'cgen',root/'test/cases'/(name+'.xl'),'--emit',out/(name+'.c')])
 run('exact-sequence-jit',[compiler,'run',root/'test/cases/native_sequence.xl',*includes])
 ordinary=out/('exact-sequence.exe' if os.name=='nt' else 'exact-sequence')
 run('exact-sequence-build',[compiler,'build',root/'test/cases/native_sequence.xl','-o',ordinary,*includes]);run('exact-sequence-run',[ordinary])
 nested=root/'test/cases/native_sequence_projection_nested.xl'
 # Nested virtual source uses its explicit fixture header relative to the
 # process include root, like the existing nested native-header diagnostics.
 run('nested-jit',[compiler,'run',nested,*includes],working=root);nested_binary=out/('nested.exe' if os.name=='nt' else 'nested')
 run('nested-static-build',[compiler,'build',nested,'-o',nested_binary,*includes],working=root);run('nested-static-run',[nested_binary],working=root)
 r=run('nested-imports',[objdump,'-p',nested_binary])
 needed=re.findall(rb'(?:DLL Name:\s*|\bNEEDED\s+)(\S+)',r.stdout)
 assert needed and not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(v.lower() for v in needed)
 cases=out/'cases';cases.mkdir()
 sem={
  'hidden-member':source.replace('auto first=rows[0]','auto first=rows[0]\n auto forbidden=first.secret'),
  'readonly-write':source.replace('auto first=rows[0]','rows[0].value=c_int(77)\n auto first=rows[0]'),
  'pointer-field':source.replace(' c_int value @c_name("actualValue")',' point secret\n c_int value @c_name("actualValue")'),
  'direct-native-value':source+'\nextern "forbidden" @header("test/cases/native_sequence_projection.h") @no_raise func forbidden(Row input)\n',
  'wrong-const-slot':source.replace('@header("test/cases/native_sequence_projection.h") @no_raise @c_const(0) func retain','@header("test/cases/native_sequence_projection.h") @no_raise @c_const(1) func retain'),
  'duplicate-marker':source.replace('@c_projection()','@c_projection() @c_projection()',1),
  'marker-arguments':source.replace('@c_projection()','@c_projection(1)',1),
  'nested-pointer':source.replace(' c_int count',' point privatePointer\n c_int count',1),
  'readonly-nested-write':source.replace('auto first=rows[0]','rows[0].inner.count=c_int(77)\n auto first=rows[0]'),
  'native-nullable-value':source+'\nextern "forbidden" @header("test/cases/native_sequence_projection.h") @no_raise func forbidden(Row? input)\n',
  'exact-embedding':source+'\nextern "NativeExact" struct Exact @header("test/cases/native_sequence_projection.h")\n Inner child\nend\n',
  'suspend-read':'''async func one() as int
 return 1
end
async func consume() as int
 auto rows=NativeSequence::<Row>()
 for row in rows
  auto value=await one()
 end
 return 0
end
'''+source,
 }
 for name,text in sem.items():
  assert text!=source;path=cases/(name+'.xl');path.write_text(text);run(name,[compiler,'cgen',path,'--emit',cases/(name+'.c')],False)
 for name,text in [('exact-record',source.replace(' @c_projection()','')),('wrong-field-type',source.replace('@c_name("actualValue")','@c_name("secret")'))]:
  path=cases/(name+'.xl');path.write_text(text);bad=cases/(name+'.h')
  run(name+'-emit',[compiler,'cgen',path,'--emit',cases/(name+'.c'),'--emit-header',bad])
  run(name+'-proof',[cc,'-std=c11',*flags,'-fsyntax-only','-x','c',bad],False)
 report.update(passed=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()),backingArrayCopies=0,logicalValue='only declared scalar fields, recursively; native stride/alignment independent')
 assert report['inputsUnchanged'];save()

if __name__=='__main__':main()
