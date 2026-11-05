"""Test metadata artifact selection only; synthetic files are never executed."""
from pathlib import Path
import argparse,hashlib,json
from foreign_native_artifacts import native_artifacts

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--legacy-packages',type=Path);p.add_argument('--legacy-engine',type=Path);a=p.parse_args();assert bool(a.legacy_packages)==bool(a.legacy_engine);out=a.out.resolve();out.mkdir(parents=True,exist_ok=False);rows=[];sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
 def case(name,windows=True,legacy=False,absolute=False,invalid=None):
  base=out/name;engine=base/'build';packages=base/'packages';private=packages/'_xge_native';(private/'native').mkdir(parents=True);engine.mkdir();load=('xge.dll' if windows else 'libxge.so') if legacy else ('xge3_host.dll' if windows else 'libxge3_host.so');image=engine/load;image.write_bytes(b'non-executable metadata fixture');staged=private/'native'/load;staged.write_bytes(image.read_bytes());library=engine/'xge3_host.lib' if windows else image
  if windows:library=engine/('xge.lib' if legacy else 'xge3_host.lib');library.write_bytes(b'non-executable import fixture')
  metadata={} if legacy else dict(runtime_image=str(image) if absolute else image.name,import_library=(str(library) if absolute else library.name) if windows else None)
  manifest=dict(native=dict(loadName=load,sha256=sha(staged)))
  if not legacy:
   (engine/('xge.dll' if windows else 'libxge.so')).write_bytes(b'wrong colliding legacy image')
   if windows:(engine/'xge.lib').write_bytes(b'wrong colliding legacy import')
  if invalid=='runtime-empty':metadata['runtime_image']=''
  elif invalid=='runtime-null':metadata['runtime_image']=None
  elif invalid=='different-load-name':manifest['native']['loadName']='different.dll'
  elif invalid=='import-null':metadata['import_library']=None
  elif invalid=='outside-import':outside=base/'outside.lib';outside.write_bytes(b'outside');metadata['import_library']=str(outside)
  elif invalid=='outside-image':outside=base/load;outside.write_bytes(image.read_bytes());metadata['runtime_image']=str(outside)
  elif invalid=='staged-mismatch':staged.write_bytes(b'different bytes')
  elif invalid=='load-path':manifest['native']['loadName']='../'+load
  (engine/'build-report.json').write_text(json.dumps(metadata));(private/'xlang.package.json').write_text(json.dumps(manifest));rejected=False
  try:result=native_artifacts(packages,engine)
  except AssertionError:rejected=True
  assert rejected==bool(invalid),name
  if not invalid:assert result['image']==image and result['library']==library and result['legacyMetadata']==legacy
  rows.append(dict(name=name,rejected=rejected,synthetic=True,nativeExecuted=False))
 for name,options in [('new-win',{}),('new-linux',{'windows':False}),('absolute-win',{'absolute':True}),('legacy-win',{'legacy':True}),('legacy-linux',{'legacy':True,'windows':False})]:case(name,**options)
 for invalid in ('runtime-empty','runtime-null','different-load-name','import-null','outside-import','outside-image','staged-mismatch','load-path'):case('reject-'+invalid,invalid=invalid)
 inputs={str(Path(__file__).resolve()):sha(__file__),str(Path(__file__).with_name('foreign_native_artifacts.py').resolve()):sha(Path(__file__).with_name('foreign_native_artifacts.py'))}
 if a.legacy_packages:
  artifacts=native_artifacts(a.legacy_packages,a.legacy_engine);assert artifacts['legacyMetadata'];paths=(artifacts['manifestPath'],artifacts['buildPath'],artifacts['image'],artifacts['stagedImage'],artifacts['library']);inputs.update((str(f),sha(f)) for f in paths);rows.append(dict(name='real-legacy-normal-stage-read',synthetic=False,nativeExecuted=False,loadName=artifacts['loadName'],imageSha256=sha(artifacts['image'])))
 report=dict(scope=__doc__,complete=True,passed=True,runtimeQualified=False,rows=rows,inputs=inputs,inputsUnchanged=all(sha(f)==h for f,h in inputs.items()));assert report['inputsUnchanged'];(out/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(len(rows),'metadata checks passed')
if __name__=='__main__':main()
