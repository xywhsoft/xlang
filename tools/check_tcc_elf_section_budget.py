#!/usr/bin/env python3
"""Focused GCC CPU test for ELF input section budgeting; no PE files emitted."""
import argparse,hashlib,json,os,re,shutil,struct,subprocess,time
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--root',required=True);ap.add_argument('--out',required=True);ap.add_argument('--cc',default='gcc');ap.add_argument('--actual-object');a=ap.parse_args()
root=Path(a.root).resolve();out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=False);source=out/'source';source.mkdir();inputs=[];steps=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for p in sorted((root/'lib/tcc').rglob('*')):
 if p.is_file() and '__pycache__' not in p.parts:
  rel=p.relative_to(root);q=source/rel;q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q);h=sha(q)
  if sha(p)!=h:raise RuntimeError('Input changed during copy: '+str(p))
  inputs.append({'path':rel.as_posix(),'sha256':h})
for name in ['test/cases/tcc_elf_section_budget.c','tools/check_tcc_elf_section_budget.py','docs/tcc-elf-section-budget.md']:
 p=root/name;q=source/name;q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q);inputs.append({'path':name,'sha256':sha(q)})
cc=shutil.which(a.cc) or a.cc
report={'scope':'ELF input-section budget only. GCC-built host fixture admits ELF inputs in memory; no emitted PE or OS/network/window tests. Existing new_section hard limit and controlled jump remain intact.','sourceInputs':inputs,'steps':steps,'passed':False}
def run(label,args):
 t=time.monotonic();p=subprocess.run([str(x) for x in args],cwd=out,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180);log=out/(label+'.log');log.write_bytes(p.stdout);steps.append({'label':label,'command':[str(x) for x in args],'exitCode':p.returncode,'seconds':time.monotonic()-t,'log':str(log),'logSha256':sha(log)})
 if p.returncode:raise RuntimeError(label+' failed: '+str(log))
 return p.stdout.decode('utf-8',errors='replace')
def object_file(path,count):
 ident=b'\x7fELF'+bytes([2,1,1,0])+b'\0'*8
 if not count:
  path.write_bytes(struct.pack('<16sHHIQQQIHHHHHH',ident,1,62,1,0,0,64,0,64,0,0,64,0,0));return
 data=bytearray(b'\0'*64);names=bytearray(b'\0');headers=[[0]*10]
 def name(s):
  n=len(names);names.extend(s.encode()+b'\0');return n
 def add_bytes(payload,align=1):
  while len(data)%align:data.append(0)
  at=len(data);data.extend(payload);return at
 remain=count-4;index=0
 while remain:
  target=len(headers);at=add_bytes(b'A');headers.append([name('.data.budget.'+str(index)),1,3,0,at,1,0,0,1,0]);remain-=1
  if remain:
   headers.append([name('.rela.data.budget.'+str(index)),4,0,0,len(data),0,0,target,8,24]);remain-=1
  index+=1
 sym=len(headers);headers.append([name('.symtab'),2,0,0,add_bytes(b'\0'*24,8),24,sym+1,1,8,24])
 headers.append([name('.strtab'),3,0,0,add_bytes(b'\0'),1,0,0,1,0])
 shstr=len(headers);last_name=name('.shstrtab');headers.append([last_name,3,0,0,add_bytes(names),len(names),0,0,1,0])
 for h in headers:
  if h[1]==4:h[6]=sym
 while len(data)%8:data.append(0)
 shoff=len(data)
 for h in headers:data.extend(struct.pack('<IIQQQQIIQQ',*h))
 assert len(headers)==count
 data[:64]=struct.pack('<16sHHIQQQIHHHHHH',ident,1,62,1,0,0,shoff,0,64,0,0,64,count,shstr);path.write_bytes(data)
try:
 run('compiler-version',[cc,'--version']);obj=out/'objects';obj.mkdir()
 for name,count in [('large.o',20189),('small.o',5),('extended.o',0)]:object_file(obj/name,count)
 report['generatedObjects']=[{'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size} for p in obj.iterdir()]
 actual=[]
 if a.actual_object:
  original=Path(a.actual_object).resolve();q=obj/'actual-runtime.o';shutil.copyfile(original,q);report['actualObject']={'source':str(original),'path':str(q),'sha256':sha(q),'bytes':q.stat().st_size};actual=[q]
 for opt in ['O0','O2']:
  exe=out/('fixture-'+opt+('.exe' if os.name=='nt' else ''))
  args=[cc,'-std=c11','-'+opt,'-DTCC_DISABLE_BUILTIN_VFS','-I'+str(source/'lib/tcc'),'-o',exe,source/'test/cases/tcc_elf_section_budget.c']
  args+=([source/'lib/tcc/tcc_utf8_io.c','-lws2_32'] if os.name=='nt' else ['-ldl','-lpthread','-lm'])
  run(opt+'-build',args);text=run(opt+'-cpu',[exe,obj,source/'lib/tcc',*actual]);m=re.search(r'ELF_BUDGET_CHECKS=(\d+) FAILURES=(\d+)',text)
  if not m or int(m[2]):raise RuntimeError('Fixture result invalid')
  steps[-1]['checks']=int(m[1])
 report['frozenInputsUnchanged']=all(sha(source/x['path'])==x['sha256'] for x in inputs)
 report['sourceInputsStillMatchWorkspace']=all(sha(root/x['path'])==x['sha256'] for x in inputs)
 report['passed']=report['frozenInputsUnchanged']
except Exception as e:report['error']=str(e)
report['checks']=sum(x.get('checks',0) for x in steps);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps({'passed':report['passed'],'checks':report['checks'],'report':str(out/'results.json'),'error':report.get('error')}));raise SystemExit(0 if report['passed'] else 1)
