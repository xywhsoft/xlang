"""Test table ownership with the original native lowerer,never in the LS core."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,shutil
from build_lsp_core import dependencies

def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--build',type=Path,required=True,help='matching full build-result.json')
 parser.add_argument('--output',type=Path,required=True,help='fresh external validation directory')
 parser.add_argument('--runtime',type=Path,required=True,help='real repository release/win_x64')
 args=parser.parse_args();b=json.loads(args.build.read_text(encoding='utf8'))
 if b['status']!='passed':raise ValueError('Full native build must have passed')
 expected={str(Path(p).resolve()):h for p,h in b['inputs'].items()}
 for p,h in expected.items():
  if sha(p)!=h:raise ValueError('Changed matching build input: '+p)
 args.output=args.output.resolve();args.output.mkdir(parents=True,exist_ok=False)
 source=args.output/'source';source.mkdir();ide=Path(__file__).resolve().parents[1]
 canonicalInputs={str(Path(__file__).resolve()):sha(Path(__file__))}
 shutil.copy2(Path(__file__),source/'check_ir_table_graph_lowered.py')
 for n in ('analysis_ir_table_graph_test.c','analysis_ir_memo_test.c'):
  original=ide/'tests'/n;h=sha(original);shutil.copy2(original,source/n)
  if sha(original)!=h or sha(source/n)!=h:raise ValueError('Test source changed during snapshot')
  canonicalInputs[str(original)]=h
 template=next(c for c in b['commands']if '-c'in c and Path(c[c.index('-c')+1]).stem=='analysis_ir_table_graph_test')
 native=next(c for c in b['commands']if '-c'in c and Path(c[c.index('-c')+1]).stem=='xl6_ir')
 link=next(c for c in b['commands']if '-c'not in c and '-o'in c and Path(c[c.index('-o')+1]).name=='analysis_ir_table_graph_test.exe')
 core=Path(next(p for p in link if p.endswith('xlang-analysis-core.a')))
 if sha(core)!=b['outputs']['xlang-analysis-core.a']:raise ValueError('Matching core archive changed')
 lower=Path(b['frontendRoot'])/'src/ir/xl6_ir_lower.c'
 env=dict(os.environ,TEMP=str(args.output),TMP=str(args.output))
 result=dict(status='failed',scope='Test-only original lowering;no backend dependency in core/server',commands=[],inputs={},objects={},programs={},canonicalInputs=canonicalInputs,linkedCore={str(core):sha(core)},fullBuildSha256=sha(args.build));owned=[]
 def save():(args.output/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
 def run(label,command,check=True):
  result['commands'].append(command);p=subprocess.run(command,env=env,cwd=args.output,capture_output=True,timeout=240);(args.output/(label+'.log')).write_bytes(p.stdout+p.stderr)
  if check and p.returncode:raise RuntimeError(label+' failed: '+p.stderr.decode(errors='replace')[-2500:])
  return p
 try:
  compileCommands=[]
  for label,flags,path in [('lower',native[:native.index('-c')],lower),
      ('test-default',template[:template.index('-c')],source/'analysis_ir_table_graph_test.c'),
      ('test',[*template[:template.index('-c')],'-DXLS_IR_TABLE_GRAPH_LOWERED_TEST=1'],source/'analysis_ir_table_graph_test.c')]:
   dep=run(label+'-dependencies',[*flags,'-MM','-MT','testObject',str(path)])
   for p in dependencies(dep.stdout.decode('utf8')):
    h=sha(p)
    if p.parent!=source and expected.get(str(p))!=h:raise ValueError('Unmatched lowered-test dependency: '+str(p))
    result['inputs'][str(p)]=h
   target=args.output/(label+'.o')
   command=[*flags,'-c',str(path),'-o',str(target)];run(label+'-compile',command);compileCommands.append(command);result['objects'][str(target)]=sha(target)
  runtime=args.runtime.resolve()
  if runtime!=(ide.parent/'release/win_x64').resolve():raise ValueError('Use the real repository runtime')
  result['controls']=[]
  for label,objectName in [('default','test-default.o'),('lowered','test.o')]:
   lc=link.copy();lc[next(i for i,p in enumerate(lc)if Path(p).name=='analysis_ir_table_graph_test.o')]=str(args.output/objectName)
   if label=='lowered':lc.insert(1,str(args.output/'lower.o'))
   program=args.output/('table-'+label+'-test.exe');lc[lc.index('-o')+1]=str(program);run(label+'-link',lc)
   h=sha(program);result['programs'][str(program)]=h;target=runtime/'.ir-table-graph-lowered-test.exe'
   if target.exists():raise ValueError('Test alias already exists')
   shutil.copy2(program,target);owned.append((target,h))
   if sha(target)!=h:raise RuntimeError('Test alias copy changed')
   p=run(label+'-run',[str(target)],False)
   result['controls'].append(dict(label=label,exit=p.returncode,stdout=p.stdout.decode(errors='replace'),stderr=p.stderr.decode(errors='replace'),programSha256=h));save()
   if p.returncode or b'analysis_ir_table_graph_test passed:'not in p.stdout:raise RuntimeError(label+' ownership validation failed')
   if sha(target)!=h:raise RuntimeError('Changed owned test alias')
   target.unlink();owned.pop()
  result['exit']=result['controls'][-1]['exit'];result['stdout']=result['controls'][-1]['stdout'];result['stderr']=result['controls'][-1]['stderr']
  for p,h in result['inputs'].items():
   if sha(p)!=h:raise ValueError('Lowered-test input changed: '+p)
  for p,h in result['canonicalInputs'].items():
   if sha(p)!=h:raise ValueError('Canonical lowered-test source changed: '+p)
  for p,h in {**result['objects'],**result['programs'],**result['linkedCore']}.items():
   if sha(p)!=h:raise ValueError('Linked lowered-test artifact changed: '+p)
  result['status']='passed';print(json.dumps(dict(status='passed',programSha256=sha(program),summary=result['stdout'].splitlines()[-1])),flush=True)
 finally:
  for p,h in owned:
   if sha(p)!=h:raise RuntimeError('Changed owned test alias')
   p.unlink()
  save()

if __name__=='__main__':main()
