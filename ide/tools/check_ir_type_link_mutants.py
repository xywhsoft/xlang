"""Compile real type-link omissions against a bound native build and oracle."""
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
 nc=command('ir_type_link');tc=command('analysis_ir_type_link_test')
 lc=next(c for c in build['commands']if '-c'not in c and '-o'in c and Path(c[c.index('-o')+1]).name=='analysis_ir_type_link_test.exe')
 core=Path(next(p for p in lc if p.endswith('xlang-analysis-core.a')))
 if sha(core)!=build['outputs']['xlang-analysis-core.a']:raise ValueError('Changed matching core')
 inc=next(Path(p)for p in expected if Path(p).name=='ir-type-reference.inc')
 native=Path(nc[nc.index('-c')+1]).read_text(encoding='utf8');schema=inc.read_text(encoding='utf8')
 roles=[l for l in schema.splitlines()if l.startswith('TYPE_REFERENCE(')]
 result=dict(status='failed',buildSha256=sha(args.build),commands=[],inputs={},objects={},programs={},runs=[],core={str(core):sha(core)},toolSha256=sha(__file__))
 env=dict(os.environ,TMP=str(args.output),TEMP=str(args.output));alias=runtime/'.ir-type-link-mutant-test.exe';owned=None
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
  if len(roles)!=12:raise ValueError('Audit expected type reference roles')
  comparePath=inc.with_name('ir-type-compare.inc');comparison=comparePath.read_text(encoding='utf8')
  testPath=Path(tc[tc.index('-c')+1]);test=compile('test',tc[:tc.index('-c')],testPath)
  private=args.output/'native-only';private.mkdir();text=testPath.read_text(encoding='utf8')
  seam='if(!typeFailures() || !emptyTypes() || !typeNative() || !mixedNativeTypes() || !typeLifetimes() || !typeVectors())return 1;'
  if text.count(seam)!=1:raise ValueError('Native-only test seam changed')
  driver=private/testPath.name;driver.write_text(text.replace(seam,'if(!mixedNativeTypes())return 1;',1),encoding='utf8')
  shutil.copy2(testPath.with_name('analysis_ir_memo_test.c'),private/'analysis_ir_memo_test.c')
  nativeTest=compile('native-only-test',[*tc[:tc.index('-c')],'-Wno-unused-function'],driver)
  cases=[('positive',native,schema,test,False)]+[('omit-'+str(i),native,schema.replace(row+'\n','',1),test,False)for i,row in enumerate(roles)]
  def change(label,old,new):
   if native.count(old)!=1:raise ValueError('Type mutation seam changed: '+label)
   cases.append((label,native.replace(old,new,1),schema,test,False))
  change('old-params','dst->f[j]=(XL6_IrTypeId)value;','dst->f[j]=(XL6_IrTypeId)(!strcmp(#f,"params") ? src->f[j] : value);')
  change('old-returns','dst->f[j]=(XL6_IrTypeId)value;','dst->f[j]=(XL6_IrTypeId)(!strcmp(#f,"returns") ? src->f[j] : value);')
  cname='memcpy(dst->cName,target->cName,sizeof(dst->cName));'
  change('old-cname',cname,'')
  change('old-result-cname','memcpy(dst->errorResultCName,target->errorResultCName,sizeof(dst->errorResultCName));','')
  change('old-hash','dst->hash=target->hash;','')
  change('old-lookup','image->pool.slots[i]=current->slots[i];','image->pool.slots[i]=0;')
  change('refresh-type-param','if(dst->kind==XL6_IR_TYPE_TYPE_PARAM)','if(0)')
  gate='if(!equal(dst,target))'
  change('skip-semantic-guard',gate,'if(!equal(dst,target) && 0)')
  change('leak-unit','xlsIrTableUnitRelease(image->units[i]);','(void)image->units[i];')
  change('leak-scratch','image->retained=count;xrtFree(destinations);','image->retained=count;(void)destinations;')
  change('undercharge-scratch','add(&peak,scratch)','add(&peak,0)')
  change('borrow-current-name','memcpy(dst,src,sizeof(*dst));size_t at;','memcpy(dst,src,sizeof(*dst));dst->name=target->name;size_t at;')
  cases.append(('native-only-positive',native,schema,nativeTest,False))
  cases.append(('native-old-cname',native.replace(cname,'',1).replace(gate,'if(!equal(dst,target) && 0)',1),schema,nativeTest,True))
  for label,text,generated,testObject,expectNativeVerifier in cases:
   directory=args.output/label;directory.mkdir();source=directory/'ir_type_link.c';source.write_text(text,encoding='utf8')
   (directory/inc.name).write_text(generated,encoding='utf8');(directory/comparePath.name).write_text(comparison,encoding='utf8')
   obj=compile(label,[nc[0],'-I'+str(directory),*nc[1:nc.index('-c')]],source)
   link=lc.copy();link[next(i for i,p in enumerate(link)if Path(p).name=='analysis_ir_type_link_test.o')]=str(testObject);link.insert(1,str(obj))
   program=args.output/(label+'.exe');link[link.index('-o')+1]=str(program)
   if run(label+'-link',link).returncode:raise RuntimeError(label+' link failed')
   h=sha(program);result['programs'][str(program)]=h
   if alias.exists():raise ValueError('Test alias exists')
   shutil.copy2(program,alias);owned=h
   p=run(label+'-run',[str(alias)]);result['runs'].append(dict(label=label,exit=p.returncode,stdout=p.stdout.decode(errors='replace'),stderr=p.stderr.decode(errors='replace'),omittedRole=roles[int(label[5:])]if label.startswith('omit-')else None))
   if sha(alias)!=h:raise RuntimeError('Owned alias changed')
   alias.unlink();owned=None
   if label in ('positive','native-only-positive'):
    if p.returncode or b'analysis_ir_type_link_test passed:'not in p.stdout:raise RuntimeError('Positive control failed')
   elif not p.returncode or b'analysis_ir_memo_test:'not in p.stderr:raise RuntimeError('Mutation not rejected by independent oracle: '+label)
   if expectNativeVerifier and b'xl6IrVerifyModule(actual,&diags)'not in p.stderr:raise RuntimeError('Original native verifier did not reject stale backend name')
  for section in ('inputs','objects','programs','core'):
   for p,h in result[section].items():
    if sha(p)!=h:raise ValueError('Changed validation input/artifact: '+p)
  result['status']='passed';print(json.dumps(dict(status='passed',mutants=len(cases)-2,positiveControls=2,originalNativeVerifierRejectedStaleCname=True)))
 finally:
  if owned:
   if sha(alias)!=owned:raise RuntimeError('Owned alias changed')
   alias.unlink()
  (args.output/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')

if __name__=='__main__':main()
