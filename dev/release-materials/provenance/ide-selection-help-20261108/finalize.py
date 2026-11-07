import gzip, hashlib, json, os, pathlib, shutil, subprocess, time, uuid, zipfile

root = pathlib.Path('D:/GIT/x-lang')
run = root/'ide/build/Release/help-follow-20261108'
frozen = pathlib.Path('F:/xlang-validation/ide-help-follow-20261108/source')
sources = root/'dev/release-materials/sources/ide-selection-help-20261108'
reports = root/'dev/release-materials/provenance/ide-selection-help-20261108'
runtime = root/'release/win_x64'
revision = (run/'source-revision.txt').read_text().strip()
meta = json.loads((run/'objects/gui-candidate-build-inputs.json').read_text(encoding='utf-8-sig'))

def sha(p):
    return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()

def identity(p):
    p = pathlib.Path(p)
    return {'path': p.relative_to(root).as_posix(), 'sha256': sha(p), 'bytes': p.stat().st_size}

def write(p, value):
    p.write_text(json.dumps(value, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')

rows = {}
for name in ['final-primary-results.json', 'final-actions-results.json', 'final-supplement-results.json', 'scrolling-controls-results.json', 'final-layout-results.json', 'final-catalog-errors-results.json']:
    for row in json.loads((run/name).read_text()):
        rows[row['test']] = row
assert len(rows) == 17 and all(row['exit'] == 0 for row in rows.values()), rows
assert all(sha(p) == h for p, h in meta['inputs'].items()), 'Build input drift'
assert json.loads((run/'committed-source-guard.json').read_text())['sourceRevision'] == revision
candidate = run/'objects/xlang_ide.link.exe'
assert sha(candidate) == meta['outputs']['xlang_ide']
objects = {p: sha(p) for p in meta['objects']}
installed = runtime/'xlang_ide.exe'
old = identity(installed)
assert old['sha256'] == '9ab1a1415f91469e70b66a71dd68155915998987a74f3288de8ae31001f1f4b0', 'Shipping GUI changed'
native = {n: sha(runtime/n) for n in ['xge.dll', 'xlang-ls.exe', 'xl.exe', 'xl.dll', 'xlc.dll']}
assert native['xge.dll'] == 'b415e78d21f8c8b0d8fcb86924169dd35d595d29951391c102892b00399718ff'
assert native['xlang-ls.exe'] == meta['languageServerSha256']
sources.mkdir(parents=True, exist_ok=True)
reports.mkdir(parents=True, exist_ok=True)
owned = json.loads((run/'owned.json').read_text())
files = {}
with zipfile.ZipFile(sources/'source-overlay.zip', 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for path in owned:
        data = subprocess.check_output(['git', 'show', revision+':'+path])
        assert data.replace(b'\r\n', b'\n') == (frozen/path).read_bytes().replace(b'\r\n', b'\n') or path == 'ide/tools/build.ps1'
        files[path] = hashlib.sha256(data).hexdigest()
        archive.writestr(path, data)
shutil.copy2(frozen/'ide/tools/build.ps1', sources/'build-driver-used.ps1')
shutil.copy2(run/'objects/xide_version.h', sources/'xide_version.h')
write(sources/'manifest.json', {
    'schema': 1, 'sourceRevision': revision, 'baseRevision': '27098344c',
    'parentSourceMaterials': identity(root/'dev/release-materials/sources/ide-dialog-layout-20261108/manifest.json'),
    'overlay': identity(sources/'source-overlay.zip'), 'files': files,
    'sdkManifest': identity(root/'ide/third_party/xge_sdk/sdk_manifest.json'),
    'optimization': '-O2', 'scope': 'IDE selection-follow help, indexed spelling candidates, persisted setting/pin, F1, immutable catalog leases, background bounded Markdown models and visible-block rendering.',
    'rebuild': 'Restore parent materials and apply overlay. Build GuiOnly using the qualified shipping language server and matched XGE SDK. The supplied build driver only changes private object output location. Full 107-object cohort plus GUI entry was rebuilt; private catalog_view.c layout and command_catalog.c error isolation adjustments were recompiled and relinked afterward.'
})
for name in ['verify.py', 'verify-helpers.py', 'relink.py', 'source_guard.py', 'finalize.py', 'build-final.log', 'private-layout-relink.log', 'committed-source-guard.json', 'baseline-comparisons.json', 'baseline-catalog_view_test.log', 'baseline-help_document_test.log', 'baseline-catalog_source_view_test.log']:
    shutil.copy2(run/name, reports/name)
for name in ['objects/gui-candidate-build-inputs.json', 'full-cohort-build-inputs.json']:
    target = reports/(pathlib.Path(name).name+'.gz')
    with target.open('wb') as stream:
        with gzip.GzipFile(filename='', mode='wb', fileobj=stream, mtime=0) as zipped:
            zipped.write((run/name).read_bytes())
write(reports/'object-hashes.json', objects)
for row in rows.values():
    name = row['test']; log = reports/(name+'.log')
    shutil.copy2(row['log'], log)
    row['log'] = identity(log)
    row['testSourceSha256'] = sha(frozen/'ide/tests'/(name+'.c'))
    row['validationStage'] = 'after-private-layout-adjustment' if name in ['catalog_test', 'catalog_follow_test', 'catalog_source_view_test', 'catalog_view_test', 'help_document_test', 'help_render_test', 'help_viewport_test', 'reading_test', 'settings_view_test', 'dock_windows_view_test'] else 'full-cohort-before-private-layout-adjustment'
follow_output = pathlib.Path(rows['catalog_follow_test']['output'])
for name in ['selection-performance.json', 'follow-help.png']:
    shutil.copy2(follow_output/name, reports/name)
performance = json.loads((reports/'selection-performance.json').read_text())
env = os.environ.copy()
env['PATH'] = str(runtime)+';'+str(pathlib.Path(meta['compiler']).parent)+';'+env['PATH']

def smoke(exe, kind, missing=False):
    out = run/(kind+'-'+uuid.uuid4().hex[:8]); out.mkdir()
    command = [str(exe), '--hidden', '--frames', '4', '--no-session', '--no-auto-diagnostics', '--settings', str(out/'settings.json'), '--session-root', str(out/'session'), '--diagnostic-root', str(out/'diagnostics'), '--screenshot', str(out/(kind+'.png'))]
    if missing:
        command += ['--catalog', str(out/'absent-commands.json')]
    began = time.monotonic()
    result = subprocess.run(command, cwd=runtime, env=env, capture_output=True, timeout=60)
    log = reports/(kind+'.log'); log.write_bytes(result.stdout+result.stderr)
    assert result.returncode == 0, (kind, result.returncode, (result.stdout+result.stderr).decode('utf-8', errors='replace'))
    shutil.copy2(out/(kind+'.png'), reports/(kind+'.png'))
    return {'command': command, 'exitCode': 0, 'seconds': round(time.monotonic()-began, 3), 'log': identity(log), 'screenshot': identity(reports/(kind+'.png'))}

temp = runtime/('xide-verify-'+uuid.uuid4().hex+'.exe')
backup = run/'previous-xlang_ide.exe'
shutil.copy2(installed, backup)
try:
    shutil.copy2(candidate, temp)
    candidate_smoke = smoke(temp, 'candidate-smoke')
    missing_smoke = smoke(temp, 'missing-catalog-smoke', True)
    assert all(sha(p) == h for p, h in meta['inputs'].items()) and all(sha(p) == h for p, h in objects.items())
    assert sha(installed) == old['sha256'] and native == {n: sha(runtime/n) for n in native}
    os.replace(temp, installed)
    try:
        installed_smoke = smoke(installed, 'installed-smoke')
    except BaseException:
        shutil.copy2(backup, installed)
        raise
finally:
    temp.unlink(missing_ok=True)
assert sha(installed) == meta['outputs']['xlang_ide']
assert native == {n: sha(runtime/n) for n in native}
acceptance = {
    'schema': 1, 'date': '2026-11-08', 'passed': True, 'complete': True,
    'scope': 'Affected IDE feature/regression gates, not the entire IDE/compiler suite.',
    'tests': list(rows.values()), 'selectionChecks': 79, 'performance': performance,
    'candidateSmoke': candidate_smoke, 'missingCatalogSmoke': missing_smoke, 'installedSmoke': installed_smoke,
    'sourceGuard': identity(reports/'committed-source-guard.json'),
    'resolvedAcceptanceFindings': [
        'Updated help tree walkers to retain glyph/table/list coverage under the new page root; removed duplicate fixture teardown.',
        'Existing qualified-prefix completion assertions incorrectly expected unrelated names; the original published objects failed the same assertions. They now check the actual single bounded match.',
        'Missing-catalog startup previously left a native file error in the thread slot, causing the next UI callback to fail. Catalog load now isolates its model errors and preserves the caller error; API identity checks and real missing-catalog GUI startup pass.',
        'New fixed follow controls consumed too much of a short high-DPI pane. They now scroll with help; native source-view click, 200% DPI and lifetime regression passes.'
    ],
    'limits': [
        'Candidates are explicitly spelling matches; no verified compiler SymbolId-to-catalog-ID mapping exists. Receiver types and overloads are not claimed as semantically confirmed.',
        'One native control creation cannot be preempted; rendering budget is cooperative, not a hard realtime bound.',
        'Interaction timing excludes paint/presentation. The 1MiB native editing path itself remains costly with following both off and on; this delivery does not claim to fix that existing cost.'
    ],
    'runtimeSettingsPreserved': True, 'unrelatedWorkspaceChangesExcluded': True
}
write(reports/'acceptance.json', acceptance)
receipt = {
    'schema': 1, 'date': '2026-11-08', 'publication': 'local-only', 'passed': True, 'complete': True,
    'sourceRevision': revision, 'sourceMaterials': identity(sources/'manifest.json'),
    'buildInputs': identity(reports/'gui-candidate-build-inputs.json.gz'), 'acceptance': identity(reports/'acceptance.json'),
    'previousGUI': old, 'publishedGUI': identity(installed), 'retainedRuntimeIdentities': native,
    'guiObjectTranslationUnits': len(meta['objects']), 'guiEntryTranslationUnits': 1,
    'optimization': '-O2', 'installedSmoke': installed_smoke, 'temporaryOwnedRuntimeFixturesRemoved': not temp.exists()
}
write(reports/'publication-receipt.json', receipt)
index = root/'dev/release-materials/provenance/current-delivery.json'
shutil.copy2(index, reports/'previous-delivery.json')
delivery = json.loads(index.read_text(encoding='utf-8-sig'))
delivery['date'] = '2026-11-08'
ide = delivery['ide']; ide['sha256'] = sha(installed)
ide['guiTranslationUnits'] = len(meta['objects']); ide['guiEntryTranslationUnits'] = 1
ide['currentBuildProvenance'] = 'dev/release-materials/provenance/ide-selection-help-20261108/publication-receipt.json'
ide['buildScope'] = 'Indexed editor-selection help, pin/settings/F1, background document preparation and bounded visible rendering. Full coherent O2 GUI cohort; focused revalidation after private scrolling-layout adjustment. Existing qualified engine and language server retained; unrelated frontend work excluded.'
ide['sourceIdentity']['selectionHelp'] = identity(sources/'manifest.json')
delivery['ideSelectionHelp'] = {'publicationReceipt': identity(reports/'publication-receipt.json'), 'sourceMaterials': identity(sources/'manifest.json'), 'regressionFixtures': len(rows), 'selectionChecks': 79, 'installed': True}
write(index, delivery)
print(json.dumps({'sourceRevision': revision, 'passedFixtures': len(rows), 'publishedGUI': identity(installed), 'performance': performance}, indent=2))
