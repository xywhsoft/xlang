#!/usr/bin/env python3
"""GCC CPU verification of non-mutating explicit-root JIT archive selection."""
import argparse,hashlib,json,os,re,shutil,struct,subprocess,time
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--root',required=True);ap.add_argument('--out',required=True);ap.add_argument('--cc',default='gcc');ap.add_argument('--tls-inputs');ap.add_argument('--partial-only',action='store_true');a=ap.parse_args()
root=Path(a.root).resolve();out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=False);source=out/'source';source.mkdir();inputs=[];steps=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def copy(p,q):
 q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q);h=sha(q)
 if sha(p)!=h:raise RuntimeError('Input changed during copy: '+str(p))
 return {'source':str(p),'path':str(q),'sha256':h}
for p in sorted((root/'lib/tcc').rglob('*')):
 if p.is_file() and '__pycache__' not in p.parts:
  row=copy(p,source/p.relative_to(root));row['relativePath']=p.relative_to(root).as_posix();inputs.append(row)
for name in ['test/cases/tcc_archive_reachability.c','test/cases/tcc_section_gc.c','test/cases/tcc_coff_tls.c','tools/check_tcc_archive_reachability.py','docs/tcc-archive-reachability.md']:
 row=copy(root/name,source/name);row['relativePath']=name;inputs.append(row)
cc=Path(shutil.which(a.cc) or a.cc);ar=cc.with_name('ar.exe' if os.name=='nt' else 'ar')
if not ar.is_file():ar=Path(shutil.which('ar'))
report={'partialOnly':a.partial_only,'scope':'Explicit-root memory/JIT archive selection only. GCC-built fixture executes production libtcc; no TinyCC-produced PE is written or run. Archive graph queries are non-mutating. AOT/default/noGC selection is observed before relocation, without emitted images.','sourceInputs':inputs,'steps':steps,'passed':False}
def run(label,args,cwd=out):
 t=time.monotonic();p=subprocess.run([str(x) for x in args],cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180);log=out/(label+'.log');log.write_bytes(p.stdout);steps.append({'label':label,'command':[str(x) for x in args],'exitCode':p.returncode,'seconds':time.monotonic()-t,'log':str(log),'logSha256':sha(log)})
 if p.returncode:raise RuntimeError(label+' failed: '+str(log))
 return p.stdout.decode('utf-8',errors='replace')
fixtures={
 'primary.c':'extern int live_arch(void),dead_arch(void);int entry(void){return live_arch();}int dead_primary(void){return dead_arch();}\n',
 'live.c':'extern int next_arch(void),ctor_arch(void);int ctor_count;static int(*volatile cb)(void)=next_arch;__attribute__((constructor)) static void setup(void){ctor_count=ctor_arch();}int live_arch(void){return cb()+ctor_count;}\n',
 'next.c':'int next_arch(void){return 36;}\n','init.c':'int ctor_arch(void){return 7;}\n',
 'tls.c':'__thread int blocked_tls=99;int dead_arch(void){return blocked_tls;}\n#ifdef _WIN32\nstatic void cb(void*a,unsigned b,void*c){(void)a;(void)b;(void)c;}\nvoid(*tls_callback)(void*,unsigned,void*) __attribute__((section(".CRT$XLB")))=cb;\n#endif\n'}
try:
 report['compilerVersion']=run('compiler-version',[cc,'--version'])
 obj=out/'objects';obj.mkdir()
 for name,text in fixtures.items():
  (obj/name).write_text(text,encoding='utf-8');run('input-'+Path(name).stem,[cc,'-O2','-ffunction-sections','-fdata-sections','-c',obj/name,'-o',obj/(Path(name).stem+'.o')])
 for name,members in [('dependencies.a',['tls.o','init.o','next.o','live.o']),('all.a',['tls.o','init.o','next.o','live.o','primary.o'])]:run('archive-'+name,[ar,'rcs',obj/name,*[obj/x for x in members]])
 def archive(members):
  data=b'!<arch>\n'
  for name,payload in members:
   header=(name.ljust(16)+'0'.ljust(12)+'0'.ljust(6)+'0'.ljust(6)+'100644'.ljust(8)+str(len(payload)).ljust(10)+'`\n').encode('ascii')
   assert len(header)==60
   data+=header+payload+(b'\n' if len(payload)&1 else b'')
  return data
 short_import=struct.pack('<HHHHIII',0,65535,0,0x8664,0,0,0)
 (obj/'short-import.a').write_bytes(archive([('import.o/',short_import)]))
 (obj/'short-metadata.a').write_bytes(archive([('/',short_import),('//',short_import),('/SYM64/',short_import),('tiny.bin/',b'\x64\x86'+b'\0'*8)]))

 wrapper=source/'test/cases/tcc_gc_jit_subset.c'
 wrapper.write_text('#define main unused_full_fixture_main\n#include "tcc_section_gc.c"\n#undef main\nint main(int argc,char**argv){if(argc!=2)return 2;library_root=argv[1];check(tcc_native_link_features()==15u,"feature bits");test_jit(0);test_jit(1);test_errors();test_registry_roots();test_limits();printf("GC_JIT_CHECKS=%d FAILURES=%d\\n",checks,failures);return failures?1:0;}\n',encoding='utf-8')
 report['generatedHarness']={'path':str(wrapper),'sha256':sha(wrapper),'calls':'existing test_jit/test_errors/test_registry_roots/test_limits only; no PE-output tests'}
 tls=None
 if a.tls_inputs:
  if os.name!='nt':raise RuntimeError('TLS inputs are only for the native Windows fixture')
  tls=out/'tls-inputs';tls.mkdir();report['tlsInputs']=[]
  for p in sorted(Path(a.tls_inputs).glob('*.o')):report['tlsInputs'].append(copy(p,tls/p.name))
 for opt in ['O0','O2']:
  dest=out/opt;dest.mkdir()
  tests=[('archive','tcc_archive_reachability.c',[obj,source/'lib/tcc']+(['--partial-only'] if a.partial_only else []),r'ARCHIVE_CHECKS=(\d+) FAILURES=(\d+)')]
  if not a.partial_only:tests += [('gc-jit','tcc_gc_jit_subset.c',[source/'lib/tcc'],r'GC_JIT_CHECKS=(\d+) FAILURES=(\d+)')]+([('tls','tcc_coff_tls.c',[tls,source/'lib/tcc'],r'CHECKS=(\d+) FAILURES=(\d+)')] if tls else [])
  for label,filename,arguments,pattern in tests:
   exe=dest/(label+('.exe' if os.name=='nt' else ''));cmd=[cc,'-std=c11','-'+opt,'-DTCC_DISABLE_BUILTIN_VFS','-I'+str(source/'lib/tcc'),'-o',exe,source/'test/cases'/filename]
   cmd+=([source/'lib/tcc/tcc_utf8_io.c','-lws2_32'] if os.name=='nt' else ['-ldl','-lpthread','-lm'])
   run(opt+'-'+label+'-build',cmd);text=run(opt+'-'+label+'-cpu',[exe,*arguments]);m=re.search(pattern,text)
   if not m or int(m[2]):raise RuntimeError('Invalid fixture result: '+label)
   steps[-1]['checks']=int(m[1]);steps[-1]['emittedPeWritten']=False;steps[-1]['emittedPeExecuted']=False
 report['frozenInputsUnchanged']=all(sha(Path(x['path']))==x['sha256'] for x in inputs)
 report['sourceInputsStillMatchWorkspace']=all(sha(Path(x['source']))==x['sha256'] for x in inputs)
 report['passed']=report['frozenInputsUnchanged']
except Exception as e:report['error']=str(e)
report['checks']=sum(x.get('checks',0) for x in steps)
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps({'passed':report['passed'],'checks':report['checks'],'report':str(out/'results.json'),'error':report.get('error')}));raise SystemExit(0 if report['passed'] else 1)
