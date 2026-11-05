"""Synthetic-workspace tests only; never inspect or apply the real release."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,sys
import publish_xge3_packages as publisher

EXAMPLES={'xge':['README.md','headless_frame.xl','headless_shape.xl','gui_shapes.xl'],
          'xui':['README.md','headless_path.xl','headless_cell.xl','context_graph.xl','manager_lifetime.xl','document_model.xl','document_ui_headless.xl','gui_surface_draw.xl','data_object.xl']}

def snapshot(root):
 return {p.relative_to(root).as_posix():publisher.digest(p) for p in root.rglob('*') if p.is_file()}

def fixture(base,platform='win_x64'):
 root=base/'repo';packages=base/'packages';root.mkdir(parents=True);packages.mkdir();old=[];new=[];examples=[];outputs={}
 def target(relative,data):
  path=root/relative;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data);return path
 def source(key,data):
  path=packages/key;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data);outputs[key]=publisher.digest(path)
  new.append(dict(platform=platform,source=str(path),target='release/'+platform+'/extlibs/'+key,sha256=outputs[key]))
 for name in ('xge','xui','_xge_native'):
  if platform=='win_x64':
   for item,data in [('xlang.package.json',b'old manifest'),('src/old_wrapper.xl',b'old wrapper')]:
    rel='release/'+platform+'/extlibs/'+name+'/'+item;p=target(rel,data);old.append(dict(currentPath=rel,sha256=publisher.digest(p),canonicalLFMatchesGit=True))
  source(name+'/xlang.package.json',json.dumps(dict(name=name,version='3.0.0',abiVersion='2')).encode())
  source(name+'/src/'+('runtime.c' if name=='_xge_native' else 'new.xl'),b'new direct declaration')
 source('_xge_native/native/'+('xge3_host.dll' if platform=='win_x64' else 'libxge3_host.so'),b'synthetic, never executed')
 for name,names in EXAMPLES.items():
  for filename in names:
   key=name+'/examples/'+filename;source(key,('maintained '+filename).encode());examples.append(dict(platform=platform,source=str(packages/key),target='release/'+platform+'/examples/'+name+'/'+filename,sha256=outputs[key]))
 old_examples=[]
 if platform=='win_x64':
  for name in ('legacy.xl','run_old.bat'):
   rel='release/'+platform+'/examples/xui/'+name;p=target(rel,b'old tutorial');old_examples.append(dict(currentPath=rel,sha256=publisher.digest(p),canonicalLFMatchesGit=True))
 # All of these must be untouched and must never enter the publication journal.
 for rel in ('examples/xui/.xlang-ide/session/settings.json','extlibs/xge/src/user_file.txt','option/settings.json','data/fonts/user.ttf','notices/user.txt','xge.dll'):
  target('release/'+platform+'/'+rel,b'user-owned fixture')
 stage=base/'stage.json';stage.write_text(json.dumps(dict(complete=True,inputsUnchanged=True,platform=platform,outputs=outputs)))
 baseline=base/'baseline.json';baseline.write_text(json.dumps(dict(complete=True,readOnly=True,inputsUnchanged=True,inputs={str(stage):publisher.digest(stage)},oldPackageFiles=old,oldVisibleExampleFiles=old_examples,newPackageFiles=new,newMaintainedExampleFiles=examples)))
 return dict(root=root,platform=platform,packages=packages,stage=stage,baseline=baseline,baseline_sha=publisher.digest(baseline),old=old+old_examples,new=new,examples=examples,base=base)

def spec(f,work):return publisher.preflight(f['root'],f['platform'],f['packages'],f['stage'],f['baseline'],f['baseline_sha'],work)

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--out',type=Path,required=True);a=parser.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=False);rows=[]
 f=fixture(out/'dry-run');before=snapshot(f['root']);s=spec(f,f['base']/'work');r=publisher.publish(s);assert not r['applied'] and snapshot(f['root'])==before;rows.append(dict(name='default-plan-only',passed=True))
 for platform in ('win_x64','linux_x64'):
  f=fixture(out/('normal-'+platform),platform);before=snapshot(f['root']);s=spec(f,f['base']/'work');r=publisher.publish(s,True);assert r['applied'] and r['finalAllTargetsVerified']
  for row in f['new']+f['examples']:assert publisher.digest(f['root']/row['target'])==row['sha256']
  expected={row['target'] for row in f['new']+f['examples']}
  for row in f['old']:
   if row['currentPath'] not in expected:assert not (f['root']/row['currentPath']).exists()
  after=snapshot(f['root']);protected={key:value for key,value in before.items() if key not in {row['currentPath'] for row in f['old']}};assert all(after[key]==value for key,value in protected.items());journal=json.loads((f['base']/'work/journal.json').read_text());assert not any('.xlang-ide' in row['target'] for row in journal['operations'])
  if platform=='linux_x64':assert r['knownOldFilesRetired']==0
  rows.append(dict(name='normal-'+platform,passed=True,hiddenAndUnknownPreserved=True))
 for failure in ('old-sha','source-sha','baseline-sha','unknown-target','work-inside-root','path-escape'):
  f=fixture(out/('reject-'+failure));work=f['base']/'work'
  if failure=='old-sha':(f['root']/f['old'][0]['currentPath']).write_bytes(b'user changed old file')
  elif failure=='source-sha':(f['packages']/'xge/src/new.xl').write_bytes(b'changed source')
  elif failure=='baseline-sha':f['baseline_sha']='0'*64
  elif failure=='unknown-target':p=f['root']/('release/'+f['platform']+'/extlibs/xge/src/new.xl');p.write_bytes(b'user file at new target')
  elif failure=='work-inside-root':work=f['root']/'work'
  elif failure=='path-escape':
   plan=json.loads(f['baseline'].read_text());plan['newPackageFiles'][0]['target']='release/win_x64/extlibs/xge/../../../option/settings.json';f['baseline'].write_text(json.dumps(plan));f['baseline_sha']=publisher.digest(f['baseline'])
  before=snapshot(f['root']);rejected=False
  try:spec(f,work)
  except ValueError:rejected=True
  assert rejected and snapshot(f['root'])==before and not work.exists();rows.append(dict(name='reject-'+failure,passed=True,noTargetWrites=True))
 f=fixture(out/'write-failure');before=snapshot(f['root']);s=spec(f,f['base']/'work');actual_replace=publisher.os.replace;counter=0;fired=False
 def failing_replace(source,target):
  nonlocal counter,fired
  if publisher.beneath(Path(target),f['root']):
   counter+=1
   if counter==2 and not fired:fired=True;raise OSError('synthetic target replace failure')
  return actual_replace(source,target)
 publisher.os.replace=failing_replace
 try:
  try:publisher.publish(s,True);raise AssertionError('failure was not injected')
  except RuntimeError:pass
 finally:publisher.os.replace=actual_replace
 journal=json.loads((f['base']/'work/journal.json').read_text());assert fired and journal['status']=='rolled-back' and not journal['rollbackFailures'] and not journal['temporaryFiles'] and snapshot(f['root'])==before;rows.append(dict(name='write-failure-journal-rollback',passed=True,originalTreeRestored=True))
 # Target temporary files and backup files are re-hashed, rather than trusted.
 for operation in journal['operations']:
  if operation['action']!='noop' and operation['before'] is not None:assert publisher.digest(f['base']/'work/backups'/operation['target'])==operation['before']
 f=fixture(out/'journal-failure');before=snapshot(f['root']);s=spec(f,f['base']/'work');actual_replace=publisher.os.replace;target_written=False;fired=False
 def failing_journal_replace(source,target):
  nonlocal target_written,fired
  target=Path(target)
  if publisher.beneath(target,f['root']):target_written=True
  if target==f['base']/'work/journal.json' and target_written and not fired:fired=True;raise OSError('synthetic journal replace failure')
  return actual_replace(source,target)
 publisher.os.replace=failing_journal_replace
 try:
  try:publisher.publish(s,True);raise AssertionError('journal failure was not injected')
  except RuntimeError:pass
 finally:publisher.os.replace=actual_replace
 journal=json.loads((f['base']/'work/journal.json').read_text());assert fired and journal['status']=='rolled-back' and not journal['rollbackFailures'] and snapshot(f['root'])==before;rows.append(dict(name='journal-failure-after-target-write-rollback',passed=True,originalTreeRestored=True))
 for platform in ('win_x64','linux_x64'):
  f=fixture(out/('cli-default-'+platform),platform);before=snapshot(f['root']);cmd=[sys.executable,'-B',str(Path(publisher.__file__)),'--root',str(f['root']),'--platform',platform,'--packages',str(f['packages']),'--stage-report',str(f['stage']),'--baseline-plan',str(f['baseline']),'--baseline-sha256',f['baseline_sha'],'--work-dir',str(f['base']/'work')];z=subprocess.run(cmd,capture_output=True,timeout=30);assert z.returncode==0 and not json.loads(z.stdout)['applied'] and snapshot(f['root'])==before;rows.append(dict(name='cli-no-apply-'+platform,passed=True))
 inputs={str(Path(__file__).resolve()):publisher.digest(__file__),str(Path(publisher.__file__).resolve()):publisher.digest(publisher.__file__)};report=dict(scope=__doc__,complete=True,passed=True,realReleaseTouched=False,syntheticOnly=True,rows=rows,inputs=inputs,inputsUnchanged=all(publisher.digest(p)==h for p,h in inputs.items()));assert report['inputsUnchanged'];(out/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(len(rows),'synthetic checks passed')
if __name__=='__main__':main()
