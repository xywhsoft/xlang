#!/usr/bin/env python3
"""Inspect PE static-TLS layout with GCC CPU fixtures; never emit/run TCC PE."""
import argparse, hashlib, json, re, shutil, struct, subprocess, time
from pathlib import Path
ap=argparse.ArgumentParser()
ap.add_argument('--root',required=True);ap.add_argument('--out',required=True)
ap.add_argument('--cc',required=True);ap.add_argument('--native-object',required=True)
a=ap.parse_args();root=Path(a.root).resolve();out=Path(a.out).resolve()
out.mkdir(parents=True,exist_ok=False);source=out/'source';source.mkdir();inputs=[];steps=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def copy(p,q):
 q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q)
 if sha(p)!=sha(q):raise RuntimeError('Input changed during copy: '+str(p))
 return {'source':str(p),'path':str(q),'sha256':sha(q),'bytes':q.stat().st_size}
for p in sorted((root/'lib/tcc').rglob('*')):
 if p.is_file() and '__pycache__' not in p.parts:
  rel=p.relative_to(root);row=copy(p,source/rel);row['relativePath']=rel.as_posix();inputs.append(row)
for name in ['test/cases/tcc_coff_tls.c','tools/check_tcc_coff_tls.py','docs/tcc-coff-tls.md']:
 p=root/name;row=copy(p,source/name);row['relativePath']=name;inputs.append(row)
cc=Path(shutil.which(a.cc) or a.cc);ar=cc.with_name('ar.exe');objdump=cc.with_name('objdump.exe')
report={'scope':'Windows AMD64 native static-TLS link layout. GCC-built CPU fixture executes exact production COFF/GC/PE prepare/address/relocate/validate pipeline in memory; no TinyCC-produced PE is written, inspected or run. The separate reference image is entirely GCC/MinGW built. Unrelated CRT/native imports in structural cases use inert test stubs, not runtime implementations.','sourceInputs':inputs,'externalInputs':[],'steps':steps,'passed':False}
def run(label,args,cwd=out):
 started=time.monotonic();r=subprocess.run([str(x) for x in args],cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180)
 log=out/(label+'.log');log.write_bytes(r.stdout);row={'label':label,'command':[str(x) for x in args],'exitCode':r.returncode,'seconds':time.monotonic()-started,'log':str(log),'logSha256':sha(log)};steps.append(row)
 if r.returncode:raise RuntimeError(label+' failed; see '+str(log))
 return r.stdout.decode('utf-8',errors='replace')
fixtures={
 'tls.c':'''__thread int tls_value=0x12345678;
__thread int tls_zero;
__thread int tls_align __attribute__((aligned(64)))=17;
extern const unsigned char relative_values[],relative_padding[];
int entry(void){return tls_value+tls_zero+tls_align+relative_values[0]+relative_padding[0];}
int plain_entry(void){return 29;}
''',
 'relative.s':'.section .rdata$before,"dr"\n.globl relative_padding\nrelative_padding: .space 33,1\n.section .rdata$anchor,"dr"\n.globl relative_anchor\nrelative_anchor: .long 0x76543210\n.section .rdata$relative,"dr"\n.globl relative_values\nrelative_values: .secidx relative_anchor\n.secrel32 relative_anchor+3\n',
 'callbacks.c':'''static void a(void*x,unsigned y,void*z){(void)x;(void)y;(void)z;}
static void b(void*x,unsigned y,void*z){(void)x;(void)y;(void)z;__asm__ volatile("nop");}
void (*cb_b)(void*,unsigned,void*) __attribute__((section(".CRT$XLD")))=b;
void (*cb_a)(void*,unsigned,void*) __attribute__((section(".CRT$XLB")))=a;
void (*cb_start)(void*,unsigned,void*) __attribute__((section(".CRT$XLA")))=0;
''',
 'unsupported.c':'void* unused_init __attribute__((section(".CRT$XCU")))=0;\n',
 'dynamic.c':'static void init(void){}\nvoid(*dynamic_init)(void) __attribute__((section(".CRT$XDB")))=init;\n',
 'bad-array.s':'.section .CRT$XLB,"dr"\n.long 1\n'
}
reference=r'''#include <windows.h>
#include <stdint.h>
#include <stdio.h>
__thread int value=0x12345678;
__thread int zero;
__thread int aligned_value __attribute__((aligned(64)))=17;
static int checks,failures;
#define CHECK(x) do{checks++;if(!(x))failures++;}while(0)
static DWORD WINAPI worker(void*p){(void)p;CHECK(value==0x12345678);CHECK(zero==0);CHECK(aligned_value==17);CHECK(((uintptr_t)&aligned_value&63)==0);value=63;zero=7;return 0;}
int main(void){CHECK(value==0x12345678);CHECK(zero==0);CHECK(((uintptr_t)&aligned_value&63)==0);value=37;zero=11;HANDLE h=CreateThread(NULL,0,worker,NULL,0,NULL);CHECK(h!=NULL);if(h){WaitForSingleObject(h,INFINITE);CloseHandle(h);}CHECK(value==37);CHECK(zero==11);printf("GCC_REFERENCE_CHECKS=%d FAILURES=%d\n",checks,failures);return failures?1:0;}
'''
try:
 report['compilerVersion']=run('compiler-version',[cc,'--version'])
 obj=out/'objects';obj.mkdir()
 for name,text in fixtures.items():
  (obj/name).write_text(text,encoding='utf-8')
  run('input-'+Path(name).stem,[cc,'-O2','-ffunction-sections','-fdata-sections','-c',obj/name,'-o',obj/(Path(name).stem+'.o')])
 library=Path(run('find-mingw-library',[cc,'-print-file-name=libmingw32.a']).strip())
 report['externalInputs'].append({'source':str(library),'sha256':sha(library),'bytes':library.stat().st_size})
 names=['lib64_libmingw32_a-'+x+'.o' for x in ['tlssup','tlsthrd','tlsmcrt','tls_atexit']]
 run('extract-real-mingw',[ar,'x',library,*names],obj)
 for name in names:report['externalInputs'].append({'source':str(library)+':'+name,'path':str(obj/name),'sha256':sha(obj/name)})
 report['externalInputs'].append(copy(Path(a.native_object).resolve(),obj/'native-md4c.o'))
 for name in ['tls.o','relative.o','lib64_libmingw32_a-tlssup.o','native-md4c.o']:
  text=run('inspect-'+Path(name).stem,[objdump,'-x','-r',obj/name])
  if name=='native-md4c.o':
   count=text.count('IMAGE_REL_AMD64_SECREL');report['nativeSecrelCount']=count
   if count!=400:raise RuntimeError('Native object identity/SECREL contract changed')
 for opt in ['O0','O2']:
  dest=out/opt;dest.mkdir();exe=dest/'fixture.exe'
  run(opt+'-build',[cc,'-std=c11','-'+opt,'-DTCC_DISABLE_BUILTIN_VFS','-I'+str(source/'lib/tcc'),'-o',exe,source/'test/cases/tcc_coff_tls.c',source/'lib/tcc/tcc_utf8_io.c','-lws2_32'])
  text=run(opt+'-cpu',[exe,obj,source/'lib/tcc']);m=re.search(r'CHECKS=(\d+) FAILURES=(\d+)',text)
  if not m or int(m[2]):raise RuntimeError('CPU fixture count invalid')
  steps[-1]['checks']=int(m[1]);steps[-1]['emittedPeWritten']=False;steps[-1]['emittedPeExecuted']=False
 (out/'gcc-reference.c').write_text(reference,encoding='utf-8')
 run('gcc-reference-build',[cc,'-O2',out/'gcc-reference.c','-o',out/'gcc-reference.exe'])
 text=run('gcc-reference-cpu',[out/'gcc-reference.exe']);m=re.search(r'GCC_REFERENCE_CHECKS=(\d+) FAILURES=(\d+)',text)
 if not m or int(m[2]):raise RuntimeError('GCC reference count invalid')
 steps[-1]['checks']=int(m[1]);steps[-1]['imageProducer']='GCC/MinGW, not TCC'
 run('gcc-reference-objdump',[objdump,'-x','-r',out/'gcc-reference.exe'])
 data=(out/'gcc-reference.exe').read_bytes();pe=struct.unpack_from('<I',data,0x3c)[0]
 optional=pe+24;magic=struct.unpack_from('<H',data,optional)[0]
 if magic!=0x20b:raise RuntimeError('GCC reference is not PE32+')
 rva,size=struct.unpack_from('<II',data,optional+112+8*9)
 if not rva or size!=40:raise RuntimeError('GCC reference lacks real 40-byte TLS directory')
 report['gccReferenceTlsDirectory']={'rva':rva,'size':size,'sha256':sha(out/'gcc-reference.exe')}
 report['frozenInputsUnchanged']=all(sha(Path(x['path']))==x['sha256'] for x in inputs)
 report['sourceInputsStillMatchWorkspace']=all(sha(Path(x['source']))==x['sha256'] for x in inputs)
 report['passed']=report['frozenInputsUnchanged']
except Exception as e:report['error']=str(e)
report['checks']=sum(x.get('checks',0) for x in steps)
(out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'passed':report['passed'],'checks':report['checks'],'report':str(out/'results.json'),'error':report.get('error')}))
raise SystemExit(0 if report['passed'] else 1)
