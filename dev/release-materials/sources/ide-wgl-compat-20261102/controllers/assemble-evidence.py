from pathlib import Path
import hashlib,json,subprocess,shutil
R=Path('D:/GIT/x-lang');E=Path(__file__).resolve().parent;O=R/'ide/build/Release/wgl-compat-20261102';S=O/'source';SDK=E/'sdk'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text('utf-8-sig'))
def receipt(p):p=Path(p);return dict(path=str(p),sha256=sha(p),bytes=p.stat().st_size)
def write(p,z):p.write_text(json.dumps(z,ensure_ascii=False,indent=2)+'\n',encoding='utf8',newline='\n')
b=read(E/'gui-build.json');v=read(E/'gui-validation-v2.json');w=read(E/'wgl-check.json');pub=read(E/'publication.json');base=read(E/'baseline.json');freeze=read(O/'source-freeze.json');up=read(S/'build/ide/build-report.json')
assert b['status']==v['status']==w['status']=='passed' and pub['status']=='passed-targeted-validation'
for row in freeze['files']:assert sha(S/row['path'])==row['sha256'],row['path']
for p,d in b['inputs'].items():assert sha(p)==d,p
for p,d in base['protected'].items():
 if Path(p).name not in ['xge.dll','xlang_ide.exe']:assert sha(p)==d,p
for name,digest in [('xlang_ide.exe',pub['rootGuiSha256']),('xge.dll',pub['rootXgeSha256'])]:assert sha(R/'release'/name)==digest
for record in [b,v,w,pub,read(E/'gui-validation.json')]:
 for row in record['commands']:
  if 'sha256' in row:assert sha(row['log'])==row['sha256'],row['log']
for row in v['faults']:assert sha(row['diagnostic'])==row['sha256']
manifest=read(SDK/'sdk_manifest.json')
for row in manifest['files']:
 assert sha(SDK/row['path'])==row['sha256'];assert sha(R/'ide/third_party/xge_sdk'/row['path'])==row['sha256']
assert len(up['compiled'])==165 and not up['reused'] and len(w['cases'])==8 and len(v['faults'])==7 and len(v['tests'])==6
normal=dict(b,release=str(R/'release'),buildMode='matched-windows-gl33-startup-repair',catalogStatus='pre-existing-stale-binding-preserved;standard-build-guard-not-passed',installedSdk=str(R/'ide/third_party/xge_sdk'),installedSdkManifestSha256=sha(R/'ide/third_party/xge_sdk/sdk_manifest.json'))
normalPath=R/'ide/build/Release/gui-build-inputs.json';write(normalPath,normal)
H=R/'dev/release-materials/sources/ide-wgl-compat-20261102';H.mkdir(exist_ok=False);entries=[]
def keep(p,rel,role):
 p=Path(p);dest=H/rel;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(p.read_bytes());assert sha(dest)==sha(p)
 entries.append(dict(recordedPath=str(p),archivedPath=dest.relative_to(H).as_posix(),role=role,bytes=dest.stat().st_size,sha256=sha(dest)))
oldRoot=Path(read(O/'parent-source-descriptor.json')['root'])
for row in read(O/'overlay.json')['files']:
 keep(oldRoot/row['path'],Path('before')/row['path'],'exact pinned original XGE/Sokol source')
 keep(S/row['path'],Path('after')/row['path'],'exact compiled patched XGE/Sokol source')
for p in [R/'ide/src/main.c',R/'ide/tools/wgl_context_overlay.py',R/'ide/tests/wgl_fault_hooks.c',R/'ide/tests/wgl_fault_hooks.h',R/'ide/tests/wgl_startup_test.c']:keep(p,Path('ide-inputs')/p.relative_to(R),'IDE change or test-only WGL boundary fixture')
for p in [*sorted(E.glob('*.py')),*sorted(O.glob('*.py'))]:keep(p,Path('controllers')/p.name,'actual fixed build or validation controller')
for name in ['source-freeze.json','overlay.json','parent-source-descriptor.json','parent-sdk-manifest.json']:keep(O/name,Path('captures')/name,'original source capture')
(H/'.gitattributes').write_bytes(b'* -text -whitespace\n');write(H/'manifest.json',dict(schema=1,scope='Exact changed SDK/IDE source and fixed controllers; original build/fault/GUI reports retain paths and bytes. Not a complete SDK/engine product bundle.',entries=entries))
out=R/'dev/release-materials/provenance/ide-wgl-compat-20261102.json'
record=dict(schema=1,date='2026-11-02',status='passed-targeted-validation',task='Windows GL3.3 startup and WGL failure cleanup for IDE',sourceBaseRevision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=R,text=True).strip(),sourceModifiedFromBase=True,
 userEnvironment=dict(os='Windows 7',desktop='local physical',gpu='AMD Radeon HD 4200',vramMiB=256,ramGiB=2,physicalDeviceTested=False),
 originalCause=dict(backendName='opengl33',actualDefaultRequest='OpenGL4.3',assertion='sokol_app.h:9064,_sapp.wgl.gl_ctx,in destruction',logger='XGE returns from Sokol PANIC logging,allowing failed WGL construction to continue',matchingSourceDescriptor=receipt(O/'parent-source-descriptor.json'),oldRootDllSha256=base['protected'][str(R/'release/xge.dll')]),
 fix=dict(explicitWindowsRequest='OpenGL3.3 core',partialWglCleanup=True,noInitOrFrameOnFailedWgl=True,runReturnsError=True,firstCausePreservedViaExistingCallback=True,assertionsDisabled=False,softwareRendererAdded=False,publicHeadersUnchanged=6,xrtImplementationOrDeclarationsChanged=False,exportNamesUnchanged=v['unchangedExportNames']),
 sdkBuild=dict(report=receipt(S/'build/ide/build-report.json'),sourceFreeze=receipt(O/'source-freeze.json'),overlay=receipt(O/'overlay.json'),translationUnits=len(up['compiled']),reused=0,sourceFiles=len(freeze['files']),inputsUnchanged=True,wholeSdkWerror=False,compiler=up['compiler'],flags=up['flags'],installedManifest=receipt(R/'ide/third_party/xge_sdk/sdk_manifest.json')),
 guiBuild=dict(report=receipt(E/'gui-build.json'),normalReceipt=receipt(normalPath),translationUnits=101,strictWarnings=['-Wall','-Wextra','-Werror'],sdkAbiFingerprintRebuiltTogether=True,existingCatalogRetained=True,completeCompilerCatalogReleaseAcceptance=False,inputs=b['inputs']),
 validation=dict(wgl=dict(report=receipt(E/'wgl-check.json'),cases=w['cases'],actualProducerFaults=True,allCreatedContextsDeleted=True,productionDllHasNoFaultHooks=True),gui=dict(report=receipt(E/'gui-validation-v2.json'),tests=v['tests'],faults=v['faults'],fullReleaseAdmission=False,physicalWin7Hd4200Tested=False),initialControllerDiagnostic=dict(report=receipt(E/'gui-validation.json'),reason='Harness expected JSON instead of the existing diagnostic .log. Product returned expected1 with original graphics error. Original report/logs retained;product inputs unchanged.'),screenshots=[receipt(E/(name+'.png')) for name in ['candidate-xlang','candidate-c','compat33-gui','root-xlang','root-c']]),
 publication=dict(report=receipt(E/'publication.json'),products=[receipt(R/'release'/name) for name in ['xlang_ide.exe','xge.dll']],protectedRuntime={p:d for p,d in base['protected'].items() if Path(p).name not in ['xge.dll','xlang_ide.exe']},matchedSdkInstalled=True,temporaryTestExecutablesPackaged=False,existingReleaseTmpPreserved=True),
 materialArchive=receipt(H/'manifest.json'),reasonForPrivateEnvironment='Matched old/new SDK comparison and actual WGL producer fault injection protect the usable root release;larger acceptance artifacts on E avoid D pressure. Published normal runtime and final smoke remain root release.',
 limitations=['User Windows7/HD4200 device not physically tested;actual installed driver still must support OpenGL3.3 core','Pre-existing catalog/compiler binding mismatch remains;independent fixed GUI build,not complete catalog/engine/release admission','No claim about full GUI performance or complete LSP/DAP goal'],sources=[dict(title='AMD Catalyst10.6 release notes;HD4200 chipset listed and RadeonHD OpenGL3.3 support',url='https://drivers.amd.com/relnotes/Catalyst_106_release_notes.pdf')])
write(out,record)
indexPath=R/'dev/release-materials/provenance/current-delivery.json';index=read(indexPath);(E/'current-delivery.before.json').write_bytes(indexPath.read_bytes());index['date']='2026-11-02';ide=index['ide'];ide['previousMatchedGuiProvenance']=ide.get('currentBuildProvenance');ide['currentBuildProvenance']=out.relative_to(R).as_posix();ide['guiTranslationUnits']=101;ide['buildCommand']='python -X utf8 -B E:/codex-builds-xlang-wgl-compat-20261102/build-gui.py';ide['buildRunnerSha256']=sha(E/'build-gui.py');ide['sourceIdentity']=dict(baseRevision=record['sourceBaseRevision'],modifiedFromBase=True,exactInputManifest=out.relative_to(R).as_posix());ide['openglStartupCompatibility']=dict(request='3.3 core',targetedValidation='passed',physicalWin7Hd4200Tested=False,matchedSdkManifestSha256=sha(R/'ide/third_party/xge_sdk/sdk_manifest.json'));ide['buildScope']='Targeted matched SDK/GUI WGL repair;existing stale catalog binding preserved,not full release admission'
for row in index['products']:
 if row['path'] in ['release/xlang_ide.exe','release/xge.dll']:
  p=R/row['path'];row.update(sha256=sha(p),bytes=p.stat().st_size,origin='ide-wgl-compat-20261102-targeted-validated')
for row in index.get('dataFiles',[]):
 if row.get('path')=='release/README.md':row.update(sha256=sha(R/row['path']),bytes=(R/row['path']).stat().st_size)
index['acceptance']['ideWglCompatibility']=dict(status=record['status'],provenance=out.relative_to(R).as_posix(),wglCases=8,guiFaults=7,nativeRegressions=6,physicalWin7Hd4200Tested=False,fullReleaseAdmission=False);write(indexPath,index)
print(json.dumps(dict(report=str(out),reportBytes=out.stat().st_size,archiveFiles=len(entries),guiSha256=pub['rootGuiSha256'],xgeSha256=pub['rootXgeSha256'],freeD=shutil.disk_usage(R).free)))
