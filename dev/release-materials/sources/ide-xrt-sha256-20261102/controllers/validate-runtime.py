from pathlib import Path
import hashlib,json,os,re,subprocess,sys,time
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
b=json.loads((O/'gui-rebuild.json').read_text());assert b['status']=='passed'
baseline=json.loads((O/'baseline.json').read_text())
protected={**b['inputs'],**baseline['protected']}
protected.update({p:sha(p) for p in b['objects']+b['resources']+[b['candidate'],str(O/'gui-rebuild.json'),str(Path(__file__))]})
record=dict(priorValidation=str(O/'validation.json'),priorValidationSha256=sha(O/'validation.json'),status='running',scope='Targeted SHA-256 GUI repair and existing native regressions against the unchanged root release libraries/engine/server. Existing catalog/compiler mismatch recorded,not full release admission.',protected=protected,commands=[],tests=[],rootTmpInitiallyPresent=(R/'release/tmp').exists())
env=dict(os.environ,PATH=str(R/'release')+os.pathsep+os.environ['PATH'],PYTHONDONTWRITEBYTECODE='1')
temp=O/'temp-runtime';temp.mkdir();env.update(TEMP=str(temp),TMP=str(temp))
def save():(O/'validation-runtime.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
def run(name,args,timeout=120):
 log=O/('runtime-'+name+'.log');row=dict(name=name,command=list(map(str,args)),log=str(log));record['commands'].append(row);save()
 with log.open('wb') as stream:
  p=subprocess.Popen(list(map(str,args)),cwd=R/'release',env=env,stdout=stream,stderr=subprocess.STDOUT)
  row['pid']=p.pid;save()
  try:p.wait(timeout=timeout)
  except BaseException:p.kill();p.wait();raise
 row.update(exit=p.returncode,sha256=sha(log));save()
 if p.returncode:raise ValueError(name+': '+log.read_text('utf8',errors='replace')[-2000:])
 return log
try:
 for p,d in protected.items():assert sha(p)==d,p
 pe=subprocess.check_output([str(Path(b['compiler']).with_name('objdump.exe')),'-p',b['candidate']],text=True)
 imports=[line.strip().split()[-1] for line in pe.splitlines() if re.search(r'\bBCrypt\w+',line)]
 assert imports==['BCryptGenRandom'],imports
 xrtImports=[line.strip().split()[-1] for line in pe.splitlines() if re.search(r'\bxrtSha256\w*',line)]
 assert set(xrtImports)=={'xrtSha256','xrtSha256Init','xrtSha256Update','xrtSha256Final'},xrtImports
 (O/'imports.txt').write_text(pe,encoding='utf8');record['imports']=dict(bcrypt=imports,xrtSha256=xrtImports,reportSha256=sha(O/'imports.txt'));save()
 tests=['rename_test','migration_store_test','startup_stage_test','startup_cache_test','build_inputs_test','editor_view_test','input_test']
 fixtures=O/'runtime-fixtures';fixtures.mkdir()
 for name in tests:
  source=R/'ide/tests'/(name+'.c');protected[str(source)]=sha(source)
  objects=[p for p in b['objects'] if not (name=='state_repair_test' and p.endswith('src_platform_state_repair.c.o'))]
  exe=R/'release'/('xide_sha256_20261102_'+name+'.exe');assert not exe.exists();run(name+'-compile',[b['compiler'],*b['flags'],source,*objects,*b['libraries'],'-o',exe])
  case=fixtures/name;case.mkdir()
  args=[exe,case]
  if name=='catalog_test':args.append(R/'release/data/commands.json')
  if name in ['build_inputs_test','rename_test']:args.append(R/'release/xl.exe')
  if name=='startup_cache_test':args=[exe,'--cases',case]
  log=run(name,args,240);record['tests'].append(dict(name=name,status='passed',output=log.read_text('utf8',errors='replace')[-1800:],executableSha256=sha(exe)));save();print(name,'passed',flush=True)
 for name in tests:
  exe=R/'release'/('xide_sha256_20261102_'+name+'.exe')
  assert sha(exe)==next(t['executableSha256'] for t in record['tests'] if t['name']==name);exe.unlink()
 for p,d in protected.items():assert sha(p)==d,p
 temporary=R/'release/tmp'
 if not record['rootTmpInitiallyPresent'] and temporary.is_dir() and not temporary.is_symlink() and not any(temporary.iterdir()):temporary.rmdir();record['removedNewEmptyTmpParent']=True
 record.update(status='passed',testsPassed=len(tests));save()
except BaseException as error:
 record.update(status='failed',error=repr(error));save();raise
