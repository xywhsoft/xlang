#!/usr/bin/env python3
"""Build isolated GCC CPU fixtures for opt-in TinyCC section GC; never run emitted PE images."""
import argparse,hashlib,json,os,re,shutil,subprocess,time
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--root',required=True);ap.add_argument('--out',required=True);ap.add_argument('--cc',default='gcc');a=ap.parse_args()
root=Path(a.root).resolve();out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=False)
source=out/'source';source.mkdir();rows=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for folder in ['lib/tcc']:
 for p in sorted((root/folder).rglob('*')):
  if not p.is_file() or '__pycache__' in p.parts:continue
  rel=p.relative_to(root);q=source/rel;q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q)
  h=sha(q)
  if sha(p)!=h:raise SystemExit('Source changed while copying: '+str(p))
  rows.append({'path':rel.as_posix(),'sha256':h})
for name in ['test/cases/tcc_section_gc.c','tools/check_tcc_section_gc.py']:
 p=root/name;q=source/name;q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q);rows.append({'path':name,'sha256':sha(q)})
cc=shutil.which(a.cc) or a.cc
report={'scope':'Native GCC fixture directly includes the exact production libtcc implementation. CPU JIT and generated object/image inspection only; no emitted PE execution, network, UI or security probe.','compiler':cc,'sourceInputs':rows,'steps':[],'passed':False}
def run(label,args,cwd=out):
 started=time.monotonic();r=subprocess.run([str(x) for x in args],cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf-8',errors='replace',timeout=180)
 log=out/(label+'.log');log.write_text(r.stdout,encoding='utf-8');step={'label':label,'command':[str(x) for x in args],'exitCode':r.returncode,'seconds':time.monotonic()-started,'log':str(log),'logSha256':sha(log)};report['steps'].append(step)
 if r.returncode:raise RuntimeError(label+' failed; see '+str(log))
 return r.stdout
try:
 report['compilerVersion']=run('compiler-version',[cc,'--version'])
 for opt in ['O0','O2']:
  dest=out/opt;dest.mkdir();exe=dest/('fixture.exe' if os.name=='nt' else 'fixture')
  cmd=[cc,'-std=c11','-'+opt,'-DTCC_DISABLE_BUILTIN_VFS','-I'+str(source/'lib/tcc'),'-o',exe,source/'test/cases/tcc_section_gc.c']
  cmd+=([source/'lib/tcc/tcc_utf8_io.c','-lws2_32'] if os.name=='nt' else ['-ldl','-lpthread','-lm'])
  run(opt+'-build',cmd)
  text=run(opt+'-cpu',[exe,source/'lib/tcc',dest]);m=re.search(r'TCC_GC_CHECKS=(\d+) FAILURES=(\d+)',text)
  if not m or int(m[2]):raise RuntimeError('Fixture result count invalid')
  report['steps'][-1]['checks']=int(m[1]);report['steps'][-1]['images']=[{'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(dest.glob('*native.bin'))]
  report['steps'][-1]['imageExecution']=False
  plain=(dest/'size-plain-native.bin').stat().st_size;collected=(dest/'size-gc-native.bin').stat().st_size
  if plain-collected<16000:raise RuntimeError('Same-source final image size did not lose dead constant storage')
  report['steps'][-1]['sizePair']={'sameSource':True,'gcOffBytes':plain,'gcOnBytes':collected,'savedFileBytes':plain-collected,'includesBssInFileSize':False}
  report['steps'][-1]['checks']+=1

  addr2line=Path(cc).with_name('addr2line.exe' if os.name=='nt' else 'addr2line')
  if not addr2line.exists():addr2line=Path(shutil.which('addr2line') or '')
  if not addr2line.is_file():raise RuntimeError('Matching GNU addr2line is required for split debug validation')
  queries=re.findall(r'DEBUG_QUERY (\w+) (0x[0-9a-f]+) (0x[0-9a-f]+)',text)
  if len(queries)!=4:raise RuntimeError('Four debug formats were not produced')
  for mode,aa,bb in queries:
   decoded=run(opt+'-'+mode+'-addr2line',[addr2line,'-f','-e',dest/('debug-'+mode+'-native.bin'),aa,bb])
   if not re.search(r'gc_debug\.c:101\b',decoded) or not re.search(r'gc_debug\.c:111\b',decoded):raise RuntimeError('Wrong debug source lines for '+mode)
   report['steps'][-1]['checks']=2

 report['frozenInputsUnchanged']=all(sha(source/x['path'])==x['sha256'] for x in rows)
 report['sourceInputsStillMatchWorkspace']=all(sha(root/x['path'])==x['sha256'] for x in rows)
 report['passed']=report['frozenInputsUnchanged']
except Exception as e:report['error']=str(e)
(out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'passed':report['passed'],'report':str(out/'results.json'),'steps':len(report['steps']),'checks':sum(x.get('checks',0) for x in report['steps'])}))
raise SystemExit(0 if report['passed'] else 1)
