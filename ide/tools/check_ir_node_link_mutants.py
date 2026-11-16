"""Compile real node-link omissions against a bound native build and oracle."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
from build_lsp_core import dependencies

def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--build',type=Path,required=True)
 parser.add_argument('--output',type=Path,required=True)
 parser.add_argument('--runtime',type=Path,required=True)
 args=parser.parse_args();build=json.loads(args.build.read_text(encoding='utf8'))
 if build['status']!='passed':raise ValueError('Need a successful native build')
 expected={str(Path(p).resolve()):h for p,h in build['inputs'].items()}
 for p,h in expected.items():
  if sha(p)!=h:raise ValueError('Changed matching build input: '+p)
 runtime=args.runtime.resolve()
 if runtime!=(Path(__file__).resolve().parents[2]/'release/win_x64').resolve():raise ValueError('Use repository root release/win_x64')
 args.output=args.output.resolve();args.output.mkdir(parents=True,exist_ok=False)
 def command(stem):return next(c for c in build['commands']if '-c'in c and Path(c[c.index('-c')+1]).stem==stem)
 nc=command('ir_node_link');tc=command('analysis_ir_node_link_test')
 lc=next(c for c in build['commands']if '-c'not in c and '-o'in c and Path(c[c.index('-o')+1]).name=='analysis_ir_node_link_test.exe')
 core=Path(next(p for p in lc if p.endswith('xlang-analysis-core.a')))
 if sha(core)!=build['outputs']['xlang-analysis-core.a']:raise ValueError('Changed matching core')
 inc=next(Path(p)for p in expected if Path(p).name=='ir-node-reference-schema.inc')
 native=Path(nc[nc.index('-c')+1]).read_text(encoding='utf8');schema=inc.read_text(encoding='utf8')
 roles=[l for l in schema.splitlines()if l.startswith('LINK_REFERENCE(')]
 result=dict(status='failed',buildSha256=sha(args.build),commands=[],inputs={},objects={},programs={},runs=[],core={str(core):sha(core)},toolSha256=sha(__file__))
 env=dict(os.environ,TMP=str(args.output),TEMP=str(args.output));alias=runtime/'.ir-node-link-mutant-test.exe';owned=None
 def run(label,c):
  result['commands'].append(c);p=subprocess.run(c,cwd=args.output,env=env,capture_output=True,timeout=240)
  (args.output/(label+'.log')).write_bytes(p.stdout+p.stderr);return p
 def compile(label,flags,source):
  dep=run(label+'-deps',[*flags,'-MM','-MT','object',str(source)])
  if dep.returncode:raise RuntimeError(label+' dependency scan failed')
  for p in dependencies(dep.stdout.decode('utf8')):
   h=sha(p);result['inputs'][str(p)]=h
   if not p.resolve().is_relative_to(args.output)and expected.get(str(p.resolve()))!=h:raise ValueError('Unbound dependency '+str(p))
  target=args.output/(label+'.o');p=run(label+'-compile',[*flags,'-c',str(source),'-o',str(target)])
  if p.returncode:raise RuntimeError(label+' compile failed')
  result['objects'][str(target)]=sha(target);return target
 try:
  if len(roles)!=27:raise ValueError('Audit expected reference roles')
  test=compile('test',tc[:tc.index('-c')],Path(tc[tc.index('-c')+1]))
  cases=[('positive',native,schema)]+[('omit-'+str(i),native,schema.replace(row+'\n','',1))for i,row in enumerate(roles)]
  seam='dst->span=current->span;'
  if native.count(seam)!=1:raise ValueError('Coordinate seam changed')
  cases.append(('old-spans',native.replace(seam,'',1),schema))
  for label,text,generated in cases:
   directory=args.output/label;directory.mkdir();source=directory/'ir_node_link.c';source.write_text(text,encoding='utf8')
   (directory/inc.name).write_text(generated,encoding='utf8')
   obj=compile(label,[nc[0],'-I'+str(directory),*nc[1:nc.index('-c')]],source)
   link=lc.copy();link[next(i for i,p in enumerate(link)if Path(p).name=='analysis_ir_node_link_test.o')]=str(test);link.insert(1,str(obj))
   program=args.output/(label+'.exe');link[link.index('-o')+1]=str(program)
   if run(label+'-link',link).returncode:raise RuntimeError(label+' link failed')
   h=sha(program);result['programs'][str(program)]=h
   if alias.exists():raise ValueError('Test alias exists')
   shutil.copy2(program,alias);owned=h
   p=run(label+'-run',[str(alias)]);result['runs'].append(dict(label=label,exit=p.returncode,stdout=p.stdout.decode(errors='replace'),stderr=p.stderr.decode(errors='replace'),omittedRole=roles[int(label[5:])]if label.startswith('omit-')else None))
   if sha(alias)!=h:raise RuntimeError('Owned alias changed')
   alias.unlink();owned=None
   if label=='positive':
    if p.returncode or b'analysis_ir_node_link_test passed:'not in p.stdout:raise RuntimeError('Positive control failed')
   elif not p.returncode or b'analysis_ir_memo_test:'not in p.stderr:raise RuntimeError('Mutation not rejected by independent oracle: '+label)
  for section in ('inputs','objects','programs','core'):
   for p,h in result[section].items():
    if sha(p)!=h:raise ValueError('Changed validation input/artifact: '+p)
  result['status']='passed';print(json.dumps(dict(status='passed',mutants=len(cases)-1,positiveControl=True)))
 finally:
  if owned:
   if sha(alias)!=owned:raise RuntimeError('Owned alias changed')
   alias.unlink()
  (args.output/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')

if __name__=='__main__':main()
