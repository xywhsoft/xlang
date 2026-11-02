from pathlib import Path
import hashlib,json,subprocess
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text('utf-8-sig'))
def receipt(p):p=Path(p);return dict(path=str(p),bytes=p.stat().st_size,sha256=sha(p))
b=read(O/'gui-rebuild.json');baseline=read(O/'baseline.json');v1=read(O/'validation.json');v2=read(O/'validation-runtime.json');v3=read(O/'validation-runtime-v3.json');pub=read(O/'publication.json')
assert b['status']=='passed' and v3['status']=='passed' and pub['status']=='passed-targeted-gui-validation'
for p,d in b['inputs'].items():assert sha(p)==d,p
for p,d in baseline['protected'].items():
 if Path(p)!=R/'release/xlang_ide.exe':assert sha(p)==d,p
assert sha(R/'release/xlang_ide.exe')==b['candidateSha256']
for record in [b,v1,v2,v3,pub]:
 for row in record['commands']:
  if 'sha256' in row:assert sha(row['log'])==row['sha256'],row['log']
assert not list((R/'release').glob('xide_sha256_20261102*.exe')) and not (R/'release/task_test.exe').exists()
tests=v1['tests']+v3['tests'];assert len(tests)==10 and all(t['status']=='passed' for t in tests)

# Keep normal GUI receipt truthful about the independent, scoped build route.
normal=dict(b,outputs=dict(xlang_ide=sha(R/'release/xlang_ide.exe'),xlang_ls=sha(R/'release/xlang-ls.exe'),xge=sha(R/'release/xge.dll')),buildMode='targeted-gui-sha256-repair',catalogStatus='pre-existing-stale-binding-preserved;standard-build-guard-not-passed')
normalPath=R/'ide/build/Release/gui-build-inputs.json';normalPath.write_text(json.dumps(normal,indent=2)+'\n',encoding='utf8',newline='\n')

H=R/'dev/release-materials/sources/ide-xrt-sha256-20261102';H.mkdir(exist_ok=False);entries=[]
paths=[R/'ide/src'/p for p in ['services/catalog_sources.c','services/build_inputs.c','services/rename_journal.c','platform/file_rename.c','platform/state_repair.c','platform/startup_stage.c','platform/startup_cache.c']]
paths+=[R/'ide/tests/sha256_test.c',R/'ide/tools/build.ps1']
paths+=[O/p for p in ['rebuild-gui.py','validate.py','validate-runtime.py','validate-runtime-v3.py','publish-gui.py','assemble-evidence.py','baseline.json','gui-build-inputs.before.json']]
for p in paths:
 rel=Path('inputs')/p.relative_to(R) if not p.is_relative_to(O) else Path('controllers')/p.name
 dest=H/rel;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(p.read_bytes());assert sha(dest)==sha(p)
 entries.append(dict(recordedPath=str(p),archivedPath=rel.as_posix(),bytes=dest.stat().st_size,sha256=sha(dest)))
(H/'.gitattributes').write_bytes(b'* -text -whitespace\n')
(H/'manifest.json').write_text(json.dumps(dict(schema=1,scope='Exact targeted SHA-256 inputs/controllers,not an entire compiler or SDK bundle. Original failed reports and recorded paths preserved.',entries=entries),indent=2)+'\n',encoding='utf8',newline='\n')

report=dict(schema=1,date='2026-11-02',status='passed-targeted-validation',task='Unify IDE SHA-256 with existing matched XRT',fullReleaseAdmission=False,
 sourceBaseRevision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=R,text=True).strip(),sourceModifiedFromBase=True,
 implementation=dict(provider='Existing matched XGE SDK xge.dll XRT exports',memory='xrtSha256',streaming=['xrtSha256Init','xrtSha256Update','xrtSha256Final'],sites=7,bufferBytes=65536,extraXrtImplementation=False,hashFormatsUnchanged=True,systemHashObjectAllocationsRemoved=True,remainingBCryptImports=['BCryptGenRandom']),
 build=dict(**receipt(O/'gui-rebuild.json'),translationUnits=b['translationUnits'],configuration='Release',cLspEnabled=False,compiler=b['compiler'],compilerSha256=b['compilerSha256'],normalGuiReceipt=receipt(normalPath),inputClosure=b['inputs']),
 validation=dict(tests=tests,testCount=10,sha256IndependentChecks=47,sha256Fixture='Python hashlib fixed oracles;empty,abc,binary andpadding55/56/63/64/65,file-block65535/65536/65537/131089',nativeInput='C/xlang typing,deletion,undo,focus andUnicode IME via real XGE input',rootSmoke=pub['commands'],screenshots=[receipt(O/('smoke-'+lang+'.png')) for lang in ['xlang','c']],imports=v3['imports'],oldWindowsMachineTested=False),
 publication=dict(**receipt(O/'publication.json'),gui=receipt(R/'release/xlang_ide.exe'),engineServerSdkSettingsAndCatalogUnchanged=True,protectedRuntime={p:d for p,d in baseline['protected'].items() if Path(p)!=R/'release/xlang_ide.exe'},catalogHashes={p:d for p,d in b['inputs'].items() if p in [str(R/'release/data/commands.json'),str(R/'release/data/catalog-sources/index.json')]},temporaryTestExecutablesRemoved=True,rootTmpPreexisting=baseline['tmpInitiallyPresent']),
 buildLimitation=dict(standardBuild='Stopped before compilation/publication: existing catalog compiler binding differs from root release engine;current development sources also differ from the older compiler source pin.',decision='Retain existing data and engine/server/SDK;independent frozen GUI-only rebuild and focused validation. Did not modify or pass the stale-catalog guard;not complete catalog/engine/release admission.',log=receipt(O/'build.log')),
 diagnostics=[dict(**receipt(O/name),status=read(O/name)['status'],error=read(O/name).get('error')) for name in ['validation.json','validation-runtime.json','validation-runtime-v3.json']],
 diagnosticExplanation=['Build-directory UI test lacked runtime fonts;reran affected UI tests in normal root release.','Root rename regression required task_test.exe companion;compiled same-cohort fixture,then remaining seven regressions passed. Earlier failed reports retained.'],
 archive=receipt(H/'manifest.json'))
out=R/'dev/release-materials/provenance/ide-xrt-sha256-20261102.json'
with out.open('x',encoding='utf8',newline='\n') as stream:stream.write(json.dumps(report,ensure_ascii=False,indent=2)+'\n')

indexPath=R/'dev/release-materials/provenance/current-delivery.json';index=read(indexPath)
(O/'current-delivery.before.json').write_bytes(indexPath.read_bytes())
index['date']='2026-11-02';ide=index['ide']
ide['previousGuiBuild']=dict(sourceRevision=ide.pop('currentSourceRevision',None),provenance=ide.get('currentBuildProvenance'),buildCommand=ide.get('buildCommand'))
ide.update(guiTranslationUnits=b['translationUnits'],currentBuildProvenance=out.relative_to(R).as_posix(),buildRunnerSha256=sha(O/'rebuild-gui.py'),buildCommand='python -X utf8 -B ide/build/Release/xrt-sha256-20261102/rebuild-gui.py',sha256=dict(provider='matched XGE XRT',BCryptHashImported=False,remainingBCryptImports=['BCryptGenRandom']),sourceIdentity=dict(baseRevision=report['sourceBaseRevision'],modifiedFromBase=True,exactInputManifest=out.relative_to(R).as_posix()),buildScope='Targeted GUI SHA-256 repair;existing stale catalog binding preserved,not full release admission')
for row in index['products']:
 if row['path']=='release/xlang_ide.exe':row.update(sha256=pub['guiSha256'],bytes=pub['guiBytes'],origin='ide-xrt-sha256-20261102-targeted-validated')
index['acceptance']['ideSha256Unification']=dict(status=report['status'],provenance=out.relative_to(R).as_posix(),tests=10,standardCatalogBuildGuardPassed=False,fullReleaseAdmission=False)
indexPath.write_text(json.dumps(index,ensure_ascii=False,indent=2)+'\n',encoding='utf8',newline='\n')
print(json.dumps(dict(provenance=str(out),bytes=out.stat().st_size,archivedFiles=len(entries),guiSha256=pub['guiSha256'])))
