"""Publish only the pinned XGE/XUI packages and maintained examples.

The default is a read-only preflight/plan written to an external work directory.
--apply enables individual-file transactions. Unknown/hidden files, GUI and
compiler binaries, settings, notices and delivery indexes are outside the scope.
"""
from pathlib import Path, PurePosixPath
import argparse,hashlib,json,os,shutil,stat,sys,uuid

PACKAGES=frozenset(('xge','xui','_xge_native'))
PLAN_R4_SHA256='9e14e9db59d5fdc61aef92230956e7a017b5698ceb207f322c40b3f9ce059164'

def digest(path):
 h=hashlib.sha256()
 with Path(path).open('rb') as stream:
  for chunk in iter(lambda:stream.read(1024*1024),b''):h.update(chunk)
 return h.hexdigest()

def require(condition,message):
 if not condition:raise ValueError(message)

def hex_digest(value):
 return isinstance(value,str) and len(value)==64 and all(c in '0123456789abcdef' for c in value)

def beneath(path,base):
 return path==base or base in path.parents

def no_links(base,path):
 require(base.is_dir(),'declared base directory disappeared: '+str(base))
 require(beneath(path,base),'path outside its declared base: '+str(path))
 for current in [base,*[base.joinpath(*path.relative_to(base).parts[:n]) for n in range(1,len(path.relative_to(base).parts)+1)]]:
  if not current.exists() and not current.is_symlink():continue
  info=current.lstat()
  require(not stat.S_ISLNK(info.st_mode) and not (getattr(info,'st_file_attributes',0)&0x400),'symlink/reparse point is not permitted: '+str(current))
  if current!=path:require(stat.S_ISDIR(info.st_mode),'ancestor is not a directory: '+str(current))

def relative_name(value):
 require(isinstance(value,str) and value and '\\' not in value,'invalid relative path')
 path=PurePosixPath(value)
 require(not path.is_absolute() and str(path)==value and all(p not in ('','..','.') and ':' not in p and not p.startswith('.xlang-ide') for p in path.parts),'unsafe relative path: '+value)
 return path.parts

def scoped_target(value,platform):
 parts=relative_name(value)
 require(len(parts)>=5 and parts[:2]==('release',platform),'wrong platform target: '+value)
 if parts[2]=='extlibs':require(parts[3] in PACKAGES,'unapproved package target: '+value)
 elif parts[2]=='examples':require(parts[3] in ('xge','xui'),'unapproved example target: '+value)
 else:raise ValueError('unapproved target: '+value)
 return parts

def atomic_json(path,value):
 temporary=path.with_name(path.name+'.tmp')
 created=False
 try:
  with temporary.open('x',encoding='utf-8',newline='\n') as stream:
   created=True;json.dump(value,stream,ensure_ascii=False,indent=2);stream.write('\n');stream.flush();os.fsync(stream.fileno())
  os.replace(temporary,path)
 except Exception:
  if created and temporary.is_file():temporary.unlink()
  raise

def current_hash(path):
 if not path.exists():return None
 require(path.is_file(),'target is not a regular file: '+str(path))
 return digest(path)

def preflight(root,platform,packages,stage_report,baseline,baseline_sha,work_dir):
 root,packages,stage_report,baseline,work_dir=[Path(p).resolve() for p in (root,packages,stage_report,baseline,work_dir)]
 require(root.is_dir() and packages.is_dir(),'root/packages directory missing')
 require(platform in ('win_x64','linux_x64'),'unsupported platform')
 require(not beneath(work_dir,root) and not beneath(work_dir,packages),'work directory must be external to the repository and staged packages')
 require(not work_dir.exists(),'work directory already exists; use a fresh external directory')
 require(hex_digest(baseline_sha) and digest(baseline)==baseline_sha,'baseline plan SHA mismatch')
 plan=json.loads(baseline.read_text(encoding='utf-8-sig'));stage=json.loads(stage_report.read_text(encoding='utf-8-sig'))
 require(plan.get('complete') and plan.get('readOnly') and plan.get('inputsUnchanged'),'unqualified baseline plan')
 require(stage.get('complete') and stage.get('inputsUnchanged') and stage.get('platform')==platform,'unqualified/wrong-platform stage report')
 require(digest(stage_report) in plan.get('inputs',{}).values(),'stage report is not pinned by the baseline plan')
 source_inputs={str(baseline):baseline_sha,str(stage_report):digest(stage_report)}
 outputs={}
 for key,value in stage.get('outputs',{}).items():
  key=key.replace('\\','/');parts=relative_name(key);require(parts[0] in PACKAGES and len(parts)>=2 and hex_digest(value),'unapproved staged output')
  require(key.casefold() not in {v.casefold() for v in outputs},'case-colliding staged output');outputs[key]=value
 require(outputs and {PurePosixPath(k).parts[0] for k in outputs}==PACKAGES,'stage must contain all three complete packages')
 desired={};expected_package_keys=set();example_count=0
 for row in plan.get('newPackageFiles',[])+plan.get('newMaintainedExampleFiles',[]):
  if row.get('platform')!=platform:continue
  target=row.get('target');parts=scoped_target(target,platform);require(hex_digest(row.get('sha256')),'invalid desired hash')
  key='/'.join(parts[3:]) if parts[2]=='extlibs' else '/'.join((parts[3],'examples',*parts[4:]))
  if parts[2]=='extlibs':expected_package_keys.add(key)
  else:example_count+=1
  require(outputs.get(key)==row['sha256'],'desired file disagrees with the normal stage: '+target)
  source=packages.joinpath(*PurePosixPath(key).parts);no_links(packages,source);require(source.is_file() and digest(source)==row['sha256'],'staged source SHA mismatch: '+str(source));source_inputs[str(source)]=row['sha256']
  require(target.casefold() not in {v.casefold() for v in desired},'duplicate/case-colliding desired target')
  desired[target]=dict(source=str(source),after=row['sha256'])
 require(expected_package_keys==set(outputs),'baseline does not cover every staged package file')
 require(example_count==13,'expected exactly 13 maintained example/README files')
 old={}
 for row in plan.get('oldPackageFiles',[])+plan.get('oldVisibleExampleFiles',[]):
  target=row.get('currentPath','')
  if not target.startswith('release/'+platform+'/'):continue
  scoped_target(target,platform);require(row.get('canonicalLFMatchesGit') and hex_digest(row.get('sha256')),'old target lacks pinned Git proof: '+target)
  require(target.casefold() not in {v.casefold() for v in old},'duplicate/case-colliding old target');old[target]=row['sha256']
 require(platform!='linux_x64' or not old,'the baseline must not invent old Linux delete targets')
 operations=[];platform_root=root/'release'/platform
 for target in sorted(set(old)|set(desired)):
  path=root.joinpath(*scoped_target(target,platform));no_links(root,path);require(beneath(path,platform_root),'target escaped platform directory');before=current_hash(path);wanted=desired.get(target,{}).get('after')
  if target in old:require(before==old[target] or (wanted is not None and before==wanted),'old target SHA mismatch/missing: '+target)
  else:require(before is None or before==wanted,'unknown target would be overwritten: '+target)
  operations.append(dict(target=target,before=before,after=wanted,source=desired.get(target,{}).get('source'),action='noop' if before==wanted else 'retire' if wanted is None else 'replace' if before is not None else 'create',state='pending'))
 return dict(root=str(root),platform=platform,platformRoot=str(platform_root),packages=str(packages),stageReport=str(stage_report),baseline=str(baseline),baselineSha256=baseline_sha,sourceInputs=source_inputs,operations=operations,workDir=str(work_dir))

def copy_verified(source,target,expected):
 target.parent.mkdir(parents=True,exist_ok=True)
 with Path(source).open('rb') as reader,target.open('xb') as writer:
  shutil.copyfileobj(reader,writer,1024*1024);writer.flush();os.fsync(writer.fileno())
 shutil.copystat(source,target,follow_symlinks=False)
 require(digest(target)==expected,'copied file SHA mismatch: '+str(target))

def write_local(prepared,target,expected,root,journal,save):
 no_links(root,target.parent)
 missing=[];parent=target.parent
 while not parent.exists():missing.append(parent);parent=parent.parent
 for directory in reversed(missing):directory.mkdir();journal['createdDirectories'].append(str(directory));save()
 no_links(root,target)
 temporary=target.with_name('.__xge3_publish_'+uuid.uuid4().hex+'.tmp');journal['temporaryFiles'].append(str(temporary));save()
 copy_verified(prepared,temporary,expected);no_links(root,target)
 os.replace(temporary,target);require(digest(target)==expected,'final target SHA mismatch: '+str(target))
 journal['temporaryFiles'].remove(str(temporary));save()

def rollback(journal,save):
 root=Path(journal['root']);work=Path(journal['workDir']);failures=[]
 for operation in reversed(journal['operations']):
  if operation['action']=='noop' or operation['state'] not in ('applying','applied'):continue
  target=root.joinpath(*PurePosixPath(operation['target']).parts)
  try:
   no_links(root,target);present=current_hash(target)
   require(present in (operation['before'],operation['after']),'rollback refuses a concurrent unknown target: '+str(target))
   if present!=operation['before']:
    if operation['before'] is None:
     if target.exists():target.unlink()
    else:
     backup=work/'backups'/operation['target'];require(backup.is_file() and digest(backup)==operation['before'],'rollback backup SHA mismatch');write_local(backup,target,operation['before'],root,journal,save)
   require(current_hash(target)==operation['before'],'rollback target verification failed');operation['state']='rolled-back';save()
  except Exception as error:failures.append(dict(target=operation['target'],error=str(error)))
 for value in list(journal['temporaryFiles']):
  try:
   temporary=Path(value);no_links(root,temporary)
   if temporary.is_file():temporary.unlink()
   journal['temporaryFiles'].remove(value)
  except Exception as error:failures.append(dict(temporary=value,error=str(error)))
 for value in reversed(journal['createdDirectories']):
  try:Path(value).rmdir()
  except OSError:pass  # Unknown user files/directories are never removed.
 journal['rollbackFailures']=failures;save();return not failures

def publish(spec,apply=False,_after_mutation=None):
 root=Path(spec['root']);work=Path(spec['workDir']);work.mkdir(parents=True,exist_ok=False)
 journal={**spec,'schema':1,'apply':apply,'status':'planned','temporaryFiles':[],'createdDirectories':[],'rollbackFailures':[]}
 journal_path=work/'journal.json'
 def save():atomic_json(journal_path,journal)
 save();atomic_json(work/'plan.json',spec)
 if not apply:
  receipt=dict(schema=1,mode='plan',complete=True,applied=False,root=str(root),platform=spec['platform'],operations=len(spec['operations']),journal=str(journal_path),baselineSha256=spec['baselineSha256']);atomic_json(work/'receipt.json',receipt);return receipt
 try:
  require(all(digest(p)==h for p,h in spec['sourceInputs'].items()),'source inputs changed after preflight')
  # Prepare and hash every backup and replacement before the first target write.
  for operation in journal['operations']:
   if operation['action']=='noop':continue
   target=root.joinpath(*PurePosixPath(operation['target']).parts);no_links(root,target);require(current_hash(target)==operation['before'],'target changed after preflight: '+str(target))
   if operation['before'] is not None:copy_verified(target,work/'backups'/operation['target'],operation['before'])
   if operation['after'] is not None:copy_verified(operation['source'],work/'prepared'/operation['target'],operation['after'])
  journal['status']='applying';save();mutations=0
  for operation in journal['operations']:
   if operation['action']=='noop':continue
   target=root.joinpath(*PurePosixPath(operation['target']).parts);no_links(root,target);require(current_hash(target)==operation['before'],'target changed before mutation: '+str(target));operation['state']='applying';save()
   if operation['after'] is None:target.unlink();require(not target.exists(),'retired target still exists')
   else:write_local(work/'prepared'/operation['target'],target,operation['after'],root,journal,save)
   operation['state']='applied';mutations+=1;save()
   if _after_mutation:_after_mutation(mutations,operation)
  require(all(digest(p)==h for p,h in spec['sourceInputs'].items()),'source inputs changed while publishing')
  for operation in journal['operations']:
   target=root.joinpath(*PurePosixPath(operation['target']).parts);no_links(root,target);require(current_hash(target)==operation['after'],'final target SHA mismatch: '+str(target))
  journal['status']='committed';save();receipt=dict(schema=1,mode='apply',complete=True,applied=True,root=str(root),platform=spec['platform'],baselineSha256=spec['baselineSha256'],journal=str(journal_path),sourceInputsUnchanged=True,finalAllTargetsVerified=True,knownOldFilesRetired=sum(o['action']=='retire' for o in journal['operations']),filesWritten=sum(o['action'] in ('create','replace') for o in journal['operations']),noops=sum(o['action']=='noop' for o in journal['operations']),unknownAndHiddenFilesEnumerated=False,backupDirectory=str(work/'backups'));atomic_json(work/'receipt.json',receipt);return receipt
 except Exception as error:
  # A journal-disk error must not prevent best-effort restoration of targets.
  def rollback_save():
   try:save()
   except Exception as journal_error:journal.setdefault('journalWriteErrors',[]).append(str(journal_error))
  journal['error']=str(error);journal['status']='rolling-back';rollback_save();ok=rollback(journal,rollback_save);journal['status']='rolled-back' if ok else 'rollback-incomplete';rollback_save();receipt=dict(schema=1,mode='apply',complete=False,applied=False,rolledBack=ok,error=str(error),journal=str(journal_path),journalWriteErrors=journal.get('journalWriteErrors',[]))
  try:atomic_json(work/'receipt.json',receipt)
  except Exception as receipt_error:receipt['receiptWriteError']=str(receipt_error)
  raise RuntimeError(json.dumps(receipt)) from error

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 for name in ('root','packages','stage-report','baseline-plan','work-dir'):parser.add_argument('--'+name,type=Path,required=True)
 parser.add_argument('--platform',choices=('win_x64','linux_x64'),required=True);parser.add_argument('--baseline-sha256',default=PLAN_R4_SHA256);parser.add_argument('--apply',action='store_true');args=parser.parse_args()
 try:
  spec=preflight(args.root,args.platform,args.packages,args.stage_report,args.baseline_plan,args.baseline_sha256,args.work_dir);print(json.dumps(publish(spec,args.apply),ensure_ascii=False))
 except Exception as error:print(str(error),file=sys.stderr);return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
