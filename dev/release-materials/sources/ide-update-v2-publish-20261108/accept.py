from pathlib import Path
import hashlib
import json
import lzma
import os
import shutil
import subprocess
import sys
import time

ROOT = Path('D:/GIT/x-lang')
sys.path.insert(0, str(ROOT / 'ide/tools/update'))
import update_manager as manager

STORE = ROOT / 'release/update_info/win_x64'
REPORT = ROOT / 'dev/release-materials/provenance/ide-update-v2-publish-20261108'
WORK = Path('F:/xlang-validation/update-v2-20261108')
WORK.mkdir(exist_ok=False)
CLIENT = WORK / 'client'
CLIENT.mkdir()
STATE = WORK / 'state'
results = dict(platform='win_x64', version=2, displayVersion='0.6.1', passed=False, checks=[])


def run(name, command, expected=0, **kwargs):
    result = subprocess.run(list(map(str, command)), capture_output=True, timeout=240, **kwargs)
    (REPORT / (name + '.log')).write_bytes(result.stdout + result.stderr)
    assert result.returncode == expected, (name, result.returncode, (result.stdout + result.stderr)[-3000:])
    results['checks'].append(name)
    print(name, 'passed', flush=True)
    return result


old = manager.payload(STORE / '.state/history/1/update.json')
new = manager.payload(STORE / 'update.json')
envelope = manager.read(STORE / 'update.json')
public = (ROOT / 'ide/update/publisher-public-key.txt').read_text().strip()
assert envelope['publicKey'] == public
(WORK / 'payload.json').write_bytes(envelope['payload'].encode('utf8'))
run('signature', [manager.crypto_path(), 'verify', public, WORK / 'payload.json', envelope['signature']])
assert len(new['files']) == len({f['path'].casefold() for f in new['files']})
assert new['version'] == 2 and new['displayVersion'] == '0.6.1' and new['forceUpdate'] is False
assert all(manager.managed(f['path'], True) for f in new['files'])
assert all(p.startswith('data/catalog-sources/') or p == 'data/fonts/NotoSansMonoCJKsc-Regular.otf' for p in new['delete'])
assert all(not f['path'].startswith(('extlibs/', 'projects/', 'examples/', 'option/settings')) for f in new['files'])
results['checks'].append('managed-scope-and-deletions')

# Materialize the exact signed v1 release, not a synthetic old installation.
for f in old['files']:
    names = [f['sha256'] + '-' + str(n) + '.lzma' for n in range(1, f['archivePartCount'] + 1)] if f.get('archiveParts') else [f['sha256'] + '.lzma']
    encoded = b''.join(manager.locate(STORE, name).read_bytes() for name in names)
    if f['compression'] == 'lzma-alone':
        encoded = encoded[:5] + b'\xff' * 8 + encoded[13:]
        raw = lzma.decompress(encoded, format=lzma.FORMAT_ALONE, memlimit=64 * 1024 * 1024)
    else:
        assert f['compression'] == 'none'
        raw = encoded
    assert len(raw) == f['size'] and hashlib.sha256(raw).hexdigest() == f['sha256'], f['path']
    dest = CLIENT / f['path']
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(raw)
results['checks'].append('exact-v1-installation-materialized')
print('exact v1 materialized', len(old['files']), 'files', flush=True)

sentinels = {
    'option/settings.json': b'{"update":{"enabled":false},"keep":"user-settings"}\n',
    'projects/user/main.xl': b'println("keep user project")\n',
    'extlibs/user-owned/preserve.txt': b'keep user extension\n',
    'option/recovery/preserve.txt': b'keep recovery text\n',
}
for name, raw in sentinels.items():
    p = CLIENT / name
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(raw)
base = [ROOT / 'release/win_x64/update.exe', '--root', CLIENT, '--state-dir', STATE, '--no-launch', '--plain']
run('v1-detects-v2', [*base, '--offline', STORE, '--check-version'], expected=2)
run('v1-to-v2-install', [*base, '--offline', STORE])
for f in new['files']:
    p = CLIENT / f['path']
    assert p.stat().st_size == f['size'] and manager.digest(p) == f['sha256'], f['path']
for p in new['delete']:
    assert not (CLIENT / p).exists(), p
for p, raw in sentinels.items():
    assert (CLIENT / p).read_bytes() == raw, p
installed = manager.read(STATE / 'installed.json')
assert installed['version'] == 2 and installed['displayVersion'] == '0.6.1'
assert not (STATE / 'txn/journal.json').exists()
results['checks'].extend(['all-v2-installed-bytes', 'obsolete-managed-files-removed', 'user-files-preserved', 'installed-receipt-and-no-pending-transaction'])
run('same-version-integrity', [*base, '--offline', STORE, '--check'])
result = run('installed-cli-version', [CLIENT / 'xl.exe', '--version', '--json'])
assert json.loads(result.stdout) == dict(version='0.6.1', versionCode=1537, versionCodeHex='0x00000601')

hello = WORK / 'hello.xl'
hello.write_text('func main(...) as int\n    println("xlang ver2 OK")\n    return 0\nend\n', encoding='utf8')
run('installed-xlang-jit', [CLIENT / 'xl.exe', 'run', hello], cwd=WORK)
cfile = WORK / 'hello.c'
cfile.write_text('#include <stdio.h>\nint main(void){puts("C ver2 OK");return 0;}\n', encoding='utf8')
env = dict(os.environ, PATH=os.path.join(os.environ['SystemRoot'], 'System32'), TEMP=str(WORK), TMP=str(WORK))
run('installed-c-jit', [CLIENT / 'xl.exe', 'run', cfile], cwd=WORK, env=env)
# This isolated runtime is necessary to test the actual installed update.
run('installed-gui-startup', [CLIENT / 'xlang_ide.exe', '--hidden', '--frames', '4', '--no-session', '--no-auto-diagnostics', '--settings', CLIENT / 'option/settings.json', '--screenshot', REPORT / 'installed-workbench.png'])
for p, raw in sentinels.items():
    assert (CLIENT / p).read_bytes() == raw, p
for f in new['files']:
    assert manager.digest(ROOT / 'release/win_x64' / f['path']) == f['sha256'], f['path']
results.update(passed=True, files=len(new['files']), deletedFiles=len(new['delete']), previousFiles=len(old['files']), work=str(WORK), completed=int(time.time()), sourceUnchanged=True)
(REPORT / 'acceptance.json').write_text(json.dumps(results, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
print(json.dumps(results, ensure_ascii=False), flush=True)
