#!/usr/bin/env python3
"""AutoTestIDE-compatible object batches, signed manifests and transactional SSH publish."""
from __future__ import annotations
import argparse
import contextlib
import hashlib
import http.client
import json
import lzma
import os
from pathlib import Path, PurePosixPath
import re
import secrets
import shlex
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request
import urllib.error
import prepare_update as maker

ROOT = Path(__file__).resolve().parents[3]
PLATFORMS = ('win_x64', 'linux_x64', 'linux_arm', 'linux_arm64')
TOP = {'xlang_ide.exe','xlang-ls.exe','xge.dll','xl.exe','xl.dll','xlc.dll','xl.def','xlc.def','xlang-update.exe',
       'xlang_ide','xlang-ls','xlang-update','xl','libxl.so','libxlc.so','libxge.so','README.md','QUICKSTART.md'}
META = ('update.json','update.lzma','version.txt')
HEX = re.compile(r'[0-9a-f]{64}(?:-[1-9][0-9]*)?\.lzma\Z')

def digest(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def safe(path):
    if not isinstance(path,str) or not path or len(path)>2048 or '\\' in path or any(ord(c)<32 or ord(c)==127 or c in ':"<>|?*' for c in path):
        return False
    for s in path.split('/'):
        if not s or s in ('.','..') or len(s.encode('utf-8'))>240 or s[-1] in '. ':
            return False
        if s.split('.')[0].lower() in {'con','prn','aux','nul',*(f'{p}{n}' for p in ('com','lpt') for n in range(1,10))}:
            return False
    return not path.startswith('/')

def managed(path, mdo=False):
    return safe(path) and (path in TOP or (mdo and path in ('mdo.exe','mdo')) or path.startswith(('data/','include/','notices/','option/defaults/')))

def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))

def write(path, value):
    maker.write_atomic(Path(path),(json.dumps(value,ensure_ascii=False,indent=2)+'\n').encode('utf-8'))

def payload(path):
    envelope=read(path)
    if envelope.get('format')!='xlang-update-signed-1':
        raise ValueError('unsupported signing envelope')
    return json.loads(envelope['payload'])

def default_key():
    base=Path(os.environ.get('LOCALAPPDATA',Path.home()/'.local/share'))
    return base/'xlang'/'update-signing.key'

def crypto_path(selected=None):
    if selected:
        return str(Path(selected).resolve())
    suffix='.exe' if os.name=='nt' else ''
    host='win_x64' if os.name=='nt' else ('linux_arm64' if os.uname().machine in ('aarch64','arm64') else 'linux_arm' if os.uname().machine.startswith('arm') else 'linux_x64')
    for path in (Path(__file__).with_name('update-crypto'+suffix), ROOT/'ide/build/Release/update'/host/('update-crypto'+suffix), ROOT/'release/tools/update'/('update-crypto'+suffix)):
        if path.is_file(): return str(path)
    raise ValueError('update-crypto not found; run ide/tools/build_update.py first')

def signed(manifest,key,crypto,co_keys=()):
    if key.is_symlink() or key.stat().st_size!=32:
        raise ValueError('signing key must be a regular 32-byte Ed25519 seed')
    data=json.dumps(manifest,ensure_ascii=False,separators=(',',':')).encode('utf-8')
    fd,name=tempfile.mkstemp(prefix='xlang-sign-')
    try:
        with os.fdopen(fd,'wb') as f: f.write(data)
        sig=subprocess.check_output([crypto,'sign',str(key),name],text=True).strip()
        public=subprocess.check_output([crypto,'public',str(key)],text=True).strip()
        if len(sig)!=128 or len(public)!=64: raise ValueError('signer returned invalid output')
        result={'format':'xlang-update-signed-1','publicKey':public,'payload':data.decode('utf-8'),'signature':sig}
        if len(co_keys)>3: raise ValueError('at most three additional signing keys')
        if co_keys:
            result['signatures']=[{'publicKey':public,'signature':sig}]
            for co_key in co_keys:
                co_key=Path(co_key).resolve()
                if co_key.is_symlink() or co_key.stat().st_size!=32: raise ValueError('invalid co-signing seed')
                co_public=subprocess.check_output([crypto,'public',str(co_key)],text=True).strip()
                co_sig=subprocess.check_output([crypto,'sign',str(co_key),name],text=True).strip()
                result['signatures'].append({'publicKey':co_public,'signature':co_sig})
        return (json.dumps(result,ensure_ascii=False,indent=2)+'\n').encode('utf-8')
    finally: os.unlink(name)

def link(source,dest):
    source,dest=Path(source),Path(dest)
    dest.parent.mkdir(parents=True,exist_ok=True)
    if dest.exists():
        if dest.is_symlink() or digest(source)!=digest(dest): raise ValueError(f'object collision: {dest}')
        return
    try: os.link(source,dest)
    except OSError: maker.copy_atomic(source,dest)

@contextlib.contextmanager
def lock(store):
    store.mkdir(parents=True,exist_ok=True)
    path=store/'.state/operation.lock';path.parent.mkdir(exist_ok=True)
    f=path.open('a+b')
    try:
        if os.name=='nt':
            import msvcrt
            f.seek(0);f.write(b'\0');f.flush();f.seek(0);msvcrt.locking(f.fileno(),msvcrt.LK_NBLCK,1)
        else:
            import fcntl
            fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
        yield
    finally: f.close()

def current(store):
    path=store/'.state/current.json'
    return read(path)['version'] if path.exists() else 0

def activate_local(store,version):
    history=store/'.state/history'/str(version)
    for name in META: maker.copy_atomic(history/name,store/name)
    write(store/'.state/current.json',{'version':version})

def object_specs(manifest):
    result={}
    for entry in manifest['files']:
        if entry.get('archiveParts'):
            for n,part in enumerate(entry['archiveParts'],1):
                result[f"{entry['sha256']}-{n}.lzma"]={'sha256':part['sha256'],'size':part['size']}
        else:
            result[entry['sha256']+'.lzma']={'sha256':entry['archiveSha256'],'size':entry['archiveSize']}
    return result

def locate(store,name):
    for path in (store/'update'/name,*sorted(store.glob('ver*/'+name))):
        if path.is_file() and not path.is_symlink(): return path
    raise ValueError('missing historical object: '+name)

def restore_main(store,manifest):
    specs=object_specs(manifest)
    for name,spec in specs.items():
        source=locate(store,name)
        if source.stat().st_size!=spec['size'] or digest(source)!=spec['sha256']: raise ValueError('corrupt object: '+name)
        link(source,store/'update'/name)
    for path in (store/'update').glob('*.lzma'):
        if path.name not in specs: path.unlink()

def runtime_inputs(source,mdo):
    # Never traverse package-manager/user-owned trees merely to ignore them later.
    result=[p for p in source.iterdir() if managed(p.name,mdo)]
    for prefix in ('data','include','notices','option/defaults'):
        directory=source/prefix
        if directory.is_symlink(): raise ValueError('linked runtime directory: '+prefix)
        if directory.is_dir(): result.extend(directory.rglob('*'))
    return sorted(set(result))

def validate_runtime(source):
    if not (source/'xlang_ide.exe').exists(): return
    for name in ('xge.dll','xlang-ls.exe','xl.exe','xl.dll','xlc.dll','data/commands.json','data/catalog-sources/index.json'):
        if not (source/name).is_file(): raise ValueError('incomplete IDE runtime: '+name)
    catalog=read(source/'data/commands.json')
    bindings=catalog.get('binding',{}).get('compilerFiles',{})
    if set(bindings)!={'xl.exe','xl.dll','xlc.dll'}:raise ValueError('command catalog has no complete compiler binding')
    binding_current=all(digest(source/name)==sha for name,sha in bindings.items())
    index=read(source/'data/catalog-sources/index.json')
    if index.get('catalogSha256')!=digest(source/'data/commands.json'): raise ValueError('command catalog/source pair mismatch')
    if not binding_current:print('NOTE: retained command-help compiler binding; package integrity does not replace compiler/catalog release acceptance',file=sys.stderr)
    return binding_current

def make(args,store):
    if (store/'.state/push.json').exists(): raise ValueError('finish the pending SSH push before preparing another version')
    previous=current(store)
    if args.command=='init' and previous: raise ValueError('already initialized; use prepare')
    if args.command=='prepare' and not previous: raise ValueError('initialize first')
    if args.version<=previous: raise ValueError('version must increase')
    config=read(store/'.state/config.json') if (store/'.state/config.json').exists() else {}
    if config.get('platform') and config['platform']!=args.platform:raise ValueError('update repository belongs to another platform')
    source=Path(args.source or config.get('source') or ROOT/'release'/args.platform).resolve()
    if not source.is_dir() or source==store or store.is_relative_to(source): raise ValueError('source missing or output is inside source')
    if source.name in PLATFORMS and source.name!=args.platform:raise ValueError('source platform directory does not match requested platform')
    binding_current=validate_runtime(source)
    if (store/f'ver{args.version}').exists() or (store/'.state/history'/str(args.version)).exists(): raise ValueError('version already exists')
    key=Path(args.key or config.get('key') or default_key()).resolve();crypto=crypto_path(args.crypto)
    old=payload(store/'.state/history'/str(previous)/'update.json') if previous else {'files':[],'delete':[]}
    known={f['path'] for f in old['files']}|set(old['delete'])
    byhash={}
    for h in (store/'.state/history').glob('*/update.json'):
        for f in payload(h)['files']: byhash[f['sha256']]=f
    staging=Path(tempfile.mkdtemp(prefix='prepare-',dir=store/'.state'))
    try:
        files=[];inputs=[];folded=set();changed=0;objects=staging/'objects';objects.mkdir()
        for path in runtime_inputs(source,args.include_mdo):
            rel=path.relative_to(source).as_posix()
            if not managed(rel,args.include_mdo): continue
            if path.is_symlink() or any(p.is_symlink() for p in path.parents if p!=source.parent): raise ValueError('linked input: '+rel)
            if path.is_dir(): continue
            if not path.is_file(): raise ValueError('nonregular input: '+rel)
            if rel.casefold() in folded: raise ValueError('case-colliding path: '+rel)
            folded.add(rel.casefold());sha=digest(path);size=path.stat().st_size
            if size>2*1024**3: raise ValueError('file too large: '+rel)
            inputs.append((path,sha));cached=byhash.get(sha)
            if cached:
                entry={k:v for k,v in cached.items() if k not in ('path','mode')}
                for name,spec in object_specs({'files':[entry]}).items():
                    original=locate(store,name)
                    if digest(original)!=spec['sha256']: raise ValueError('corrupt cached object: '+name)
                    link(original,objects/name)
            else:
                compression,parts=maker.create_object(path,objects,sha)
                archive=objects/(sha+'.lzma')
                if parts:
                    size_packed=sum(p['size'] for p in parts['archiveParts'])
                    hasher=hashlib.sha256()
                    for n in range(1,parts['archivePartCount']+1):
                        with (objects/f'{sha}-{n}.lzma').open('rb') as f:
                            while chunk:=f.read(1024*1024): hasher.update(chunk)
                    packed_hash=hasher.hexdigest()
                else: size_packed=archive.stat().st_size;packed_hash=digest(archive)
                entry={'sha256':sha,'size':size,'compression':compression,'archiveSize':size_packed,'archiveSha256':packed_hash,**parts}
                # Validate the encoded bytes as well as the input identity.
                def chunks():
                    names=[objects/f'{sha}-{n}.lzma' for n in range(1,parts['archivePartCount']+1)] if parts else [archive]
                    for name in names:
                        with name.open('rb') as f:
                            while b:=f.read(1024*1024): yield b
                rawhash=hashlib.sha256();decoder=lzma.LZMADecompressor(format=lzma.FORMAT_ALONE,memlimit=64*1024*1024) if compression=='lzma-alone' else None
                first=True
                for chunk in chunks():
                    # AutoTestIDE stores a known length + EOS marker. liblzma's
                    # streaming validator expects the original unknown-length header.
                    if decoder and first: chunk=chunk[:5]+b'\xff'*8+chunk[13:]
                    first=False
                    rawhash.update(decoder.decompress(chunk,max_length=1024*1024) if decoder else chunk)
                    while decoder and not decoder.needs_input and not decoder.eof:
                        rawhash.update(decoder.decompress(b'',max_length=1024*1024))
                if rawhash.hexdigest()!=sha or (decoder and not decoder.eof): raise ValueError('input changed while encoding: '+rel)
            mode=0o755 if (path.suffix.lower() in ('.exe','.so') or os.access(path,os.X_OK) and os.name!='nt' or rel in ('xl','xlang-update','xlang_ide','xlang-ls','mdo')) else 0o644
            files.append({'path':rel,**entry,'mode':mode})
            if not any(f['path']==rel and f['sha256']==sha for f in old['files']): changed+=1
        if not files: raise ValueError('no managed runtime files')
        for p,sha in inputs:
            if digest(p)!=sha: raise ValueError('input changed during preparation: '+str(p))
        notes=Path(args.notes_file).read_text(encoding='utf-8') if args.notes_file else args.notes or ''
        manifest={'schema':4,'platform':args.platform,'version':args.version,'notes':notes,'files':files,
                  'delete':sorted(p for p in known-{f['path'] for f in files} if managed(p,args.include_mdo) and p not in ('xlang-update.exe','xlang-update'))}
        data=signed(manifest,key,crypto,args.co_sign_key)
        maker.write_atomic(staging/'update.json',data);maker.write_atomic(staging/'update.lzma',maker.lzma_alone(data));maker.write_atomic(staging/'version.txt',f'{args.version}\n'.encode())
        oldspec=object_specs(old);specs=object_specs(manifest);batch=store/f'ver{args.version}';batch.mkdir()
        for name in specs:
            if name not in oldspec: link(objects/name,batch/name)
            link(objects/name,store/'update'/name)
        history=store/'.state/history'/str(args.version);history.mkdir(parents=True)
        for name in META: maker.copy_atomic(staging/name,history/name)
        write(history/'record.json',{'version':args.version,'previous':previous,'platform':args.platform,'created':int(time.time()),'published':False,'catalogCompilerBindingCurrent':binding_current,'changedFiles':changed,'objects':len(specs),'newObjects':len(list(batch.glob('*.lzma')))})
        write(store/'.state/config.json',{'source':str(source),'platform':args.platform,'key':str(key)})
        activate_local(store,args.version);restore_main(store,manifest)
        print(json.dumps({'version':args.version,'changedFiles':changed,'newObjects':len(list(batch.glob('*.lzma'))),'store':str(store)},ensure_ascii=False))
    finally: shutil.rmtree(staging)

class SSH:
    def __init__(self,config):
        self.host=config['host'];self.root=config['remoteRoot'].rstrip('/');self.url=config['publicUrl'].rstrip('/')
        self.config=config.get('sshConfig')
        if not re.fullmatch(r'[A-Za-z0-9_][A-Za-z0-9_.@-]*',self.host): raise ValueError('invalid SSH host alias')
        if not self.root.startswith('/') or any(p=='..' for p in self.root.split('/')) or any(ord(c)<32 for c in self.root): raise ValueError('invalid absolute remote root')
        self.exe=config.get('ssh') or shutil.which('ssh')
        if not self.exe and os.name=='nt':
            for p in (Path(os.environ.get('WINDIR','C:/Windows'))/'System32/OpenSSH/ssh.exe',Path('C:/Program Files/Git/usr/bin/ssh.exe')):
                if p.is_file(): self.exe=str(p);break
        if not self.exe: raise ValueError('OpenSSH not found; configure ssh path on the publishing machine')
    def run(self,script,*,file=None,check=True):
        # Leave room for local client arguments and quoting below legacy 8 KiB
        # command limits. A truncated mutating command must never be executed.
        if len(script.encode('utf-8'))>6000: raise ValueError('SSH command exceeds the safe transport limit; shorten the remote path or reduce the batch')
        command=[self.exe]+(['-F',self.config] if self.config else [])+['-oBatchMode=yes','-oConnectTimeout=15',self.host,script]
        p=subprocess.run(command,stdin=file if file else subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        if check and p.returncode: raise RuntimeError(p.stderr.decode(errors='replace').strip() or f'SSH failed ({p.returncode})')
        return p.stdout.decode('utf-8',errors='strict')
    def upload(self,local,remote):
        with Path(local).open('rb') as f: self.run('umask 077; cat > '+shlex.quote(remote),file=f)
    def hashes(self,directory,names):
        result={}
        for i in range(0,len(names),64):
            group=names[i:i+64]
            script='cd '+shlex.quote(directory)+' && for f in '+' '.join(shlex.quote(n) for n in group)+'; do if test -f "$f" && ! test -L "$f"; then sha256sum -- "$f"; else printf "MISSING %s\\n" "$f"; fi; done'
            for line in self.run(script).splitlines():
                if re.match(r'^[0-9a-f]{64}  ',line): result[line[66:]]=line[:64]
        return result
    def set_read_permissions(self,directory,names):
        for i in range(0,len(names),64):
            self.run('cd '+shlex.quote(directory)+' && chmod 644 '+' '.join(shlex.quote(n) for n in names[i:i+64]))

def public_retry(read,url):
    for attempt in range(3):
        try:return read()
        except (OSError,urllib.error.URLError,http.client.HTTPException) as error:
            if attempt==2:raise RuntimeError('public download failed: '+url+': '+str(error)) from error
            print(f'RETRY public {attempt+1}/2 {url}: {error}',flush=True);time.sleep(attempt+1)

def fetch_public(url,limit=16*1024*1024):
    def read():
        request=urllib.request.Request(url,headers={'Cache-Control':'no-cache','Accept-Encoding':'identity'})
        with public_open(request) as response:
            data=response.read(limit+1)
            if len(data)>limit: raise ValueError('public response exceeded limit')
            return data
    return public_retry(read,url)

def verify_public_object(url,spec):
    def read():
        h=hashlib.sha256();size=0
        with public_open(url) as response:
            while block:=response.read(1024*1024):
                h.update(block);size+=len(block)
                if size>spec['size']:raise ValueError('public object exceeded declared size: '+url)
        if size<spec['size']:raise OSError('public object response was truncated: '+url)
        if size!=spec['size'] or h.hexdigest()!=spec['sha256']:raise ValueError('public object verification failed: '+url)
    public_retry(read,url)

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,*args): return None

def public_open(url):
    # Match the updater: a deployment must serve the configured endpoint,
    # rather than verifying a redirected endpoint the client cannot use.
    return urllib.request.build_opener(NoRedirect()).open(url,timeout=30)

def push(args,store):
    version=current(store)
    if not version: raise ValueError('initialize first')
    activate_local(store,version);manifest=payload(store/'update.json');restore_main(store,manifest)
    config=read(store/'.state/deploy.json');remote=SSH(config)
    if not remote.root.endswith('/update_'+args.platform): raise ValueError('remoteRoot must end with /update_'+args.platform)
    specs=object_specs(manifest);names=sorted(specs)
    if any(not HEX.fullmatch(n) for n in names): raise ValueError('invalid object name')
    q=shlex.quote;root=remote.root;parent=str(PurePosixPath(root).parent)
    private=parent+'/.xlang-publish-'+hashlib.sha256(root.encode()).hexdigest()[:16]
    lockdir=private+'/lock';stage=private+'/v'+str(version)
    if args.dry_run:
        print(json.dumps({'host':remote.host,'remoteRoot':root,'publicUrl':remote.url,'version':version,'objects':len(names),'bytes':sum(v['size'] for v in specs.values())},ensure_ascii=False,indent=2));return
    pending=store/'.state/push.json'
    state=read(pending) if pending.exists() else {'version':version,'token':secrets.token_hex(16)}
    if state['version']!=version: raise ValueError('finish or recover the previous push before preparing another version')
    write(pending,state);token=state['token']
    remote.run(f'umask 077; mkdir -p {q(root)}/update {q(private)}; chmod 755 {q(root)} {q(root)}/update; if mkdir {q(lockdir)} 2>/dev/null; then printf %s {q(token)} > {q(lockdir)}/owner; else test "$(cat {q(lockdir)}/owner)" = {q(token)}; fi')
    activated=False;metadata_started=False
    try:
        active=remote.run(f'if test -f {q(root)}/version.txt; then cat {q(root)}/version.txt; else printf 0; fi').strip()
        if not active.isdigit() or int(active)>version: raise ValueError('remote version is newer; prepare a new release number')
        if int(active)==version:
            remote_json=remote.run(f'cat {q(root)}/update.json').encode('utf-8')
            if remote_json!=(store/'update.json').read_bytes(): raise ValueError('published version is immutable; choose a new version number')
            activated=True
        remote.run(f'mkdir -p {q(stage)}/objects {q(stage)}/previous; for f in update.json update.lzma version.txt; do if test -f {q(root)}/"$f"; then test -f {q(stage)}/previous/"$f" || cp -- {q(root)}/"$f" {q(stage)}/previous/"$f"; fi; done')
        existing=remote.hashes(root+'/update',names);missing=[n for n in names if existing.get(n)!=specs[n]['sha256']]
        if not activated and any(n in existing for n in missing):
            raise ValueError('immutable object collision or corruption; verify existing release before overwriting objects')
        for index,name in enumerate(missing,1):
            print(f'UPLOAD {index}/{len(missing)} {name}',flush=True)
            temp=stage+'/objects/'+name+'.part';remote.upload(store/'update'/name,temp)
            output=remote.run('sha256sum -- '+q(temp)).split()[0]
            if output!=specs[name]['sha256']: raise ValueError('remote object verification failed: '+name)
            remote.run('chmod 644 '+q(temp)+' && mv -fT -- '+q(temp)+' '+q(root+'/update/'+name))
        actual=remote.hashes(root+'/update',names)
        if any(actual.get(n)!=specs[n]['sha256'] for n in names): raise ValueError('remote complete-object check failed')
        remote.set_read_permissions(root+'/update',names)
        # Validate public object access BEFORE exposing the new version marker.
        for index,name in enumerate(names,1):
            verify_public_object(remote.url+'/update/'+name,specs[name])
            if index==1 or index%64==0 or index==len(names):print(f'VERIFY PUBLIC {index}/{len(names)}',flush=True)
        for name in META:
            remote.upload(store/name,stage+'/'+name)
            if remote.run('sha256sum -- '+q(stage+'/'+name)).split()[0]!=digest(store/name): raise ValueError('remote metadata hash mismatch')
        metadata_started=True
        # Each rename is atomic. Clients bracket mutable-manifest reads with version reads.
        remote.run(' && '.join('cp -- '+q(stage+'/'+n)+' '+q(root+'/'+n+'.new')+' && chmod 644 '+q(root+'/'+n+'.new')+' && mv -fT -- '+q(root+'/'+n+'.new')+' '+q(root+'/'+n) for n in META[:2]))
        nonce='?verify='+secrets.token_hex(8)
        for name in META[:2]:
            if fetch_public(remote.url+'/'+name+nonce)!=(store/name).read_bytes(): raise ValueError('public download verification failed: '+name)
        remote.run('cp -- '+q(stage+'/version.txt')+' '+q(root+'/version.txt.new')+' && chmod 644 '+q(root+'/version.txt.new')+' && mv -fT -- '+q(root+'/version.txt.new')+' '+q(root+'/version.txt'))
        activated=True
        if fetch_public(remote.url+'/version.txt'+nonce)!=(store/'version.txt').read_bytes():raise ValueError('version activated; public verification pending, rerun push')
        history=store/'.state/history'/str(version);record=read(history/'record.json');record.update(published=True,publishedAt=int(time.time()),host=remote.host,publicUrl=remote.url);write(history/'record.json',record)
        pending.unlink();print(f'PUBLISHED {args.platform} version={version} uploaded={len(missing)} reused={len(names)-len(missing)}')
    except BaseException:
        # A disconnected SSH acknowledgement may hide a completed rename.
        # Never roll the release number backwards after clients could install it.
        safe_to_restore=False
        if metadata_started and not activated:
            try:
                observed=remote.run(f'if test -f {q(root)}/version.txt; then cat {q(root)}/version.txt; else printf 0; fi').strip()
                safe_to_restore=observed.isdigit() and int(observed)<version
            except (OSError,RuntimeError):pass
        if safe_to_restore:
            # Restore the activation marker last; objects are immutable and retained.
            remote.run(' && '.join(f'if test -f {q(stage)}/previous/{n}; then cp -- {q(stage)}/previous/{n} {q(root)}/{n}.new && chmod 644 {q(root)}/{n}.new && mv -fT -- {q(root)}/{n}.new {q(root)}/{n}; else rm -f -- {q(root)}/{n}; fi' for n in META),check=False)
        raise
    finally:
        remote.run(f'if test "$(cat {q(lockdir)}/owner 2>/dev/null)" = {q(token)}; then rm -f -- {q(lockdir)}/owner; rmdir {q(lockdir)}; fi',check=False)

def menu():
    platform=input('平台 [win_x64 / linux_x64 / linux_arm / linux_arm64，默认 win_x64]: ').strip() or 'win_x64'
    if platform not in PLATFORMS: print('平台无效');return 1
    selected=input('更新工作目录 [默认使用 ide/build/updates 下的平台目录]: ').strip()
    shared=['--platform',platform]+(['--store',selected] if selected else [])
    choices={'1':'init','2':'prepare','3':'push','4':'status','5':'list','6':'rebuild','7':'discard','8':'configure'}
    while True:
        print('\n1 初始化全量  2 制作新版本  3 推送上线  4 本地/线上状态\n5 版本列表    6 重建最新    7 撤销未发布版本  8 配置 SSH  0 退出')
        choice=input('选择: ').strip()
        if choice=='0': return 0
        if choice not in choices: continue
        command=choices[choice];args=[command,*shared]
        if command in ('init','prepare'):
            version=input('新版本号（递增整数）: ').strip()
            if not version.isdigit(): print('版本号无效');continue
            args+=['--version',version,'--notes',input('更新简介: ')]
            if command=='init':args+=['--source',input('平台发行目录: ').strip()]
        if command=='configure':
            args+=['--host',input('SSH 主机别名: ').strip(),'--remote-root',input('远端文件系统目录: ').strip(),'--public-url',input('公开下载 URL: ').strip()]
        if command=='status' and input('同时查询线上？[y/N]: ').strip().lower()=='y':args+=['--remote']
        main(args)

def main(argv=None):
    if argv is None and len(sys.argv)==1 and sys.stdin.isatty(): return menu()
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command',choices=('init','prepare','list','status','rebuild','discard','configure','push','verify'))
    parser.add_argument('--platform',choices=PLATFORMS,default='win_x64' if os.name=='nt' else 'linux_x64')
    parser.add_argument('--store',type=Path);parser.add_argument('--source');parser.add_argument('--version',type=int,default=0)
    parser.add_argument('--key');parser.add_argument('--crypto');parser.add_argument('--notes');parser.add_argument('--notes-file');parser.add_argument('--include-mdo',action='store_true')
    parser.add_argument('--co-sign-key',action='append',default=[],help='additional trusted rollout key; preserves the previous signature')
    parser.add_argument('--host');parser.add_argument('--remote-root');parser.add_argument('--public-url');parser.add_argument('--ssh');parser.add_argument('--ssh-config');parser.add_argument('--dry-run',action='store_true');parser.add_argument('--remote',action='store_true')
    args=parser.parse_args(argv);store=(args.store or ROOT/'ide/build/updates'/args.platform).resolve()
    try:
        with lock(store):
            if args.command in ('init','prepare'): make(args,store)
            elif args.command=='configure':
                if not all((args.host,args.remote_root,args.public_url)): raise ValueError('configure requires --host --remote-root --public-url')
                if not args.public_url.startswith('https://') and not args.public_url.startswith(('http://127.0.0.1:','http://localhost:')): raise ValueError('public URL requires HTTPS')
                write(store/'.state/deploy.json',{'host':args.host,'remoteRoot':args.remote_root,'publicUrl':args.public_url,'ssh':args.ssh,'sshConfig':args.ssh_config});print('Deployment configuration saved locally')
            elif args.command=='push': push(args,store)
            elif args.command=='list':
                for history in sorted((store/'.state/history').glob('*'),key=lambda p:int(p.name)): print(json.dumps(read(history/'record.json'),ensure_ascii=False))
            elif args.command=='status':
                print('Local current:',current(store))
                if args.remote:
                    config=read(store/'.state/deploy.json');remote=SSH(config);print('Remote current:',remote.run('cat '+shlex.quote(remote.root+'/version.txt')).strip());print('Public current:',fetch_public(remote.url+'/version.txt',128).decode().strip())
            elif args.command in ('verify','rebuild'):
                version=current(store)
                if not version: raise ValueError('initialize first')
                m=payload(store/'.state/history'/str(version)/'update.json');restore_main(store,m)
                if args.command=='rebuild': activate_local(store,version)
                print('Verified version',version)
            elif args.command=='discard':
                version=current(store);h=store/'.state/history'/str(version);record=read(h/'record.json')
                if record['published'] or not record['previous'] or (store/'.state/push.json').exists(): raise ValueError('only the latest unpublished, unpushed version can be discarded')
                previous=record['previous'];restore_main(store,payload(store/'.state/history'/str(previous)/'update.json'));activate_local(store,previous)
                shutil.rmtree(store/f'ver{version}');shutil.rmtree(h);print('Restored local version',previous)
        return 0
    except (OSError,ValueError,KeyError,RuntimeError,subprocess.SubprocessError,lzma.LZMAError) as e:
        print('ERROR:',e,file=sys.stderr);return 1

if __name__=='__main__': raise SystemExit(main())
