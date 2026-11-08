from pathlib import Path
import hashlib,json,lzma,shutil,subprocess,sys,time
r=Path('D:/GIT/x-lang');sys.path.insert(0,str(r/'ide/tools/update'));import update_manager as u
store=r/'release/update_info/win_x64';out=r/'dev/release-materials/provenance/ide-update-v3-publish-20261109';work=Path('F:/xlang-validation/update-v3-20261109');record=u.read(store/'.state/history/3/record.json');assert record['published']and record['version']==3;assert not(store/'.state/push.json').exists();assert u.read(out/'acceptance.json')['passed'];url=record['publicUrl'];checks=[]
def ref(p):return dict(path=p.relative_to(r).as_posix(),sha256=u.digest(p),bytes=p.stat().st_size)
def write(p,d):p.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
for name in u.META:
 for attempt in range(3):
  try:raw=u.fetch_public(url+'/'+name+'?ver3-final='+str(int(time.time())));break
  except Exception:
   if attempt==2:raise
 assert raw==(store/name).read_bytes(),name;(out/name).write_bytes(raw);checks.append('public-'+name+'-exact')
b=(out/'update.lzma').read_bytes();assert lzma.decompress(b[:5]+b'\xff'*8+b[13:],format=lzma.FORMAT_ALONE)==(out/'update.json').read_bytes();checks.append('public-json-lzma-pair')
root=work/'online-ver2-client';(root/'data').mkdir(parents=True,exist_ok=True);write(root/'data/release.json',dict(format='xlang-release-1',platform='win_x64',version=2,displayVersion='0.6.1'))
for name,client,state,want in [('online-ver2-detects-ver3',root,work/'online-ver2-state',2),('online-ver3-current',work/'client',work/'state',0)]:
 cmd=[r/'release/win_x64/update.exe','--root',client,'--state-dir',state,'--check-version','--no-launch','--plain']
 if(out/(name+'.log')).exists():shutil.copy2(out/(name+'.log'),out/(name+'-previous.log'))
 for attempt in range(3):
  p=subprocess.run(list(map(str,cmd)),capture_output=True,timeout=120);(out/(name+'-attempt-'+str(attempt+1)+'.log')).write_bytes(p.stdout+p.stderr)
  if p.returncode==want:break
 assert p.returncode==want,(name,p.returncode,p.stdout,p.stderr);(out/(name+'.log')).write_bytes(p.stdout+p.stderr);checks.append(name)

write(out/'online-verification.json',dict(passed=True,checks=checks,url=url,verificationRoute=record['publicVerificationRoute'],allObjectsVerified=record['objects']))
write(out/'publish-record.json',record);summary=u.read(out/'change-summary.json');sourceRevision='63c0c5d6e';write(out/'source.json',dict(revision=subprocess.check_output(['git','rev-parse',sourceRevision],cwd=r,text=True).strip(),scope='0.6.2 release preparation with qualified source overlays; unrelated development excluded.'))
receipt=dict(schema=1,platform='win_x64',version=3,displayVersion='0.6.2',versionCode=1538,versionCodeHex='0x00000602',forceUpdate=False,published=True,publishedAt=record['publishedAt'],sourceRevision=u.read(out/'source.json')['revision'],publicUrl=url,publishedObjects=record['objects'],files=summary['files'],changedFiles=len(summary['changedFiles']),deletedFiles=len(summary['deletedFiles']),newDeletions=len(summary['newDeletions']),newObjects=summary['newObjects'],deltaBytes=summary['deltaBytes'],mainBytes=summary['mainBytes'],signatureKey=(r/'ide/update/publisher-public-key.txt').read_text().strip(),sourceMaterials=ref(r/'dev/release-materials/sources/ide-update-v3-publish-20261109/manifest.json'),releaseAcceptance='dev/release-materials/provenance/release-0.6.2-20261109/receipt.json',acceptance=ref(out/'acceptance.json'),onlineVerification=ref(out/'online-verification.json'),publishedMetadata=[ref(out/n)for n in u.META],record=ref(out/'publish-record.json'),notes=ref(out/'notes.txt'),limitations=['Windows formal updater channel only; Linux local 0.6.2 was built and validated but not remotely published.','Existing update scope excludes extlibs, static, examples, projects and user settings; MDO inclusion inherited from ver2, unchanged bytes.','Historical compiler/catalog binding remains; package source-pair integrity and IDE startup verified, not a full compiler/help audit.','Targeted release regression only; unrelated worktree changes excluded.'])
write(out/'receipt.json',receipt);refReceipt=ref(out/'receipt.json');indexPath=r/'dev/release-materials/provenance/current-delivery.json';index=u.read(indexPath)
for row in index['products']+index['dataFiles']:
 if row['path']=='release/win_x64/data/release.json':row.update(ref(r/row['path']))
index['externalPublishPerformed']=True;index['currentScope']='Windows formal update 0.6.2 / ver3 published and verified, qualified compiler version overlay plus latest unified-new/open IDE. Linux 0.6.2 delivered locally. Unrelated development excluded.';index['updater'].update(preparedVersion=3,publishedVersion=3,preparedDisplayVersion='0.6.2',publishedDisplayVersion='0.6.2',preparedForceUpdate=False,publishedForceUpdate=False,published=True,currentPublishProvenance=refReceipt['path']);index['onlinePublication']=refReceipt;index['acceptance']['currentOnlinePublication']=refReceipt;index['deliveryMaterials']['updateV3Publication']=refReceipt
releasePath=r/'dev/release-materials/provenance/release-0.6.2-20261109/receipt.json';release=u.read(releasePath);release['publication']='Windows ver3 published; Linux local-only';release['onlinePublication']=refReceipt['path']
for row in release['installed']:
 if row['path']=='release/win_x64/data/release.json':
  row['installedBeforeUpdatePreparationSha256']=row['sha256'];row['sha256']=u.digest(r/row['path']);row['bytes']=(r/row['path']).stat().st_size;row['metadataUpdatedBy']='Signed ver3 preparation'
write(releasePath,release);newRelease=ref(releasePath);index['release062']=newRelease;index['cleanup']['release062']=ref(r/'dev/release-materials/provenance/release-0.6.2-20261109/cleanup-summary.json');write(indexPath,index)
(out/'README.md').write_text(f'''# Windows 正式更新 0.6.2 / ver3

2026-11-09 已上线：https://xcompiler.com/download/update_win_x64 。产品版本 0.6.2，代号 0x00000602，普通更新，沿用既有 Ed25519 公钥。

本次包含 {summary['files']:,} 个受管文件，相对 ver2 更新 {len(summary['changedFiles'])} 个；新增 {summary['newObjects']} 个压缩对象，{summary['deltaBytes']:,} 字节（{summary['deltaBytes']/1048576:.2f} MiB）。本次新增删除两份旧帮助源码快照；清单的累计删除项仍保留历史清退记录。

签名与全部对象校验通过。从真实 ver2 还原安装并升级 ver3，全部受管文件、删除项、安装回执、同版完整性、CLI、C/xlang JIT 和 IDE 启动检查通过；用户设置、工程、自有扩展及恢复文件保留。

上线先上传对象，再从公开 HTTPS 校验全部 {record['objects']} 个对象，最后切换清单与版本标记。三个公开信息文件与本地签名包完全相同；正式客户端在线确认 ver2 检出普通更新，ver3 为最新。

原有更新范围不含 extlibs、static、examples、projects 和用户设置，MDO 继承原纳入规则且字节未变。Linux 0.6.2 为本地交付。历史命令帮助编译器绑定及其他基线限制见相邻 0.6.2 验收材料，未宣称全仓回归。

精确结果见 receipt.json、acceptance.json、online-verification.json、publish-record.json 和 publish.log。线上 update.json / update.lzma / version.txt 的原始字节已保存。不包含签名私钥及 SSH 凭据。
''',encoding='utf8')
shutil.copy2(r/'build/release-0.6.2-20261109/push-ver3.log',out/'publish.log')
p=r/'docs/release-0.6.2.md';s=p.read_text(encoding='utf8');s+='\nWindows 正式通道现已上线 **0.6.2 / ver3**。真实 ver2 升级、全部 1,652 个公开 HTTPS 对象、签名元数据和正式客户端在线检测均通过。\n';p.write_text(s,encoding='utf8')
print(json.dumps(dict(published=True,version=3,displayVersion='0.6.2',checks=checks,receipt=refReceipt),ensure_ascii=False))
