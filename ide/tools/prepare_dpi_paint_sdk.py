"""Prepare SDK65 privately: collect paint ink only for the actual drawing font."""
from datetime import datetime, timezone
import hashlib, json, os, shutil, subprocess, time, uuid
from pathlib import Path

IDE = Path(__file__).resolve().parents[1]

def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))

def write(path, value):
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    os.replace(temporary, path)

def main():
    parent_build = IDE/'build/editor-sdk-geometry-paint-e2151ed3e111/sdk-build.json'
    parent = read(parent_build)
    descriptor = Path(parent['sourceManifest'])
    before = read(descriptor)
    assert parent['exitCode'] == 0 and parent['sourceUnchanged']
    assert sha(descriptor) == parent['sourceManifestSha256']
    root = IDE/'build'/('editor-sdk-dpi-paint-'+uuid.uuid4().hex[:12])
    source = root/'source'
    source.mkdir(parents=True)
    protected = {parent_build: sha(parent_build), descriptor: sha(descriptor), Path(__file__): sha(__file__)}
    for row in before['files']:
        path = Path(parent['source'])/row['path']
        assert sha(path) == row['sha256']
        protected[path] = row['sha256']
        target = source/row['path']
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
    for row in parent['outputs']:
        assert sha(row['path']) == row['sha256']
        protected[Path(row['path'])] = row['sha256']
    shutil.copy2(__file__, root/Path(__file__).name)
    edits = []
    def replace(relative, old, new):
        path = source/relative
        content = path.read_text(encoding='utf-8')
        assert content.count(old) == 1, (relative, old, content.count(old))
        path.write_text(content.replace(old, new), encoding='utf-8')
        edits.append({'path': relative, 'before': old, 'after': new})
    replace('src/xui_proxy_xge.c', '\tint bHasFallback;\n};',
            '\tint bHasFallback;\n\t/* Conservative hint: a sized child handles physical painting. */\n\tint bHasSizedChild;\n};')
    replace('src/xui_proxy_xge.c', '\tpFont->iFlags = pSource->iFlags;',
            '\tpFont->iFlags = pSource->iFlags;\n\tpSource->bHasSizedChild = 1;')
    replace('src/xui_cursor_proxy_internal.inc',
            'iTextSize>=65536?xgeInternalTextShapeVisitPaintCached:xgeInternalTextShapeVisitCached',
            'iTextSize>=65536 && !pFont->bHasSizedChild?xgeInternalTextShapeVisitPaintCached:xgeInternalTextShapeVisitCached')
    strict = Path(parent['command'][3])
    assert sha(strict) == parent['strictBuildScriptSha256']
    protected[strict] = sha(strict)
    shutil.copy2(strict, root/'build_strict_sdk.bat')
    files = [{'path': str(p.relative_to(source)).replace('\\', '/'), 'sha256': sha(p), 'size': p.stat().st_size}
             for p in sorted(source.rglob('*')) if p.is_file()]
    manifest = root/'source-manifest.json'
    write(manifest, {'root': str(root), 'source': str(source), 'files': files, 'edits': edits,
                     'parentManifest': str(descriptor), 'parentManifestSha256': sha(descriptor),
                     'sdkVersion': 65, 'publicHeadersChanged': False, 'conservativeSizedParentHint': True,
                     'productionChanged': False, 'sdkInstalled': False, 'selectedForCurrentCandidate': False})
    log = root/'actual-sdk-build.log'
    receipt = root/'sdk-build.json'
    command = [os.environ['COMSPEC'], '/d', '/c', str(root/'build_strict_sdk.bat'), 'ide']
    record = {'status': 'building', 'source': str(source), 'sourceManifest': str(manifest),
              'sourceManifestSha256': sha(manifest), 'utc': datetime.now(timezone.utc).isoformat(),
              'command': command, 'log': str(log), 'wholeSdkWerror': True,
              'strictBuildScriptSha256': sha(strict), 'productionChanged': False, 'sdkInstalled': False}
    write(receipt, record)
    print(root, flush=True)
    env = dict(os.environ)
    env['PATH'] = r'E:\software\w64devkit\bin;'+env.get('PATH', '')
    started = time.monotonic()
    with log.open('xb') as stream:
        child = subprocess.Popen(command, cwd=source, env=env, stdout=stream, stderr=stream, creationflags=0x08000000)
        record['pid'] = child.pid
        while child.poll() is None:
            record.update(elapsedSeconds=round(time.monotonic()-started, 3), lastConfirmedLiveUtc=datetime.now(timezone.utc).isoformat())
            write(receipt, record)
            time.sleep(.5)
    for path, digest in protected.items():
        assert sha(path) == digest, str(path)
    record['sourceUnchanged'] = all(sha(source/r['path']) == r['sha256'] for r in files)
    assert record['sourceUnchanged']
    record.update(status='sdk-build-finished', exitCode=child.returncode,
                  elapsedSeconds=round(time.monotonic()-started, 3), logSha256=sha(log), parentUnchanged=True)
    record['outputs'] = [{'path': str(source/'build/ide'/name), 'sha256': sha(source/'build/ide'/name),
                          'size': (source/'build/ide'/name).stat().st_size}
                         for name in ('xge.dll', 'xge.lib', 'xge_res.o') if (source/'build/ide'/name).exists()]
    write(receipt, record)
    print(json.dumps({'exitCode': child.returncode, 'report': str(receipt), 'elapsedSeconds': record['elapsedSeconds']}), flush=True)
    if child.returncode:
        print(log.read_text(encoding='utf-8', errors='replace')[-12000:], flush=True)
    return child.returncode

if __name__ == '__main__':
    raise SystemExit(main())
