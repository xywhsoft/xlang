"""Observe the exact experimental SDK preprocessing closure and retain source notices."""
import argparse,hashlib,json,re,shlex,subprocess,sys,time
from pathlib import Path,PurePosixPath

def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
def write(p,v):
 assert not p.exists();p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
def relative(s):
 p=PurePosixPath(s)
 if p.is_absolute() or '..' in p.parts or ':' in s or '\\' in s:raise ValueError('Invalid inventory path: '+s)
 return Path(*p.parts)
def plain(p):
 if not p.is_file():raise ValueError('Missing input: '+str(p))
 for part in (p,*p.parents):
  if part.is_symlink() or part.stat().st_file_attributes&0x400:raise ValueError('Reparse input: '+str(part))
def run(a):
 sdk=a.sdk_root.resolve();out=a.output.absolute();parent=read(a.parent_materials);build=read(a.build_report);consumer=read(sdk/'sdk_manifest.json')
 if out.exists() or out.is_relative_to(sdk):raise ValueError('Use a fresh output outside the consumer SDK')
 if build['status']!='sdk-build-finished' or build['exitCode']!=0 or not build['wholeSdkWerror']:raise ValueError('Completed strict SDK build required')
 if sha(Path(build['sourceManifest']))!=build['sourceManifestSha256']:raise ValueError('Source build manifest differs')
 if not consumer.get('privateCandidate') or consumer['provenance']['sourceBuildReportSha256']!=sha(a.build_report):raise ValueError('Consumer SDK build provenance differs')
 info=read(Path(build['sourceManifest']));source=Path(info['source']).resolve()
 if out.is_relative_to(source):raise ValueError('Output must be outside source inputs')
 for part in out.parents:
  if part.exists() and (part.is_symlink() or part.stat().st_file_attributes&0x400):raise ValueError('Reparse output parent')
 if parent['format']!='xlang-ide-sdk-materials-1' or parent['legalDistributionReviewComplete']:raise ValueError('Expected incomplete parent source materials')
 parentManifest=read(sdk/'provenance/parent-sdk-manifest.json')
 parentFiles={r['path']:r['sha256'] for r in parentManifest['files']}
 if parent['sdkDllSha256']!=parentFiles['xge.dll'] or parent['sourceManifestSha256']!=parentFiles['xge-source-files.json']:raise ValueError('Parent materials refer to another SDK')
 inventory={r['path']:r for r in info['files']}
 descriptor=read(sdk/'xge-source-files.json')
 if descriptor['files']!=info['files'] or descriptor['sourceFileCount']!=len(inventory):raise ValueError('Consumer source descriptor differs')
 def verify():
  for name,r in inventory.items():
   p=source/relative(name);plain(p)
   if sha(p)!=r['sha256']:raise ValueError('Source input changed: '+name)
  for r in consumer['files']:
   p=sdk/relative(r['path']);plain(p)
   if sha(p)!=r['sha256']:raise ValueError('Consumer payload changed: '+r['path'])
  for r in build['outputs']:
   p=Path(r['path']);plain(p)
   if sha(p)!=r['sha256']:raise ValueError('Actual build artifact changed')
 verify()
 batch=Path(build['command'][3]);plain(batch)
 lines=batch.read_text(encoding='utf-8-sig').splitlines()
 values=[line[len('set FLAGS='):] for line in lines if line.startswith('set FLAGS=')]
 if len(values)!=1 or values[0].count('%PROFILE_FLAGS%')!=1:raise ValueError('Unexpected executed compiler flags')
 flags=shlex.split(values[0].replace('%PROFILE_FLAGS%','-DXGE_XRT_PROFILE_IDE'))
 if '-Werror' not in flags or '-DXGE_BUILD_DLL' not in flags or '-DXUI_BUILD_DLL' not in flags:raise ValueError('Unexpected strict source configuration')
 observed={'xge.c'}
 for name in ('xui_sources.bat','xui_document_sources.bat'):
  for line in (source/name).read_text(encoding='utf8').splitlines():
   if line.startswith('set '):observed.update(s.replace('\\','/') for s in re.findall(r'[\w./\\-]+\.c\b',line))
 units=parent['translationUnits']
 if set(units)!=observed or len(units)!=len(observed):raise ValueError('Translation unit entrypoints differ from parent inventory')
 out.mkdir(parents=True);commands=[];dependencies=set();started=time.monotonic()
 version=subprocess.run([str(a.gcc),'--version'],capture_output=True,check=True,text=True).stdout
 (out/'compiler-version.txt').write_text(version,encoding='utf8')
 if build['compiler'] not in version:raise ValueError('Dependency compiler differs from actual build compiler')
 for number,unit in enumerate(units):
  dep=out/('unit-%03d.d'%number);log=out/('unit-%03d.log'%number)
  args=[str(a.gcc),*flags,'-I.','-MM','-MT','xide_unit_%03d.o'%number,'-MF',str(dep),unit]
  began=time.monotonic()
  with log.open('xb') as f:r=subprocess.run(args,cwd=source,stdout=f,stderr=f,timeout=120)
  row=dict(unit=unit,arguments=args,exitCode=r.returncode,elapsedSeconds=round(time.monotonic()-began,3),dependencyFile=dep.name,log=log.name,logSha256=sha(log));commands.append(row)
  if r.returncode:raise RuntimeError('Dependency observation failed: '+str(log))
  text=re.sub(r'\\\r?\n',' ',dep.read_text(encoding='utf8'));head,body=text.split(':',1)
  if head!='xide_unit_%03d.o'%number:raise ValueError('Unexpected dependency target')
  for token in re.findall(r'(?:\\[ \t#:]|[^\s])+',body):
   name=token.replace('\\ ',' ').replace('\\#','#').replace('\\:',':').replace('\\','/')
   resolved=(source/name).resolve()
   if not resolved.is_relative_to(source):raise ValueError('Preprocessor dependency escapes source inventory: '+name)
   name=resolved.relative_to(source).as_posix()
   if name not in inventory:raise ValueError('Preprocessor dependency outside verified source inventory: '+name)
   dependencies.add(name)
  row['dependencySha256']=sha(dep)
 if not set(units)<=dependencies:raise ValueError('Translation unit missing from dependencies')
 for name in parent['buildEntrypoints']:
  if name not in inventory:raise ValueError('Build/resource entrypoint missing: '+name)
 for row in parent['materials']:
  if row['source'] not in inventory:raise ValueError('Named or embedded notice source missing')
 verify()
 files=[dict(path=name,sha256=inventory[name]['sha256'],size=(source/relative(name)).stat().st_size) for name in sorted(inventory)]
 result=dict(format='xlang-ide-sdk-materials-1',sourceManifestSha256=sha(sdk/'xge-source-files.json'),sdkDllSha256=sha(sdk/'xge.dll'),sourceScope='Exact verified SDK48 source inventory including observed IDE-profile preprocessing closure, build/resource entrypoints, pinned library sources/notices and experimental helper sources; complete distribution review remains pending',translationUnits=units,preprocessorDependencies=sorted(dependencies),buildEntrypoints=parent['buildEntrypoints'],materials=parent['materials'],files=files,legalDistributionReviewComplete=False,privateCandidate=True,formalRelease=False,sourceBuildReportSha256=sha(a.build_report),sourceBuildManifestSha256=sha(Path(build['sourceManifest'])),parentMaterialsSha256=sha(a.parent_materials),dependencyConfigurationFlags=flags)
 spec=out/'xge-sdk-materials.json';write(spec,result)
 report=dict(status='exact-private-sdk-source-and-preprocessing-materials-observed',root=str(out),source=str(source),sdkRoot=str(sdk),compiler=str(a.gcc),compilerVersion=version.splitlines()[0],executedBatch=str(batch),executedBatchSha256=sha(batch),sourceFiles=len(files),translationUnits=len(units),preprocessorDependencyFiles=len(dependencies),noticeSources=len(parent['materials']),commands=commands,elapsedSeconds=round(time.monotonic()-started,3),descriptor=str(spec),descriptorSha256=sha(spec),inputsUnchanged=True,productionChanged=False,sdkInstalled=False,legalDistributionReviewComplete=False,formalRelease=False)
 write(out/'result.json',report)
 print(json.dumps(dict(status=report['status'],sourceFiles=len(files),translationUnits=len(units),preprocessorDependencyFiles=len(dependencies),elapsedSeconds=report['elapsedSeconds'])),flush=True)

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for flag in ('sdk-root','build-report','parent-materials','output'):p.add_argument('--'+flag,type=Path,required=True)
 p.add_argument('--gcc',type=Path,default=Path(r'E:\software\w64devkit\bin\gcc.exe'))
 try:run(p.parse_args())
 except Exception as e:print(type(e).__name__+': '+str(e),file=sys.stderr);raise SystemExit(1)
