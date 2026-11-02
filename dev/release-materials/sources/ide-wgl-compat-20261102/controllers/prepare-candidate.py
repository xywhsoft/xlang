from pathlib import Path
import hashlib,json,shutil,subprocess,sys,difflib
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;S=O/'source';E=Path('E:/codex-builds-xlang-wgl-compat-20261102');E.mkdir(exist_ok=False)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,z):p.write_text(json.dumps(z,indent=2)+'\n',encoding='utf8')
parent=R/'ide/third_party/xge_sdk';sdk=E/'sdk';manifest=json.loads((parent/'sdk_manifest.json').read_text());freeze=json.loads((O/'source-freeze.json').read_text())
for row in manifest['files']:assert sha(parent/row['path'])==row['sha256'],row['path']
for row in freeze['files']:assert sha(S/row['path'])==row['sha256'],row['path']
shutil.copytree(parent,sdk)
for name in ['xge.dll','xge.lib','xge_build_config.h']:shutil.copyfile(S/'build/ide'/name,sdk/name)
for name in ['xge.h','xui.h','xui_document.h','xui_document_ui.h','xge_config.h','xui_config.h']:assert sha(parent/name)==sha(S/'include'/name),name
materials=sdk/'provenance/wgl-context-v1';materials.mkdir()
for name in ['sdk_manifest.json','xge-source-files.json','provenance/source-build-report.json','provenance/source-build-manifest.json']:
 p=materials/('parent-'+Path(name).name);shutil.copyfile(parent/name,p)
shutil.copyfile(R/'ide/tools/wgl_context_overlay.py',materials/'wgl_context_overlay.py');shutil.copyfile(O/'overlay.json',materials/'overlay.json');shutil.copyfile(O/'source-freeze.json',materials/'source-freeze.json');shutil.copyfile(S/'build/ide/build-report.json',materials/'upstream-build-report.json')
old=json.loads((parent/'xge-source-files.json').read_text());oldRoot=Path(old['root'])
for row in json.loads((O/'overlay.json').read_text())['files']:
 name=row['path'];before=oldRoot/name;after=S/name
 patch=''.join(difflib.unified_diff(before.read_text('utf8').splitlines(True),after.read_text('utf8').splitlines(True),fromfile='a/'+name,tofile='b/'+name))
 (materials/(Path(name).name+'.patch')).write_text(patch,encoding='utf8')
descriptor=dict(old,root=str(S),files=freeze['files'],sourceFileCount=len(freeze['files']),wglContextOverlay='provenance/wgl-context-v1/overlay.json')
write(sdk/'xge-source-files.json',descriptor);write(sdk/'provenance/source-build-manifest.json',descriptor)
build=dict(status='sdk-build-finished',exitCode=0,profile='ide',source=str(S),sourceCommit=old['sourceCommit'],sourceUnchanged=True,wholeSdkWerror=False,upstreamBuild=json.loads((S/'build/ide/build-report.json').read_text()),overlay='provenance/wgl-context-v1/overlay.json',outputs=[dict(path=str(S/'build/ide'/n),sha256=sha(S/'build/ide'/n)) for n in ['xge.dll','xge.lib','xge_build_config.h']])
write(sdk/'provenance/source-build-report.json',build)
manifest['provenance']=dict(manifest['provenance'],wglContextOverlay='provenance/wgl-context-v1/overlay.json',sourceBuildReportSha256=sha(sdk/'provenance/source-build-report.json'),sourceBuildManifestSha256=sha(sdk/'provenance/source-build-manifest.json'),currentCompiler=build['upstreamBuild']['compiler'][2],sourceWorktreeDirty=True)
manifest['files']=[dict(path=p.relative_to(sdk).as_posix(),sha256=sha(p)) for p in sorted(sdk.rglob('*')) if p.is_file() and p!=sdk/'sdk_manifest.json'];write(sdk/'sdk_manifest.json',manifest)
protected={str(R/'release'/n):sha(R/'release'/n) for n in ['xlang_ide.exe','xge.dll','xlang-ls.exe','xl.exe','xl.dll','xlc.dll','option/settings.json','data/commands.json','data/catalog-sources/index.json']}
write(E/'baseline.json',dict(protected=protected,parentSdkManifestSha256=sha(parent/'sdk_manifest.json'),sourceFreezeSha256=sha(O/'source-freeze.json')))
print('prepared',sdk,'DLL',sha(sdk/'xge.dll'))
subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(R/'ide/tools/verify_xge_sdk.ps1'),'-SdkRoot',str(sdk)],cwd=R,check=True)
