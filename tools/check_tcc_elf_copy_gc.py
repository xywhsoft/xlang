#!/usr/bin/env python3
"""Linux CPU regression for late ELF COPY storage after empty-BSS GC."""
import argparse,hashlib,json,os,re,shutil,struct,subprocess,time
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--root',required=True);ap.add_argument('--out',required=True);ap.add_argument('--cc',default='gcc');ap.add_argument('--libc',default='/lib/x86_64-linux-gnu/libc.so.6');a=ap.parse_args()
assert os.name!='nt','This fixture emits/runs ELF only, never PE'
root=Path(a.root).resolve();out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=False);source=out/'source';source.mkdir();inputs=[];steps=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for p in sorted((root/'lib/tcc').rglob('*')):
 if p.is_file() and '__pycache__' not in p.parts:
  rel=p.relative_to(root);q=source/rel;q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q);h=sha(q);assert h==sha(p);inputs.append({'path':rel.as_posix(),'sha256':h})
for name in ['test/cases/tcc_elf_copy_gc.c','tools/check_tcc_elf_copy_gc.py']:
 p=root/name;q=source/name;q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q);inputs.append({'path':name,'sha256':sha(q)})
libc=Path(a.libc).resolve();cc=shutil.which(a.cc) or a.cc
report={'scope':'Linux ELF COPY storage after final GC. GCC-built CPU host emits tiny ELF-only no-own-BSS executable; readelf/ELF PT_LOAD checks plus actual loader/run verify stdout/stderr COPY targets. No PE generation, UI, GPU or network.','sourceInputs':inputs,'libc':{'path':str(libc),'sha256':sha(libc)},'steps':steps,'passed':False}
def run(label,args):
 t=time.monotonic();p=subprocess.run([str(x) for x in args],cwd=out,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180);log=out/(label+'.log');log.write_bytes(p.stdout);steps.append({'label':label,'command':[str(x) for x in args],'exitCode':p.returncode,'seconds':time.monotonic()-t,'log':str(log),'logSha256':sha(log)})
 if p.returncode:raise RuntimeError(label+' failed: '+str(log))
 return p.stdout.decode(errors='replace')
try:
 run('compiler-version',[cc,'--version'])
 assembly=out/'startup.s';assembly.write_text('.text\n.globl _start\n.type _start,@function\n_start:\n xor %ebp,%ebp\n andq $-16,%rsp\n call main\n movl %eax,%edi\n call _Exit\n.size _start,.-_start\n.section .note.GNU-stack,"",@progbits\n')
 run('startup-object',[cc,'-c',assembly,'-o',out/'startup.o'])
 for opt in ['O0','O2']:
  d=out/opt;d.mkdir();host=d/'host';program=d/'program'
  run(opt+'-host-build',[cc,'-std=c11','-'+opt,'-DTCC_DISABLE_BUILTIN_VFS','-I'+str(source/'lib/tcc'),'-o',host,source/'test/cases/tcc_elf_copy_gc.c','-ldl','-pthread','-lm'])
  text=run(opt+'-emit',[host,source/'lib/tcc',out/'startup.o',libc,program]);m=re.search(r'ELF_COPY_CHECKS=(\d+) FAILURES=(\d+)',text)
  if not m or int(m[2]):raise RuntimeError('Host assertion result missing')
  steps[-1]['checks']=int(m[1])
  decoded=run(opt+'-inspect',['readelf','-W','-r','-S','-l',program]);targets=[int(x,16) for x in re.findall(r'^([0-9a-f]{16})\s+\S+\s+R_X86_64_COPY\b',decoded,re.M)]
  if len(targets)!=2:raise RuntimeError('Expected two actual ELF COPY relocations')
  data=program.read_bytes();head=struct.unpack_from('<16sHHIQQQIHHHHHH',data,0);phoff,phentsize,phnum=head[5],head[9],head[10];loads=[]
  for i in range(phnum):
   kind,flags,offset,vaddr,paddr,filesz,memsz,align=struct.unpack_from('<IIQQQQQQ',data,phoff+i*phentsize)
   if kind==1 and flags&2:loads.append((vaddr,vaddr+memsz))
  if not all(any(lo<=target and target+8<=hi for lo,hi in loads) for target in targets):raise RuntimeError('COPY target outside writable PT_LOAD')
  steps[-1]['checks']=3;steps[-1]['copyTargets']=targets;steps[-1]['writableLoadRanges']=loads
  result=run(opt+'-standalone',[program]);assert 'COPY stdout' in result and 'COPY stderr' in result
  steps[-1]['checks']=2;steps[-1]['elfSha256']=sha(program)
 report['frozenInputsUnchanged']=all(sha(source/x['path'])==x['sha256'] for x in inputs);report['sourceInputsStillMatchWorkspace']=all(sha(root/x['path'])==x['sha256'] for x in inputs);report['passed']=report['frozenInputsUnchanged']
except Exception as e:report['error']=str(e)
report['checks']=sum(x.get('checks',0) for x in steps);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'passed':report['passed'],'checks':report['checks'],'report':str(out/'results.json'),'error':report.get('error')}));raise SystemExit(0 if report['passed'] else 1)
