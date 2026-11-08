from pathlib import Path
import hashlib
import json
import lzma
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
record = manager.read(STORE / '.state/history/2/record.json')
assert record['published'] and record['version'] == 2
assert not (STORE / '.state/push.json').exists()
assert manager.read(REPORT / 'acceptance.json')['passed']
public_url = record['publicUrl']
checks = []


def file_ref(path):
    return dict(path=path.relative_to(ROOT).as_posix(), sha256=manager.digest(path), bytes=path.stat().st_size)


def write(path, data, crlf=False):
    raw = json.dumps(data, ensure_ascii=False, indent=2) + '\n'
    path.write_bytes(raw.replace('\n', '\r\n').encode('utf8') if crlf else raw.encode('utf8'))


for name in manager.META:
    raw = manager.fetch_public(public_url + '/' + name + '?v2-final=' + str(int(time.time())))
    assert raw == (STORE / name).read_bytes(), name
    (REPORT / name).write_bytes(raw)
    checks.append('public-' + name + '-exact')
compressed = (REPORT / 'update.lzma').read_bytes()
decoded = lzma.decompress(compressed[:5] + b'\xff' * 8 + compressed[13:], format=lzma.FORMAT_ALONE)
assert decoded == (REPORT / 'update.json').read_bytes()
checks.append('public-json-lzma-pair')

# Exercise the unmodified public client against production HTTPS. This only
# checks signed version metadata; it performs no installation or launch.
root = WORK / 'online-v1-client'
(root / 'data').mkdir(parents=True, exist_ok=True)
write(root / 'data/release.json', dict(format='xlang-release-1', platform='win_x64', version=1, displayVersion='0.6.0'))
base = [ROOT / 'release/win_x64/update.exe', '--root', root, '--state-dir', WORK / 'online-v1-state', '--check-version', '--no-launch', '--plain']
result = subprocess.run(list(map(str, base)), capture_output=True, timeout=120)
(REPORT / 'online-v1-detects-v2.log').write_bytes(result.stdout + result.stderr)
assert result.returncode == 2, (result.returncode, result.stdout, result.stderr)
checks.append('production-client-v1-detects-optional-v2')
base = [ROOT / 'release/win_x64/update.exe', '--root', WORK / 'client', '--state-dir', WORK / 'state', '--check-version', '--no-launch', '--plain']
result = subprocess.run(list(map(str, base)), capture_output=True, timeout=120)
(REPORT / 'online-v2-current.log').write_bytes(result.stdout + result.stderr)
assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
checks.append('production-client-v2-current')
write(REPORT / 'online-verification.json', dict(passed=True, checks=checks, url=public_url, verificationRoute=record['publicVerificationRoute']))
write(REPORT / 'publish-record.json', record)

index_path = ROOT / 'dev/release-materials/provenance/current-delivery.json'
index = manager.read(index_path)
changed = []
for row in index['products'] + index['dataFiles']:
    if row['path'] == 'release/win_x64/data/release.json':
        row.update(file_ref(ROOT / row['path']))
        changed.append(row['path'])
assert len(changed) == 1
summary = manager.read(REPORT / 'change-summary.json')
receipt = dict(
    schema=1, platform='win_x64', version=2, displayVersion='0.6.1', versionCode=1537,
    forceUpdate=False, published=True, publishedAt=record['publishedAt'],
    sourceRevision=manager.read(REPORT / 'source.json')['revision'],
    publicUrl=public_url, publishedObjects=record['objects'],
    files=summary['files'], changedFiles=len(summary['changedFiles']), deletedFiles=len(summary['deletedFiles']),
    newObjects=summary['newObjects'], deltaBytes=summary['deltaBytes'], mainBytes=summary['mainBytes'],
    signatureKey=(ROOT / 'ide/update/publisher-public-key.txt').read_text().strip(),
    sourceMaterials=file_ref(ROOT / 'dev/release-materials/sources/ide-update-v2-publish-20261108/manifest.json'),
    releaseAcceptance='dev/release-materials/provenance/release-0.6.1-20261108/receipt.json',
    acceptance=file_ref(REPORT / 'acceptance.json'), onlineVerification=file_ref(REPORT / 'online-verification.json'),
    publishedMetadata=[file_ref(REPORT / n) for n in manager.META],
    record=file_ref(REPORT / 'publish-record.json'), notes=file_ref(REPORT / 'notes.txt'),
    localMetadataChanges=changed,
    limitations=[
        'Windows formal update channel only; Linux local 0.6.1 delivery is unchanged and is not remotely published by this operation.',
        'Existing updater scope excludes extlibs, static packages, examples, projects and user settings; MDO inclusion is inherited from v1 and its bytes are unchanged.',
        'The original non-extension command-help compiler binding remains historical (catalogCompilerBindingCurrent=false); source-pair integrity and installed GUI startup passed. This publication does not claim a full compiler/help compatibility audit.',
        'Existing unrelated semantic/LS development and flat-release migration changes are not included.',
    ],
)
write(REPORT / 'receipt.json', receipt)
ref = file_ref(REPORT / 'receipt.json')
index['externalPublishPerformed'] = True
index['currentScope'] = 'Windows formal updater publication 0.6.1 / iteration 2, based on the qualified 25959debc local release; Linux remains a local delivery. Unrelated semantic/LS and flat-layout changes excluded.'
index['updater'].update(preparedVersion=2, publishedVersion=2, preparedDisplayVersion='0.6.1', publishedDisplayVersion='0.6.1', preparedForceUpdate=False, publishedForceUpdate=False, published=True, currentPublishProvenance=ref['path'])
index['onlinePublication'] = ref
index['acceptance']['currentOnlinePublication'] = ref
index['deliveryMaterials']['updateV2Publication'] = ref
write(index_path, index, crlf=True)

readme = '''# Windows 正式更新 0.6.1 / ver2

2026-11-08 已发布到 `https://xcompiler.com/download/update_win_x64`。产品版本 0.6.1、版本代号 0x00000601，更新迭代 2，普通更新（非强制）。沿用正式通道与客户端已信任的 Ed25519 公钥。

本次包含 2,173 个受管文件，更新 139 个文件；新增 134 个压缩对象，合计 25,209,046 字节（24.04 MiB）。122 个过时受管文件被清退，其中 121 个是旧命令帮助源码快照，另一个是旧字体文件。文件删除范围在上线前已逐项检查。

已从真实 ver1 清单还原安装，使用正式更新器执行 ver1 → ver2，再逐项验证全部新文件、删除项、安装回执、同版完整性、CLI 版本、C / xlang JIT 与 IDE 启动。设置、用户项目、扩展目录和恢复文件哨兵均保留。上线按先对象、后清单、最后版本标记的顺序执行；服务器使用复用连接的 curl 逐项从公开 HTTPS 验证对象，并复核三个公开信息文件。前两次独立请求校验遇到 TLS／连接中断，均未激活版本；失败日志已保留，最终通过复用连接的校验后才切换版本。正式客户端在线确认 ver1 检出普通更新、ver2 为最新。

范围沿用既有更新器合同：不更新 extlibs、static、examples、projects 及用户设置；MDO 继承 ver1 的纳入选项，文件未变。Linux 没有在线发布。旧的非扩展命令帮助绑定仍是历史编译器指纹，本次已验证资料配对与安装后 GUI 启动，不将其表述为完整编译器／帮助兼容验收。

详细记录见 receipt.json、acceptance.json、online-verification.json、publish.log。update.json / update.lzma / version.txt 是线上精确字节；源码材料位于 dev/release-materials/sources/ide-update-v2-publish-20261108/。不包含签名私钥和 SSH 凭据。
'''
(REPORT / 'README.md').write_text(readme, encoding='utf8', newline='\n')
doc = ROOT / 'docs/release-0.6.1.md'
text = doc.read_text(encoding='utf8')
text += '\nWindows 正式更新通道随后已发布 **0.6.1 / 迭代 2**（普通更新）；Linux 保持本地交付。线上签名、完整对象和正式客户端版本检测均通过，见 [ver2 发布记录](../dev/release-materials/provenance/ide-update-v2-publish-20261108/README.md)。\n'
doc.write_text(text, encoding='utf8', newline='\n')
doc = ROOT / 'ide/tools/update/README.md'
text = doc.read_text(encoding='utf8')
text = text.replace('**Windows 正式通道已于 2026-11-07 发布 `0.6.0 / 迭代 1`。此前迭代 3 的测试通道及其旧暂存资料已归档。后续从迭代 2 递增，不要再次初始化或重置正式通道。**', '**Windows 正式通道于 2026-11-07 发布 `0.6.0 / 迭代 1`，2026-11-08 已发布 `0.6.1 / 迭代 2`（普通更新）。此前迭代 3 的测试通道及其旧暂存资料已归档。正式通道后续从迭代 3 递增，不要再次初始化或重置。**')
text = text.replace('prepare --platform win_x64 --version 2 --notes-file release/update_info/notes/0.6.0-v2.txt', 'prepare --platform win_x64 --version 3 --notes-file release/update_info/notes/0.6.1-v3.txt')
doc.write_text(text, encoding='utf8', newline='\n')
print(json.dumps(dict(published=True, version=2, displayVersion='0.6.1', checks=checks, receipt=ref), ensure_ascii=False))
