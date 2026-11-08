from pathlib import Path
import json,hashlib,shutil,sys,subprocess
r=Path('D:/GIT/x-lang');p=r/'dev/release-materials/provenance/ide-update-v3-publish-20261109';m=r/'dev/release-materials/sources/ide-update-v3-publish-20261109';m.mkdir(exist_ok=True);store=r/'release/update_info/win_x64';sys.path.insert(0,str(r/'ide/tools/update'));import update_manager as u
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def ref(f):return dict(path=f.relative_to(r).as_posix(),sha256=sha(f),bytes=f.stat().st_size)
def write(f,v):f.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
old=u.payload(store/'.state/history/2/update.json');new=u.payload(store/'update.json');before={f['path']:f for f in old['files']};changed=[f['path']for f in new['files']if f['path']not in before or before[f['path']]['sha256']!=f['sha256']];summary=dict(files=len(new['files']),changedFiles=changed,deletedFiles=new['delete'],newDeletions=sorted(set(new['delete'])-set(old['delete'])),newObjects=len(list((store/'ver3').glob('*.lzma'))),deltaBytes=sum(f.stat().st_size for f in(store/'ver3').glob('*.lzma')),mainBytes=sum(f.stat().st_size for f in(store/'update').iterdir()if f.is_file()));write(p/'change-summary.json',summary)
for name in u.META:shutil.copy2(store/name,p/name)
shutil.copy2(store/'.state/history/3/record.json',p/'prepare-record.json')
shutil.copy2(r/'build/release-0.6.2-20261109/accept_update.py',m/'accept.py');shutil.copy2(r/'dev/release-materials/sources/ide-update-v2-publish-20261108/publish_persistent_curl.py',m/'publish_persistent_curl.py')
for name in ['update_manager.py','prepare_update.py']:shutil.copy2(r/'ide/tools/update'/name,m/name)
write(m/'manifest.json',dict(schema=1,version=3,displayVersion='0.6.2',files=[ref(f)for f in sorted(m.iterdir())if f.name!='manifest.json'],publicKey=(r/'ide/update/publisher-public-key.txt').read_text().strip(),privateSigningKeyIncluded=False,scope='Existing updater publisher, exact signed ver2-to-ver3 acceptance, persistent HTTPS object verification. Local deployment configuration and signing secret excluded.'))
(p/'README.md').write_text('# xlang 0.6.2 / ver3 发布\n\n普通更新；正式 Windows 渠道：https://xcompiler.com/download/update_win_x64 。\n\n当前状态以 receipt.json 和 online-verification.json 为准。签名、对象校验及真实 ver2 升级已通过；用户文件保留。原有更新范围继续排除 extlibs、static、examples、工程及用户设置；MDO 按原渠道规则保留。\n\nLinux 为本地 0.6.2 交付。命令帮助编译器绑定仍是历史绑定，未宣称完整编译器/帮助兼容性审计。详见相邻 0.6.2 验收材料。\n',encoding='utf8')
print(json.dumps({k:v for k,v in summary.items()if k not in ['changedFiles','deletedFiles']},ensure_ascii=False))
