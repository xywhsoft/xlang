"""Actual compiler-input receipt checks. Every run owns its directory/logs."""
from pathlib import Path
import argparse,ctypes,hashlib,json,os,subprocess,uuid,datetime,shutil
parser=argparse.ArgumentParser();parser.add_argument('--compiler-root',required=True);parser.add_argument('--work-root',required=True)
args=parser.parse_args();compiler=Path(args.compiler_root).resolve();source=Path(__file__).resolve().parents[1]
work=Path(args.work_root).resolve()/('build-inputs-'+str(uuid.uuid4()));work.mkdir(parents=True)
shutil.copy2(__file__,work/'check_build_inputs.py')
shutil.copy2(source/'test/build_input_snapshot_test.c',work/'build_input_snapshot_test.c')
checks=[];calls=[]
def check(label,value):
 checks.append({'label':label,'passed':bool(value)})
 if not value: raise AssertionError(label)
def call(argv,name,input=None,ok=True):
 result=subprocess.run([str(x) for x in argv],cwd=work,input=input,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=90)
 (work/(name+'.log')).write_bytes(result.stdout);calls.append({'name':name,'args':[str(x) for x in argv],'exitCode':result.returncode})
 check(name+' exit',result.returncode==0 if ok else result.returncode!=0);return result
def verify_receipt(name):
 data=json.loads((work/name).read_text(encoding='utf8'))
 check(name+' complete schema',data['schema']==1 and data['complete'] is True and data['scope']=='xlang-native-build-disk-inputs')
 for row in [*data['files'],data['artifact']]:
  p=Path(row['path']);check('bytes '+p.name,p.is_absolute() and p.is_file() and p.stat().st_size==row['size'] and hashlib.sha256(p.read_bytes()).hexdigest()==row['sha256'])
 check(name+' missing candidates',all(not Path(p).exists() for p in data['nativeMissing']))
 return data
try:
 # The unit fixture tests immutable transport independently of compilation.
 unit=work/'snapshot-test.exe'
 call(['gcc','-std=c11','-O0','-w','-ffunction-sections','-fdata-sections','-I'+str(source),'-I'+str(source/'release/include'),'-c',source/'tools/xrt_single_header_impl.c','-o',work/'xrt.o'],'snapshot-runtime-compile')
 call(['gcc','-std=c11','-Wall','-Wextra','-Werror','-Wno-pointer-sign','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-O2','-finput-charset=UTF-8','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(source),'-I'+str(source/'release/include'),'-I'+str(source/'lib/tcc'),'-I'+str(source/'lib/lzma'),source/'test/build_input_snapshot_test.c',source/'src/compile/xl6_build_inputs.c',source/'src/compile/xl6_source_report.c',source/'src/common/xl6_output.c',source/'lib/tcc/tcc_builtin_vfs.c',source/'lib/tcc/tcc_utf8_io.c',source/'lib/lzma/LzmaDec.c',source/'tools/tcc_empty_builtin_resources.c',work/'xrt.o','-lws2_32','-liphlpapi','-lshell32','-luser32','-lgdi32','-lole32','-lwinmm','-lm','-o',unit],'snapshot-compile')
 call([unit],'snapshot-run')
 (work/'root.c').write_text('#define PICK "nested.h"\n#include PICK\nint main(void) {return VALUE;}\n',encoding='utf8')
 inc=work/'headers';inc.mkdir();(inc/'nested.h').write_text('#include "value.h"\n',encoding='utf8');(inc/'value.h').write_text('#define VALUE 7\n',encoding='utf8')
 xl=compiler/'release/xl.exe'
 call([xl,'build','root.c','--inc-path',inc,'-o','c.exe','--input-report','c.json'],'c-build')
 c=verify_receipt('c.json');paths={Path(f['path']).resolve() for f in c['files']}
 check('macro and transitive headers observed',{work/'root.c',inc/'nested.h',inc/'value.h'}<=paths)
 check('failed include search observed',str(work/'nested.h') in c['nativeMissing'])
 result=subprocess.run([str(work/'c.exe')],cwd=work,timeout=15);check('C output executes',result.returncode==7)
 (work/'module.xl').write_text('export func value() as int\n return 9\nend\n',encoding='utf8')
 (work/'entry.xl').write_text('import module\nfunc main(...) as int\n return module.value()\nend\n',encoding='utf8')
 call([xl,'build','entry.xl','-o','x.exe','--input-report','x.json'],'xlang-build')
 x=verify_receipt('x.json');check('transitive xlang source observed',work/'module.xl' in {Path(f['path']).resolve() for f in x['files']})
 check('xlang output executes',subprocess.run([str(work/'x.exe')],cwd=work,timeout=15).returncode==9)
 # Bind disk/project/native inputs to the same invocation, with no IDE scan.
 (work/'entry2.xl').write_text('func main(...) as int\n return 0\nend\n',encoding='utf8')
 (work/'helper.c').write_text('#include "headers/value.h"\nint auxiliary(void) {return VALUE;}\n',encoding='utf8')
 (work/'resource.xlon').write_text('{"manifest": {"path": "application.manifest"}, "extension": set[char("你")]}',encoding='utf8')
 (work/'application.manifest').write_text('<?xml version="1.0" encoding="UTF-8"?><assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0"><assemblyIdentity version="1.0.0.0" processorArchitecture="*" name="Input.Test" type="win32"/></assembly>',encoding='utf8')
 (work/'project.xlp').write_text(json.dumps({'format':'xlang-project-1','kind':'exe','entry':'entry2.xl','cSources':['helper.c'],'resources':['resource.xlon']}),encoding='utf8')
 call([xl,'build','project.xlp','-o','mixed.exe','--input-report','mixed.json'],'mixed-build')
 mixed=verify_receipt('mixed.json');paths={Path(f['path']).resolve() for f in mixed['files']}
 check('project/C/resource/asset all observed',{work/'project.xlp',work/'helper.c',inc/'value.h',work/'resource.xlon',work/'application.manifest'}<=paths)
 # A disk object in TCC's native object format participates in linking.
 (work/'library.c').write_text('int linked_value(void) {return 5;}\n',encoding='utf8')
 call([compiler/'build/tmp/tcc_bootstrap.exe','-I'+str(compiler/'lib/tcc/include'),'-c',work/'library.c','-o',work/'library.o'],'library-compile')
 call([xl,'build','root.c','--inc-path',inc,'--link',work/'library.o','-o','linked.exe','--input-report','linked.json'],'linked-build')
 linked=verify_receipt('linked.json');check('linked object bytes observed',work/'library.o' in {Path(f['path']).resolve() for f in linked['files']})
 # API completion cannot manufacture a receipt for a changed graph/artifact.
 dll=ctypes.CDLL(str(compiler/'release/xlc.dll'))
 for name,argtypes,restype in [('xlcCreateContext',[ctypes.c_void_p],ctypes.c_void_p),('xlcBeginInputReport',[ctypes.c_void_p],ctypes.c_int),('xlcAddSourceFile',[ctypes.c_void_p,ctypes.c_char_p],ctypes.c_int),('xlcAddCSourceFile',[ctypes.c_void_p,ctypes.c_char_p],ctypes.c_int),('xlcEmitExe',[ctypes.c_void_p,ctypes.c_char_p],ctypes.c_int),('xlcWriteInputReport',[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_char_p],ctypes.c_int),('xlcDestroyContext',[ctypes.c_void_p],None)]:
  fn=getattr(dll,name);fn.argtypes=argtypes;fn.restype=restype
 previous=os.getcwd();os.chdir(work)
 try:
  def changed_case(label,mutate):
   (inc/'value.h').write_text('#define VALUE 7\n',encoding='utf8');(work/'module.xl').write_text('export func value() as int\n return 9\nend\n',encoding='utf8')
   ctx=dll.xlcCreateContext(None);check(label+' context',bool(ctx))
   try:
    check(label+' begin',dll.xlcBeginInputReport(ctx)==1)
    check(label+' entry',dll.xlcAddSourceFile(ctx,b'entry.xl')==1)
    check(label+' C input',dll.xlcAddCSourceFile(ctx,b'helper.c')==1)
    output=(label+'.exe').encode();receipt=(label+'.json').encode()
    check(label+' actual emit',dll.xlcEmitExe(ctx,output)==1)
    mutate(work/output.decode());check(label+' receipt rejects changed inputs',dll.xlcWriteInputReport(ctx,output,receipt)==0)
    check(label+' no success receipt',not (work/receipt.decode()).exists())
   finally: dll.xlcDestroyContext(ctx)
  changed_case('header-changed',lambda p:(inc/'value.h').write_text('#define VALUE 8\n',encoding='utf8'))
  changed_case('import-changed',lambda p:(work/'module.xl').write_text('export func value() as int\n return 8\nend\n',encoding='utf8'))
  changed_case('artifact-changed',lambda p:p.write_bytes(p.read_bytes()+b'changed'))
  # Companion-C discovery is a resolution input even when the file is absent.
  changed_case('companion-created',lambda p:(work/'module.c').write_text('int newly_added(void) {return 0;}\n',encoding='utf8'))
 finally: os.chdir(previous)
 for n,argv in [('missing-operand',['build','entry2.xl','--input-report']),('wrong-command',['check','entry2.xl','--input-report','wrong.json']),('duplicate',['build','entry2.xl','--input-report','a.json','--input-report','b.json']),('overlap',['build','root.c','--inc-path',inc,'-o',inc/'value.h','--input-report','overlap.json']),('save-temps',['build','entry2.xl','--save-temps','--input-report','save-temps.json']),('emit-header',['build','entry2.xl','--emit-header','other.h','--input-report','emit-header.json']),('stdin-link',['build','entry2.xl','--link','-','--input-report','stdin.json'])]:
  before=(inc/'value.h').read_bytes();call([xl,*argv],n,ok=False);check(n+' input untouched',(inc/'value.h').read_bytes()==before)
except Exception as e:
 error=str(e);print('FAILED:',error)
else: error=None
report={'status':'passed' if error is None else 'failed','error':error,'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source':str(source),'compiler':str(compiler),'directory':str(work),'checks':checks,'calls':calls,'compilerSha256':hashlib.sha256((compiler/'release/xl.exe').read_bytes()).hexdigest(),'testSourceSha256':hashlib.sha256((work/'check_build_inputs.py').read_bytes()).hexdigest(),'snapshotTestSourceSha256':hashlib.sha256((work/'build_input_snapshot_test.c').read_bytes()).hexdigest()}
(work/'evidence.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
print(work,'checks',len(checks),'status',report['status']);raise SystemExit(0 if error is None else 1)
