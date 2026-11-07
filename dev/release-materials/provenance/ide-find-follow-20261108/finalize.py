import gzip, hashlib, json, os, pathlib, re, shutil, subprocess, time, uuid, zipfile
root = pathlib.Path('D:/GIT/x-lang')
run = root/'ide/build/Release/find-follow-20261108'
frozen = pathlib.Path('F:/xlang-validation/ide-find-follow-20261108/source')
sources = root/'dev/release-materials/sources/ide-find-follow-20261108'
reports = root/'dev/release-materials/provenance/ide-find-follow-20261108'
runtime = root/'release/win_x64'
revision = (run/'source-revision.txt').read_text().strip()
meta = json.loads((run/'objects/gui-candidate-build-inputs.json').read_text(encoding='utf-8-sig'))
def sha(p): return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def identity(p):
    p = pathlib.Path(p)
    return {'path':p.relative_to(root).as_posix(), 'sha256':sha(p), 'bytes':p.stat().st_size}
def write(p, v): p.write_text(json.dumps(v, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
rows = {}
for filename in ['final-primary-results.json', 'final-actions-results.json', 'final-fonts-results.json', 'final-scope-results.json']:
    for row in json.loads((run/filename).read_text()): rows[row['test']] = row
assert len(rows)==17 and all(r['exit']==0 for r in rows.values()), rows
native_edges = json.loads((run/'native-edges-results.json').read_text())
baseline = {r['test']:r for r in json.loads((run/'baseline-comparisons.json').read_text())}
assert len(native_edges)==4 and all(r['exit']==baseline[r['test']]['exit']==1 for r in native_edges)
assert all(sha(p)==h for p,h in meta['inputs'].items()), 'Build input drift'
assert json.loads((run/'committed-source-guard.json').read_text())['sourceRevision']==revision
candidate = run/'objects/xlang_ide.link.exe'
assert sha(candidate)==meta['outputs']['xlang_ide']
objects = {p:sha(p) for p in meta['objects']}
installed = runtime/'xlang_ide.exe'
previous = identity(installed)
assert previous['sha256']=='2f60d20aedddec387f23ecd660b1f3e069692e0b29aa582afa263adf67ebc8b6', 'Shipping GUI changed concurrently'
retained = {n:sha(runtime/n) for n in ['xge.dll','xlang-ls.exe','xl.exe','xl.dll','xlc.dll']}
assert retained['xge.dll']=='b415e78d21f8c8b0d8fcb86924169dd35d595d29951391c102892b00399718ff'
assert retained['xlang-ls.exe']==meta['languageServerSha256']
sources.mkdir(parents=True, exist_ok=True); reports.mkdir(parents=True, exist_ok=True)
owned = json.loads((run/'owned.json').read_text())
files = {}
with zipfile.ZipFile(sources/'source-overlay.zip','w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for path in owned:
        data = subprocess.check_output(['git','show',revision+':'+path])
        assert data.replace(b'\r\n',b'\n')==(frozen/path).read_bytes().replace(b'\r\n',b'\n') or path=='ide/tools/build.ps1'
        files[path]=hashlib.sha256(data).hexdigest(); z.writestr(path,data)
shutil.copy2(frozen/'ide/tools/build.ps1',sources/'build-driver-used.ps1')
shutil.copy2(run/'objects/xide_version.h',sources/'xide_version.h')
write(sources/'manifest.json',{
    'schema':1,'sourceRevision':revision,'baseRevision':'596cab8f6',
    'parentSourceMaterials':identity(root/'dev/release-materials/sources/ide-selection-help-20261108/manifest.json'),
    'overlay':identity(sources/'source-overlay.zip'),'files':files,
    'sdkManifest':identity(root/'ide/third_party/xge_sdk/sdk_manifest.json'),
    'optimization':'-O2','scope':'Shared modeless IDE find/replace session with automatic active editor tracking, all-open-document native find scope, native navigation/replacement, responsive fixed footer and explicit overlay font lifetime integration.',
    'rebuild':'Restore parent materials and overlay. Build GuiOnly against the qualified shipping language server and matched SDK. The supplied driver changes private object output only. Full 108-object cohort plus GUI entry rebuilt; final find_view.c and ui_font.c corrections recompiled and relinked with unchanged public layouts.'})
for name in ['verify.py','verify-helpers.py','relink.py','source_guard.py','finalize.py','baseline_compare.py','build-final.log','private-final-relink.log','committed-source-guard.json','baseline-comparisons.json']:
    shutil.copy2(run/name,reports/name)
for filename in ['objects/gui-candidate-build-inputs.json','full-cohort-build-inputs.json']:
    with (reports/(pathlib.Path(filename).name+'.gz')).open('wb') as stream:
        with gzip.GzipFile(filename='', mode='wb', fileobj=stream, mtime=0) as out: out.write((run/filename).read_bytes())
write(reports/'object-hashes.json',objects)
final_corrected = {'find_follow_view_test','actions_test','settings_view_test','ui_font_test','font_test','help_render_test','reading_test'}
for row in rows.values():
    name = row['test']; log = reports/(name+'.log'); shutil.copy2(row['log'],log)
    row['log']=identity(log); row['testSourceSha256']=sha(frozen/'ide/tests'/(name+'.c'))
    row['validationStage']='after-final-overlay-font-and-layout-corrections' if name in final_corrected else 'full-coherent-cohort-before-private-corrections'
edge_rows = []
for row in native_edges:
    name = row['test']; current = reports/(name+'.log'); original = reports/('baseline-'+name+'.log')
    shutil.copy2(row['log'],current); shutil.copy2(baseline[name]['log'],original)
    # Test line numbers moved when the low-level fixtures were annotated.
    pattern = r'^'+re.escape(name)+r':\d+: (.+)$'
    old_failures = re.findall(pattern,original.read_text(encoding='utf-8',errors='replace'),re.M)
    new_failures = re.findall(pattern,current.read_text(encoding='utf-8',errors='replace'),re.M)
    assert old_failures and set(old_failures)==set(new_failures),(name,old_failures,new_failures)
    edge_rows.append({'test':name,'currentExit':1,'baselineExit':1,'sameFailedAssertions':True,'currentLog':identity(current),'baselineLog':identity(original)})
follow = pathlib.Path(rows['find_follow_view_test']['output'])
shutil.copy2(follow/'shared-find.png',reports/'shared-find.png')
checks = int(re.search(r'passed: (\d+) checks',(reports/'find_follow_view_test.log').read_text()).group(1))
env = os.environ.copy(); env['PATH']=str(runtime)+';'+str(pathlib.Path(meta['compiler']).parent)+';'+env['PATH']
def smoke(exe,kind):
    out=run/(kind+'-'+uuid.uuid4().hex[:8]);out.mkdir()
    cmd=[str(exe),'--hidden','--frames','4','--no-session','--no-auto-diagnostics','--settings',str(out/'settings.json'),'--session-root',str(out/'session'),'--diagnostic-root',str(out/'diagnostics'),'--screenshot',str(out/(kind+'.png'))]
    began=time.monotonic(); p=subprocess.run(cmd,cwd=runtime,env=env,capture_output=True,timeout=60)
    log=reports/(kind+'.log'); log.write_bytes(p.stdout+p.stderr)
    assert p.returncode==0,(kind,p.returncode,(p.stdout+p.stderr).decode('utf-8',errors='replace'))
    shutil.copy2(out/(kind+'.png'),reports/(kind+'.png'))
    return {'command':cmd,'exitCode':0,'seconds':round(time.monotonic()-began,3),'log':identity(log),'screenshot':identity(reports/(kind+'.png'))}
temporary = runtime/('xide-verify-'+uuid.uuid4().hex+'.exe')
backup = run/'previous-xlang_ide.exe';shutil.copy2(installed,backup)
try:
    shutil.copy2(candidate,temporary); candidate_smoke=smoke(temporary,'candidate-smoke')
    assert all(sha(p)==h for p,h in meta['inputs'].items()) and all(sha(p)==h for p,h in objects.items())
    assert sha(installed)==previous['sha256'] and retained=={n:sha(runtime/n) for n in retained}
    os.replace(temporary,installed)
    try: installed_smoke=smoke(installed,'installed-smoke')
    except BaseException: shutil.copy2(backup,installed);raise
finally: temporary.unlink(missing_ok=True)
assert sha(installed)==meta['outputs']['xlang_ide'] and retained=={n:sha(runtime/n) for n in retained}
acceptance={
    'schema':1,'date':'2026-11-08','passed':True,'complete':True,
    'scope':'Requested shared IDE session and affected regression gates; not a clean pass of all native SDK boundary tests.',
    'tests':list(rows.values()),'sharedFindChecks':checks,'candidateSmoke':candidate_smoke,'installedSmoke':installed_smoke,
    'nativeSdkBoundaryGatesPassed':False,'preexistingNativeFailures':edge_rows,
    'nativeFailureDisposition':'Same four native SDK gates fail against the previously published GUI objects and unchanged XGE DLL. Kept visible and unchanged; shared IDE UI prevalidates NUL/truncated/surrogate escapes even when there are no matches, and checks no text mutation/ambient error. No XGE binary or SDK upgrade performed.',
    'contracts':{'scope':'All open C/xlang/text/untitled documents, including readonly; excludes unopened disk files and embedded help examples. Global find can run without an active editor.','replacement':'Current document only; labelled explicitly. A replace-all operation keeps native per-document single-undo behavior.','queryScheduling':'80 ms coalescing; explicit buttons use newest query. No idle search or per-result widget creation. No full-frame performance claim.','lifetime':'Version/close/reuse invalidation; floating UI explicitly rebound on font replacement; native layout with measured footer wrapping and computed content extent.'},
    'sourceGuard':identity(reports/'committed-source-guard.json'),'unrelatedWorkspaceChangesExcluded':True,'userSettingsPreserved':True}
write(reports/'acceptance.json',acceptance)
receipt={'schema':1,'date':'2026-11-08','publication':'local-only','passed':True,'complete':True,'sourceRevision':revision,'sourceMaterials':identity(sources/'manifest.json'),'buildInputs':identity(reports/'gui-candidate-build-inputs.json.gz'),'acceptance':identity(reports/'acceptance.json'),'previousGUI':previous,'publishedGUI':identity(installed),'retainedRuntimeIdentities':retained,'guiObjectTranslationUnits':len(meta['objects']),'guiEntryTranslationUnits':1,'optimization':'-O2','installedSmoke':installed_smoke,'temporaryOwnedRuntimeFixturesRemoved':not temporary.exists()}
write(reports/'publication-receipt.json',receipt)
index=root/'dev/release-materials/provenance/current-delivery.json';shutil.copy2(index,reports/'previous-delivery.json')
delivery=json.loads(index.read_text(encoding='utf-8-sig'));delivery['date']='2026-11-08';ide=delivery['ide']
ide['sha256']=sha(installed);ide['guiTranslationUnits']=len(meta['objects']);ide['guiEntryTranslationUnits']=1
ide['currentBuildProvenance']='dev/release-materials/provenance/ide-find-follow-20261108/publication-receipt.json'
ide['buildScope']='Shared modeless find/replace, automatic editor target tracking, all-open-document native find scope, explicit current-document replacement, responsive footer and overlay font lifetime. Full O2 GUI cohort and targeted private corrections; existing qualified XGE and language server retained; unrelated WIP excluded.'
ide['sourceIdentity']['sharedFind']=identity(sources/'manifest.json')
delivery['ideSharedFind']={'publicationReceipt':identity(reports/'publication-receipt.json'),'sourceMaterials':identity(sources/'manifest.json'),'regressionFixtures':len(rows),'sharedFindChecks':checks,'knownNativeBoundaryFailures':len(edge_rows),'installed':True}
write(index,delivery)
print(json.dumps({'sourceRevision':revision,'passedRegressionFixtures':len(rows),'sharedFindChecks':checks,'knownNativeBoundaryFailures':len(edge_rows),'publishedGUI':identity(installed)},indent=2))
