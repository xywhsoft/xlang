"""Prepare a verified source copy with the configured TCC bitfield query."""
import argparse,base64,hashlib,json,shutil
from pathlib import Path
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def prepare(seed,manifest,output):
 seed,manifest,output=Path(seed).resolve(),Path(manifest).resolve(),Path(output).absolute()
 patch=Path(__file__).resolve().parent.parent/'third_party/compiler_patches/native-c-record-layout-v1/changes.json'
 record=json.loads(patch.read_text(encoding='utf-8'));assert record['format']=='xlang-ide-private-compiler-patch-1'
 assert sha(manifest)==record['seedSourceManifestSha256']
 rows=json.loads(manifest.read_text(encoding='utf-8'))['files'];assert len(rows)==record['seedSourceFiles'] and not output.exists()
 assert not output.resolve().is_relative_to(seed) and not seed.is_relative_to(output.resolve())
 expected={r['path']:r['sha256'] for r in rows+record['rootBuildScripts']};replacements={}
 for relative,digest in expected.items():
  p=seed/relative;assert p.resolve().is_relative_to(seed) and not p.is_symlink() and sha(p)==digest,relative
 for row in record['changedFiles']:
  data=(seed/row['path']).read_bytes();assert hashlib.sha256(data).hexdigest()==row['beforeSha256'];parts=[];cursor=0
  for change in row['edits']:
   at=change['offset'];old=base64.b64decode(change['removeBase64'],validate=True);new=base64.b64decode(change['insertBase64'],validate=True)
   assert cursor<=at<=len(data) and data[at:at+len(old)]==old
   parts.extend((data[cursor:at],new));cursor=at+len(old)
  result=b''.join(parts+[data[cursor:]]);assert hashlib.sha256(result).hexdigest()==row['afterSha256'];replacements[row['path']]=result
 output.mkdir(parents=True)
 for name in expected:
  p=output/name;p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(seed/name,p)
 for name,data in replacements.items():(output/name).write_bytes(data)
 expected.update({r['path']:r['afterSha256'] for r in record['changedFiles']})
 for name,digest in expected.items():assert sha(output/name)==digest,name
 for r in rows+record['rootBuildScripts']:assert sha(seed/r['path'])==r['sha256'],r['path']
 report={'format':'xlang-ide-prepared-native-record-compiler-1','seed':str(seed),'sourceManifest':str(manifest),
  'sourceManifestSha256':sha(manifest),'patchSha256':sha(patch),'files':[{'path':n,'sha256':d} for n,d in expected.items()],
  'prepared':True,'built':False,'formalRelease':False}
 (output/'prepared-native-record-source.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 print('Prepared',len(expected),'verified compiler inputs in',output)
 return report
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('seed','manifest','output'):p.add_argument('--'+n,required=True)
 a=p.parse_args();prepare(a.seed,a.manifest,a.output)
