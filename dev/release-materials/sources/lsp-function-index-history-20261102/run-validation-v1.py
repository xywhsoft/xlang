from pathlib import Path
import hashlib,json,os,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[4];A=Path(__file__).resolve().parent
B=A/'full-v1';OLD=ROOT/'ide/build/Release/lsp-typed-function-ownership-20261102/full-v2';O=A/'validation-v1'
sys.path.insert(0,str(ROOT/'ide/tools'))
from build_lsp_lifecycle_profile import verify_build,sha

def main():
 O.mkdir(exist_ok=False);normal=json.loads((B/'build-result.json').read_text());verify_build(normal,B)
 record=dict(status='running',accepted=False,scope='Independent original collector function index assets,actual C ownership/range/provenance omissions,21-case wire parity and56 retained protocol suites. No ordinary AST/IR locality,GUI/performance/release admission.',commands=[],rootTmpInitiallyPresent=(ROOT/'release/tmp').exists(),normalBuild=str(B),protected={str(B/'build-result.json'):sha(B/'build-result.json'),str(Path(__file__)):sha(Path(__file__))})
 for name in ['xge.dll','xl.exe','xl.dll','xlc.dll','xlang_ide.exe','xlang-ls.exe','option/settings.json']:
  p=ROOT/'release'/name;record['protected'][str(p)]=sha(p)
 temp=O/'temp';temp.mkdir();env=dict(os.environ,TMP=str(temp),TEMP=str(temp));env['PATH']=str(Path(normal['compiler']).parent)+os.pathsep+env.get('PATH','')
 def save():(O/'result.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
 def run(name,command):
  log=O/(name+'.log');row=dict(name=name,command=command,status='running',log=str(log));record['commands'].append(row);save();start=time.monotonic()
  with log.open('wb') as stream:
   child=subprocess.Popen(command,cwd=O,env=env,stdout=stream,stderr=subprocess.STDOUT);row['pid']=child.pid;save()
   try:child.wait(timeout=900)
   except BaseException:child.kill();child.wait(timeout=5);raise
  row.update(exit=child.returncode,status='passed' if not child.returncode else 'failed',seconds=time.monotonic()-start,sha256=sha(log));save();print(name,row['exit'],flush=True)
  assert not child.returncode,(name,log.read_text('utf8',errors='replace')[-2400:])
 try:
  old=json.loads((OLD/'build-result.json').read_text());original=A/'previous-ide-inputs';manifest=json.loads((A/'capture.json').read_text())['files'];rebound=[]
  for path,digest in old['inputs'].items():
   p=Path(path)
   if sha(p)!=digest:
    rel=str(p.relative_to(ROOT)).replace('\\','/');assert rel in manifest and manifest[rel]==digest,(rel,'unarchived baseline input change');q=original/rel;assert sha(q)==digest;rebound.append(dict(recordedPath=path,archivedPath=str(q),sha256=digest))
  for name,digest in old['outputs'].items():
   p=Path(name) if Path(name).is_absolute() else OLD/name;assert sha(p)==digest,p
  record['baselineClosure']=dict(status='passed',inputs=len(old['inputs']),outputs=len(old['outputs']),archivedChangedInputs=rebound,scope='Original report bytes/paths unchanged;exact pre-edit bytes retained for owned changed inputs. No arbitrary pin bypass.')
  save()
  run('index-asset-omissions',[sys.executable,'-X','utf8','-B',str(ROOT/'ide/tools/check_function_index_assets.py'),'--build',str(B),'--output',str(A/'omissions-v1')])
  run('unified-wire',[sys.executable,'-X','utf8','-B',str(ROOT/'ide/tools/check_lsp_unified_frontend.py'),'--server',str(B/'xlang-ls.exe'),'--corpus',str(B/'unified-fixtures'),'--output',str(A/'wire-v1'),'--c-interop'])
  registryPath=OLD.parent/'validation-v1/result.json';registry=json.loads(registryPath.read_text());assert registry['status']=='passed' and registry['suites']==56;registry['commands']=registry['commands'][2:];assert len(registry['commands'])==56;record['protected'][str(registryPath)]=sha(registryPath)
  protocol=O/'protocol';protocol.mkdir()
  for previous in registry['commands']:
   command=[]
   for value in previous['command']:
    if value.startswith(str(OLD)):value=value.replace(str(OLD),str(B),1)
    elif value.startswith(str(OLD.parent/'validation-v1/protocol')):value=value.replace(str(OLD.parent/'validation-v1/protocol'),str(protocol),1)
    command.append(value)
   if '--expected-version' in command:
    slot=command.index('--expected-version')+1;assert command[slot]=='0.101'
   for value in command:
    path=Path(value)
    if path.suffix=='.py' and path.is_file():record['protected'][str(path)]=sha(path)
   run(previous['name'],command)
  verify_build(normal,B)
  for path,digest in record['protected'].items():assert sha(Path(path))==digest,path
  record.update(status='passed',suites=56)
 except BaseException as error:record.update(status='failed',error=repr(error));raise
 finally:
  temporary_parent=ROOT/'release/tmp'
  if not record['rootTmpInitiallyPresent'] and temporary_parent.is_dir() and not temporary_parent.is_symlink() and not any(temporary_parent.iterdir()):
   temporary_parent.rmdir();record['removedNewEmptyTmpParent']=True
  save()
if __name__=='__main__':main()
