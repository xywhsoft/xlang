"""Batch public XUI gallery validation with the installed compiler.

check/cgen/build do not open windows. smoke explicitly runs bounded real
windows through the same public examples and frame loop. Reports retain every
failure; a later batch must use a fresh output directory.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,subprocess,time

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--runtime',type=Path);p.add_argument('--out',type=Path,required=True);p.add_argument('--modes',default='check,cgen,build');p.add_argument('--names',default='');p.add_argument('--jobs',type=int,default=2);p.add_argument('--resume',action='store_true');a=p.parse_args();root=a.root.resolve();runtime=(a.runtime or root/'release/win_x64').resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=a.resume);cli=runtime/('xl.exe'if os.name=='nt'else'xl');source=root/'lib/bindings/xui/examples';catalog=json.loads((source/'gallery.json').read_text(encoding='utf-8'));names=a.names.split(',')if a.names else[row['name']for row in catalog['examples']];modes=a.modes.split(',');assert set(modes)<={'check','cgen','build','smoke','execute'}and 1<=a.jobs<=3;assert set(names)<={row['name']for row in catalog['examples']}
 env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k not in('XLANG_PACKAGE_PATH','CPATH','C_INCLUDE_PATH','LIBRARY_PATH','XUI_EXAMPLE_DUMP','XUI_EXAMPLE_CYCLE','XUI_EXAMPLE_EXERCISE')};env.update(XLANG_PACKAGE_PATH=str(runtime/'extlibs'),XUI_EXAMPLE_FRAMES='3');inputs={str(p):sha(p)for p in source.rglob('*')if p.is_file()and '.xlang-ide'not in p.parts};inputs.update((str(runtime/n),sha(runtime/n))for n in ('xl.exe','xl.dll','xlc.dll','xl','libxl.so','libxlc.so')if(runtime/n).is_file());inputs.update((str(p),sha(p))for package in ('xui','xge','_xge_native')for p in(runtime/'extlibs'/package).rglob('*')if p.is_file()and '.xlang-ide'not in p.parts);inputs[str(Path(__file__).resolve())]=sha(__file__);report=dict(complete=False,passed=False,scope=__doc__,runtime=str(runtime),inputs=inputs,rows=[],modes=modes,examples=names,windowTests=bool({'smoke','execute'}&set(modes)),networkTests=False)
 if a.resume:
  previous_path=out/'results.json';previous=json.loads(previous_path.read_text())
  assert previous['modes']==modes and previous['examples']==names and previous['runtime']==str(runtime)
  checker=str(Path(__file__).resolve())
  assert set(previous['inputs'])==set(inputs)
  assert all(path==checker or digest==inputs[path]for path,digest in previous['inputs'].items()),'Validation inputs changed'
  for row in previous['rows']:
   log=out/row['example']/(row['mode']+'.log');assert sha(log)==row['logSha256']
  stamp=time.time_ns();(out/('results-before-resume-'+str(stamp)+'.json')).write_bytes(previous_path.read_bytes())
  report['rows']=previous['rows'];report['resumeHistory']=previous.get('resumeHistory',[])+[dict(checkerSha256=previous['inputs'][checker],recordedRows=len(previous['rows']),stamp=stamp)]
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 def run(name,mode):
  prefix=out/name;prefix.mkdir(exist_ok=True);args=[str(cli),mode if mode!='smoke'else'run',str(source/(name+'.xl'))];child_env=dict(env)
  if mode=='cgen':args+=['--emit',str(prefix/'generated.c')]
  if mode=='build':args+=['-o',str(prefix/('program.exe'if os.name=='nt'else'program'))]
  if mode=='execute':
   args=[str(prefix/('program.exe'if os.name=='nt'else'program'))]
   child_env.pop('XLANG_PACKAGE_PATH',None)
   native=runtime/'extlibs/_xge_native/native'
   child_env['PATH']=str(native)+os.pathsep+child_env.get('PATH','')
   if os.name!='nt':child_env['LD_LIBRARY_PATH']=str(native)+os.pathsep+child_env.get('LD_LIBRARY_PATH','')
   if name=='all_controls':
    scenes=sum(row['name']!='all_controls'and row.get('gallery',True)for row in catalog['examples'])
    child_env.update(XUI_EXAMPLE_CYCLE='1',XUI_EXAMPLE_FRAMES=str(scenes*3+2))
  start=time.monotonic()
  try:r=subprocess.run(args,cwd=runtime,env=child_env,capture_output=True,timeout=180);code=r.returncode;data=r.stdout+r.stderr
  except subprocess.TimeoutExpired as e:code=-1;data=(e.stdout or b'')+(e.stderr or b'')+b'\nTIMEOUT'
  except OSError as e:code=-3;data=str(e).encode('utf-8')
  if code==0 and mode=='execute'and name=='all_controls'and sum(line.startswith(b'SCENE ')for line in data.splitlines())!=sum(row['name']!='all_controls'and row.get('gallery',True)for row in catalog['examples']):
   code=-2;data+=b'\nGallery did not execute every scene'
  log=prefix/(mode+'.log');log.write_bytes(data);artifact=prefix/('program.exe'if os.name=='nt'else'program');artifact_hash=sha(artifact)if mode in('build','execute')and artifact.is_file()else None;return dict(artifactSha256=artifact_hash,example=name,mode=mode,command=args,exitCode=code,seconds=round(time.monotonic()-start,3),logSha256=sha(log),diagnostic=data.decode(errors='replace')[-2200:]if code else'')
 for mode in modes:
  with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs)as pool:
   completed={row['example']for row in report['rows']if row['mode']==mode and row['exitCode']==0}
   pending=[name for name in names if name not in completed]
   for row in pool.map(lambda name:run(name,mode),pending):
    report['rows'].append(row);save();print(row['mode'],row['example'],row['exitCode'],flush=True)
 report['inputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items());report.update(complete=True,passed=report['inputsUnchanged']and all(row['exitCode']==0 for row in report['rows']));save();print('PASSED',report['passed'],'ROWS',len(report['rows']),flush=True)
 if not report['passed']:raise SystemExit(1)
if __name__=='__main__':main()
