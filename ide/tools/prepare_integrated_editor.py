"""Copy frozen application inputs and verified editor/SDK overlays to a private tree."""
import argparse,hashlib,json,shutil,sys
from pathlib import Path,PurePosixPath

def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
def write(p,v):
 assert not p.exists();p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
def plain(p):
 if not p.is_file():raise ValueError('Missing regular file: '+str(p))
 for part in (p,*p.parents):
  if part.is_symlink() or part.stat().st_file_attributes&0x400:raise ValueError('Reparse input path: '+str(part))
def relative(s):
 p=PurePosixPath(s)
 if p.is_absolute() or '..' in p.parts or ':' in s or '\\' in s:raise ValueError('Invalid relative path: '+s)
 return Path(*p.parts)
def run(a):
 ide=a.ide_root.resolve();out=a.output.absolute();base=read(a.baseline);sdk=a.sdk_root.resolve();overlay=read(a.editor_overlay)
 if out.exists() or not out.is_relative_to(ide/'build') or out.is_relative_to(sdk):raise ValueError('Use a fresh directory under IDE build')
 for parent in out.parents:
  if parent.exists() and (parent.is_symlink() or parent.stat().st_file_attributes&0x400):raise ValueError('Reparse output parent')
 manifest=read(sdk/'sdk_manifest.json');prepared=read(sdk/'preparation-report.json')
 if not manifest.get('privateCandidate') or prepared['status']!='private-matched-consumer-sdk-prepared' or not prepared['inputsUnchanged']:raise ValueError('Expected matched private consumer SDK')
 if prepared['manifestSha256']!=sha(sdk/'sdk_manifest.json'):raise ValueError('SDK preparation report differs')
 if len(overlay['files'])!=10 or not overlay['privateOnly']:raise ValueError('Expected verified editor factory overlay')
 source=Path(overlay['root']);chosen=[]
 def verify():
  for r in base['saved']:
   plain(Path(r['original']));plain(Path(r['copy']))
   if sha(Path(r['original']))!=r['sha256'] or sha(Path(r['copy']))!=r['sha256']:raise ValueError('Frozen input changed: '+r['original'])
  for k in ('coreObjects','outputHashes','compilerOutputs'):
   for p,h in base[k].items():
    if sha(Path(p))!=h:raise ValueError('Protected output changed: '+p)
  for r in manifest['files']:
   p=sdk/relative(r['path']);plain(p)
   if sha(p)!=r['sha256']:raise ValueError('SDK payload changed: '+r['path'])
  for r in overlay['files']:
   p=source/relative(r['path']);plain(p)
   if sha(p)!=r['sha256']:raise ValueError('Editor overlay changed: '+r['path'])
 verify()
 for r in base['saved']:
  p=Path(r['original']);rel=p.relative_to(ide)
  if rel.parts[0] in ('src','tests','tools','data'):chosen.append((p,rel,r['sha256']))
 if len({str(r) for p,r,h in chosen})!=len(chosen):raise ValueError('Duplicate application input')
 out.mkdir(parents=True)
 def copy(p,rel,expected):
  q=out/rel;q.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,q)
  if sha(q)!=expected:raise ValueError('Input changed while copying: '+str(p))
 for p,rel,h in chosen:copy(p,rel,h)
 changes=[]
 for r in overlay['files']:
  rel=relative(r['path']);old=sha(out/rel);copy(source/rel,rel,r['sha256'])
  changes.append(dict(path=rel.as_posix(),originalSha256=old,candidateSha256=r['sha256']))
 for r in manifest['files']:copy(sdk/relative(r['path']),Path('third_party/xge_sdk')/relative(r['path']),r['sha256'])
 for name in ('sdk_manifest.json','preparation-report.json'):copy(sdk/name,Path('third_party/xge_sdk')/name,sha(sdk/name))
 build=out/'tools/build.ps1';before=sha(build);text=build.read_text(encoding='utf-8-sig')
 marker="'-DXGE_DEBUGMODE=0'"
 if text.count(marker)!=1 or 'XIDE_CURSOR_LAYOUT_ENABLED' in text:raise ValueError('Unexpected frozen build flags')
 text=text.replace(marker,marker+",'-DXIDE_STREAM_LAYOUT_ENABLED=1','-DXIDE_CURSOR_LAYOUT_ENABLED=1'")
 build.write_text(text,encoding='utf8')
 changes.append(dict(path='tools/build.ps1',originalSha256=before,candidateSha256=sha(build),reason='activate both verified editor visitor hooks'))
 files=[]
 for p in sorted(out.rglob('*')):
  if p.is_file():files.append(dict(path=p.relative_to(out).as_posix(),sha256=sha(p),size=p.stat().st_size))
 verify()
 result=dict(status='private-integrated-editor-tree-prepared',root=str(out),baseline=str(a.baseline.resolve()),baselineSha256=sha(a.baseline),originalApplicationInputFiles=len(chosen),sdkRoot=str(sdk),sdkManifestSha256=sha(sdk/'sdk_manifest.json'),editorOverlay=str(a.editor_overlay.resolve()),editorOverlaySha256=sha(a.editor_overlay),changes=changes,files=files,enabledDefines=['XIDE_STREAM_LAYOUT_ENABLED=1','XIDE_CURSOR_LAYOUT_ENABLED=1'],inputsUnchanged=True,productionChanged=False,sdkInstalled=False,formalRelease=False,portableMetadataUpdated=False,goalStatus='active')
 write(out/'preparation-report.json',result)
 print(json.dumps(dict(status=result['status'],root=str(out),files=len(files),changes=len(changes))),flush=True)

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for flag in ('ide-root','baseline','sdk-root','editor-overlay','output'):p.add_argument('--'+flag,type=Path,required=True)
 try:run(p.parse_args())
 except Exception as e:print(type(e).__name__+': '+str(e),file=sys.stderr);raise SystemExit(1)
