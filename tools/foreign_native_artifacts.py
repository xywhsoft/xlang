"""Read exact Native image names from build metadata and the staged manifest.

Legacy reports without runtime_image/import_library retain their historical
names. A supplied metadata field is never ignored in favor of a nearby DLL.
This helper identifies artifacts; it does not qualify a synthetic SDK.
"""
from pathlib import Path
import hashlib,json,os

def native_artifacts(packages,engine_build):
 packages,engine_build=Path(packages).resolve(),Path(engine_build).resolve()
 manifest_path=packages/'_xge_native/xlang.package.json';manifest=json.loads(manifest_path.read_text(encoding='utf-8'));native=manifest.get('native',{});build_path=engine_build/'build-report.json';build=json.loads(build_path.read_text(encoding='utf-8'))
 load_name=native.get('loadName')
 if load_name is None:load_name='xge.dll' if os.name=='nt' else 'libxge.so'
 assert isinstance(load_name,str) and load_name and Path(load_name).name==load_name and '/' not in load_name and '\\' not in load_name
 def at_build(value):
  assert isinstance(value,str) and value
  path=Path(value);path=(path if path.is_absolute() else engine_build/path).resolve()
  assert path.parent==engine_build and path.is_file(),path
  return path
 image=at_build(build['runtime_image']) if 'runtime_image' in build else at_build(load_name)
 assert image.name==load_name,'Native build image and staged loadName disagree'
 staged=packages/'_xge_native/native'/load_name;assert staged.is_file()
 digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
 assert digest(image)==digest(staged),'staged Native image differs from the build'
 if native.get('sha256') is not None:assert native['sha256']==digest(staged)
 windows=load_name.lower().endswith('.dll')
 if 'import_library' in build and build['import_library'] is not None:library=at_build(build['import_library'])
 elif windows:
  assert 'import_library' not in build,'Windows metadata omitted its declared import library'
  library=at_build(image.with_suffix('.lib').name)
 else:library=image
 return dict(build=build,buildPath=build_path,manifest=manifest,manifestPath=manifest_path,
             image=image,stagedImage=staged,library=library,loadName=load_name,
             legacyMetadata='runtime_image' not in build or 'import_library' not in build)
