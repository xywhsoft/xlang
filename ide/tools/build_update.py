#!/usr/bin/env python3
"""Build the standalone updater; normal outputs live under release/<platform>."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import struct
import sys
ROOT=Path(__file__).resolve().parents[2]
def run(argv):
    print(' '.join(map(str,argv)),flush=True);subprocess.run(list(map(str,argv)),check=True)
def main():
    p=argparse.ArgumentParser();p.add_argument('--cc',default='gcc');p.add_argument('--platform',choices=('win_x64','linux_x64','linux_arm','linux_arm64'),default='win_x64' if os.name=='nt' else 'linux_x64');p.add_argument('--key',type=Path);p.add_argument('--public-key');p.add_argument('--static',action='store_true');p.add_argument('--skip-run',action='store_true',help='cross-compilation: verify architecture without running foreign code');a=p.parse_args()
    windows=a.platform.startswith('win_');suffix='.exe' if windows else '';out=ROOT/'ide/build/Release/update'/a.platform;out.mkdir(parents=True,exist_ok=True)
    release=ROOT/'release'/a.platform;release.mkdir(parents=True,exist_ok=True);src=ROOT/'ide/update'
    flags=['-std=gnu11','-O2','-ffunction-sections','-fdata-sections','-finput-charset=UTF-8','-fexec-charset=UTF-8','-Wall','-Wextra','-Wno-unused-parameter','-Wno-misleading-indentation','-I'+str(ROOT/'lib/xrt'),'-I'+str(src),'-I'+str(ROOT/'lib/lzma'),'-I'+str(out)]
    if a.cc.endswith('musl-gcc'):
        flags += ['-idirafter','/usr/include','-idirafter','/usr/include/x86_64-linux-gnu']
    libs=['-lws2_32','-liphlpapi','-ladvapi32','-lbcrypt','-lcrypt32','-lshell32'] if windows else ['-pthread','-lm']
    link=['-Wl,--gc-sections','-s']+(['-static'] if windows or a.static else [])
    crypto=out/('update-crypto'+suffix);run([a.cc,*flags,src/'crypto_tool.c',*link,'-o',crypto,*libs])
    committed_public=src/'publisher-public-key.txt'
    if a.public_key: public=a.public_key
    elif committed_public.exists() and not a.key: public=committed_public.read_text().strip()
    else:
        key=a.key or Path(os.environ.get('LOCALAPPDATA',Path.home()/'.local/share'))/'xlang/update-signing.key'
        if not key.exists():
            key.parent.mkdir(parents=True,exist_ok=True)
            fd=os.open(key,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
            with os.fdopen(fd,'wb') as f: f.write(os.urandom(32));f.flush();os.fsync(f.fileno())
            if os.name=='nt':
                identity=subprocess.check_output(['whoami'],text=True).strip()
                subprocess.run(['icacls',str(key),'/inheritance:r','/grant:r',identity+':F'],check=True,stdout=subprocess.DEVNULL)
            print('Created private signing seed outside repository:',key)
        public=subprocess.check_output([str(crypto),'public',str(key)],text=True).strip()
    if len(public)!=64 or any(c not in '0123456789abcdef' for c in public): raise ValueError('invalid Ed25519 public key')
    (out/'public_key.h').write_text('#define UP_PUBLIC_KEY "'+public+'"\n',encoding='ascii')
    (out/'license.h').write_text('static const char up_license[] = '+json.dumps((src/'NOTICE.txt').read_text(encoding='utf-8'))+';\n',encoding='ascii')
    objects=[];inputs={};compiler_identity=subprocess.check_output([a.cc,'--version'],text=True).splitlines()[0]
    sources=[src/n for n in ('runtime.c','platform.c','tui.c','net.c','decode.c','updater.c')]+[ROOT/'lib/lzma/LzmaDec.c']
    for source in [*sources,src/'crypto_tool.c',*src.glob('*.h'),src/'publisher-public-key.txt',src/'NOTICE.txt',ROOT/'lib/xrt/xrt.h',*(ROOT/'lib/lzma').glob('*.h'),out/'public_key.h',out/'license.h',Path(__file__)]:
        inputs[str(source.relative_to(ROOT))]=hashlib.sha256(source.read_bytes()).hexdigest()
    for source in sources:
        obj=out/(source.stem+'.o');stamp=obj.with_suffix('.fingerprint')
        headers=[k for k in sorted(inputs) if k.endswith('.h')]
        if source.name=='runtime.c':headers=[k for k in headers if Path(k).name in ('xrt_profile.h','xrt.h')]
        fingerprint=hashlib.sha256((compiler_identity+json.dumps(flags)+str(source)+hashlib.sha256(source.read_bytes()).hexdigest()+''.join(inputs[k] for k in headers)).encode()).hexdigest()
        if not obj.exists() or not stamp.exists() or stamp.read_text()!=fingerprint:
            run([a.cc,*flags,'-c',source,'-o',obj]);stamp.write_text(fingerprint)
        objects.append(obj)
    candidate=out/('update'+suffix);run([a.cc,*objects,*link,'-o',candidate,*libs])
    for name,expected in inputs.items():
        if hashlib.sha256((ROOT/name).read_bytes()).hexdigest()!=expected: raise ValueError('build input changed: '+name)
    binary=candidate.read_bytes()
    if windows:
        offset=struct.unpack_from('<I',binary,60)[0]
        if binary[offset:offset+4]!=b'PE\0\0' or struct.unpack_from('<H',binary,offset+4)[0]!=0x8664:raise ValueError('not a Windows x64 executable')
    else:
        expected={'linux_x64':62,'linux_arm':40,'linux_arm64':183}[a.platform]
        if binary[:4]!=b'\x7fELF' or binary[5]!=1 or struct.unpack_from('<H',binary,18)[0]!=expected:raise ValueError('ELF architecture does not match update platform')
    if not a.skip_run:
        output=subprocess.check_output([candidate,'--version'],text=True).strip()
        if output!='xlang-update 2 '+a.platform:raise ValueError('runtime platform identity mismatch')
        print(output)
    executable=release/candidate.name;temp=executable.with_suffix('.publish');shutil.copy2(candidate,temp);os.replace(temp,executable)
    tools=ROOT/'release/tools/update';tools.mkdir(parents=True,exist_ok=True)
    shutil.copy2(crypto,tools/crypto.name)
    for source in (ROOT/'ide/tools/update').iterdir():
        if source.is_file() and source.suffix in ('.py','.bat','.sh','.md','.json'): shutil.copy2(source,tools/source.name)
    if windows:
        defaults=release/'option/defaults';defaults.mkdir(parents=True,exist_ok=True)
        shutil.copy2(ROOT/'ide/data/defaults/settings.json',defaults/'settings.json')
    notices=release/'notices';notices.mkdir(exist_ok=True)
    shutil.copy2(src/'NOTICE.txt',notices/'xlang-update.txt');shutil.copy2(src/'NOTICE.txt',tools/'NOTICE.txt')
    receipt={'platform':a.platform,'compiler':compiler_identity,'publicKey':public,'static':windows or a.static,'inputs':inputs,'sha256':hashlib.sha256(executable.read_bytes()).hexdigest(),'bytes':executable.stat().st_size}
    (out/'build.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8');print(json.dumps(receipt,ensure_ascii=False))
if __name__=='__main__': main()
