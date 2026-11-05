"""Assemble an isolated source candidate from immutable inputs, never release.

Project source comes from an explicit Git commit, never the working tree.
Historical base files are copied only through an explicit material allowlist;
ordered SHA-pinned layers supply later SDK/package/native material. Cache and
untracked validation output directories are not inherited. No publication or
build occurs. Receipts stay outside the complete frozen inventory.
"""
from pathlib import Path,PurePosixPath
import argparse,hashlib,json,os,shutil,subprocess,tarfile

ROOTS=('src','lib','tools','test','docs','res','cgenrule','AGENTS.md',
 'LICENSE','LICENSES','THIRD_PARTY_NOTICES.md','README.md','README_zh.md',
 'build.bat','build.sh','build_dll.bat','build_linux.sh')
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def safe(name):
 p=PurePosixPath(name)
 if p.is_absolute()or '..'in p.parts or not p.parts or ':'in p.parts[0]:raise ValueError('Unsafe relative path: '+name)
 return p
def inventory(root):return {p.relative_to(root).as_posix():sha(p)for p in sorted(root.rglob('*'))if p.is_file()}
def excluded(name):
 p=PurePosixPath(name)
 return '__pycache__'in p.parts or '.pytest_cache'in p.parts or p.suffix.lower()in('.pyc','.pyo')
def copy_link(source,target):
 try:os.link(source,target)
 except OSError:shutil.copy2(source,target)

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('repo','base','out','receipt'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--commit',required=True);p.add_argument('--layer',type=Path,action='append',default=[])
 p.add_argument('--base-material',action='append',default=[],help='Explicit relative file/directory from historical base; default copies none')
 p.add_argument('--root',action='append',dest='roots');p.add_argument('--plan-only',action='store_true')
 a=p.parse_args();repo,base,out,receipt=[v.resolve()for v in (a.repo,a.base,a.out,a.receipt)]
 assert base.is_dir()and not out.exists()and not out.is_relative_to(base)and not receipt.is_relative_to(out)
 assert not out.is_relative_to(repo/'release')and not receipt.is_relative_to(repo/'release')
 commit=subprocess.check_output(['git','-C',str(repo),'rev-parse',a.commit+'^{commit}'],text=True).strip()
 roots=a.roots or list(ROOTS);listing=subprocess.check_output(['git','-C',str(repo),'ls-tree','-rz','--full-tree',commit,'--',*roots])
 gitFiles={}
 for raw in listing.split(b'\0'):
  if not raw:continue
  metadata,name=raw.split(b'\t',1);mode,kind,oid=metadata.decode().split();name=name.decode('utf-8');safe(name)
  assert kind=='blob'and mode in ('100644','100755'),('Non-regular source input',name,mode)
  if not excluded(name):gitFiles[name]=oid
 materials={};excludedBase=[]
 for name in a.base_material:
  relative=safe(name);origin=base/relative
  assert origin.exists(),origin
  for source in sorted(origin.rglob('*'))if origin.is_dir()else[origin]:
   if not source.is_file():continue
   assert not source.is_symlink(),source
   target=source.relative_to(base).as_posix()
   if excluded(target):excludedBase.append(target);continue
   materials[target]=sha(source)
 layers=[]
 for file in a.layer:
  data=json.loads(file.read_text(encoding='utf-8-sig'));source=Path(data.get('sourceRoot',file.parent)).resolve();prefix=data.get('targetPrefix','')
  rows=[]
  for row in data['files']:
   relative=row['path'];target=(safe(prefix)/safe(relative)).as_posix()if prefix else safe(relative).as_posix()
   assert not excluded(target),'Cache bytes cannot enter a source/SDK layer: '+target
   origin=source/row.get('source',relative);digest=row.get('sha256',row.get('Sha256'));assert digest and sha(origin)==digest.lower(),origin
   rows.append(dict(source=str(origin),target=target,sha256=digest.lower()))
  deletes=[safe(v).as_posix()for v in data.get('deletes',[])];layers.append(dict(manifest=str(file.resolve()),manifestSha256=sha(file),files=rows,deletes=deletes))
 plan=dict(scope=__doc__,mode='plan'if a.plan_only else'assembly',base=str(base),out=str(out),commit=commit,roots=roots,gitFiles=gitFiles,layers=layers,
  baseMaterialAllowlist=a.base_material,baseMaterialHashes=materials,excludedCacheFiles=excludedBase,
  publishing=False,buildExecuted=False,releaseWritten=False)
 receipt.parent.mkdir(parents=True,exist_ok=True)
 if a.plan_only:receipt.write_text(json.dumps(plan,indent=2)+'\n');print('Assembly plan:',receipt);return
 out.mkdir(parents=True)
 for name,digest in materials.items():
  source=base/name;assert sha(source)==digest,source
  target=out/name;target.parent.mkdir(parents=True,exist_ok=True);copy_link(source,target)
 process=subprocess.Popen(['git','-C',str(repo),'archive','--format=tar',commit,'--',*roots],stdout=subprocess.PIPE)
 written=set();archiveConversions=[]
 try:
  with tarfile.open(fileobj=process.stdout,mode='r|')as archive:
   for member in archive:
    if member.isdir():continue
    assert member.isfile(),member.name
    relative=safe(member.name).as_posix()
    if excluded(relative):continue
    assert relative in gitFiles
    data=archive.extractfile(member).read();oid=hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
    if oid!=gitFiles[relative]:
     # archive applies attributes (including EOL/export substitutions). Source
     # qualification needs the actual committed blob, not checkout conversion.
     archiveConversions.append(relative)
     data=subprocess.check_output(['git','-C',str(repo),'cat-file','blob',gitFiles[relative]])
     assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==gitFiles[relative],relative
    target=out/relative;target.parent.mkdir(parents=True,exist_ok=True);target.unlink(missing_ok=True);target.write_bytes(data)
    if member.mode&0o111:target.chmod(target.stat().st_mode|0o111)
    written.add(relative)
  assert process.wait()==0
  for relative in sorted(set(gitFiles)-written):
   data=subprocess.check_output(['git','-C',str(repo),'cat-file','blob',gitFiles[relative]])
   assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==gitFiles[relative],relative
   target=out/relative;target.parent.mkdir(parents=True,exist_ok=True);target.unlink(missing_ok=True);target.write_bytes(data);written.add(relative)
   archiveConversions.append(relative)
  assert written==set(gitFiles)
 except BaseException:
  process.kill();process.wait();raise
 for layer in layers:
  assert sha(layer['manifest'])==layer['manifestSha256']
  for row in layer['files']:
   assert sha(row['source'])==row['sha256'];target=out/row['target'];target.parent.mkdir(parents=True,exist_ok=True);target.unlink(missing_ok=True);shutil.copy2(row['source'],target)
  for relative in layer['deletes']:
   target=out/relative;assert target.is_file(),'Only explicit file deletions, never recursive trees';target.unlink()
 for name,digest in materials.items():assert sha(base/name)==digest,'Base material changed: '+name
 final=inventory(out);assert not any(excluded(name)for name in final)
 plan.update(complete=True,passed=True,files=final,baseMaterialsUnchanged=True,baseMaterialCount=len(materials),
  noImplicitBaseClone=True,noUntrackedSourceInputs=True,archiveAttributeConversionsReplacedWithExactBlobs=archiveConversions)
 receipt.write_text(json.dumps(plan,indent=2)+'\n');print('Frozen source:',out,'files',len(plan['files']))
if __name__=='__main__':main()
