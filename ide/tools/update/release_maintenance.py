#!/usr/bin/env python3
"""Inspect, archive local state, and create a user-only portable ZIP."""
import argparse
from datetime import datetime,timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import zipfile
import update_manager as manager
ROOT=Path(__file__).resolve().parents[3]
RUNTIME_DIRS={'data','examples','extlibs','include','notices','static'}
RUNTIME_FILES=manager.TOP|{'mdo.exe','mdo'}
def linked(p):
    s=p.lstat();return p.is_symlink() or bool(getattr(s,'st_file_attributes',0)&0x400)
def plan(source):
    result=[]
    for p in sorted(source.iterdir()):
        if p.name=='.xlang-update':continue # Migrated by the updater under its own lock.
        if p.name=='option' and p.is_dir() and not linked(p):
            result.extend(q for q in sorted(p.iterdir()) if q.name!='defaults');continue
        if p.name in RUNTIME_DIRS and p.is_dir() and not linked(p):continue
        if p.name in RUNTIME_FILES and p.is_file() and not linked(p):continue
        result.append(p)
    return result
def updater_state(source,platform,migrate=False):
    exe=source/('update.exe' if platform.startswith('win_') else 'update')
    if not exe.is_file():
        if (source/'.xlang-update').exists():raise ValueError('native updater required to migrate existing state')
        return None
    args=[str(exe),'--root',str(source)]
    if migrate:
        r=subprocess.run([*args,'--cached-policy','--plain','--no-launch'],capture_output=True,timeout=60)
        if r.returncode not in (0,4):raise ValueError('state migration refused: '+r.stderr.decode(errors='replace'))
    p=Path(subprocess.check_output([*args,'--print-state-dir'],text=True,timeout=30).strip())
    if p.is_relative_to(source):raise ValueError('updater still uses in-client state; rebuild it first')
    return p

def clean(source,platform,archive,apply=False):
    candidates=plan(source);legacy=(source/'.xlang-update').exists()
    publisher=ROOT/'release/tools'
    extra=source==ROOT/'release'/platform and publisher.exists()
    info={'source':str(source),'archive':str(archive),'archivePaths':[p.relative_to(source).as_posix() for p in candidates], 'migrateUpdateState':legacy,'archivePublisherTools':extra,'applied':False}
    if not apply:return info
    state=updater_state(source,platform,True)
    if state and (state/'txn/journal.json').exists():raise ValueError('unfinished updater transaction: run update --recover-only before cleaning')
    if archive.exists() or archive.resolve().is_relative_to(source) or source.is_relative_to(archive.resolve()):raise ValueError('use a new archive directory outside the runtime')
    pairs=[(p,archive/p.relative_to(source)) for p in candidates]
    provenance=ROOT/'dev/release-materials/provenance/legacy-runtime'/archive.name
    if source==ROOT/'release'/platform:pairs=[(p,provenance/p.name if p.name=='ide-install-receipt.json' else q) for p,q in pairs]
    tools_archive=ROOT/'release/update_info/tools-archive'/archive.name
    if extra:pairs.append((publisher,tools_archive))
    for p,q in pairs:
        allowed=p.is_relative_to(source) or p==ROOT/'release/tools'
        allowed_target=q.resolve().is_relative_to(archive.resolve()) or (p.name=='ide-install-receipt.json' and q.resolve().is_relative_to(provenance.resolve())) or (p==publisher and q.resolve().is_relative_to(tools_archive.resolve()))
        if not allowed or not allowed_target or linked(p):raise ValueError('unsafe archive path: '+str(p))
        # Verify the physical source before any recursive directory move.
        if not p.resolve().is_relative_to(source.resolve()) and p!=publisher:raise ValueError('archive source escapes runtime')
    archive.mkdir(parents=True);moved=[]
    info['updateState']=str(state) if state else None
    info['moves']=[{'source':str(p),'destination':str(q)} for p,q in pairs]
    manager.write(archive/'manifest.json',{**info,'status':'moving'})
    try:
        for p,q in pairs:q.parent.mkdir(parents=True,exist_ok=True);p.rename(q);moved.append((p,q))
    except BaseException:
        for p,q in reversed(moved):
            if not p.exists():q.rename(p)
        manager.write(archive/'manifest.json',{**info,'status':'failed-rolled-back'});raise
    info['applied']=True;manager.write(archive/'manifest.json',{**info,'status':'archived'})
    return info

def package(source,platform,output):
    if plan(source) or (source/'.xlang-update').exists():raise ValueError('runtime has local state; run clean first')
    metadata=manager.read(source/'data/release.json')
    if metadata.get('format')!='xlang-release-1' or metadata.get('platform')!=platform or type(metadata.get('version')) is not int or metadata['version']<1:raise ValueError('prepare a signed update version before creating the portable ZIP')
    if output.exists() or output.resolve().is_relative_to(source):raise ValueError('use a new ZIP path outside the runtime')
    if source==ROOT/'release'/platform:
        store=ROOT/'release/update_info'/platform
        if manager.current(store)!=metadata['version']:raise ValueError('portable version differs from the prepared update repository')
        for item in manager.payload(store/'update.json')['files']:
            if manager.digest(source/item['path'])!=item['sha256']:raise ValueError('runtime changed after update preparation: '+item['path'])
    files=[]
    for p in source.rglob('*'):
        if linked(p):raise ValueError('linked runtime input: '+str(p))
        if p.is_file():files.append((p,manager.digest(p)))
    output.parent.mkdir(parents=True,exist_ok=True);temp=output.with_name(output.name+'.part')
    if temp.exists():raise ValueError('previous ZIP staging file exists; preserve and review it')
    try:
        with zipfile.ZipFile(temp,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
            for p,h in files:z.write(p,p.relative_to(source).as_posix())
        for p,h in files:
            if manager.digest(p)!=h:raise ValueError('runtime changed while packaging: '+str(p))
        with zipfile.ZipFile(temp) as z:
            if z.testzip():raise ValueError('ZIP integrity check failed')
        os.replace(temp,output)
    finally:temp.unlink(missing_ok=True)
    return {'zip':str(output),'sha256':manager.digest(output),'files':len(files),'version':metadata['version'],'displayVersion':metadata['displayVersion']}

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('command',choices=('inspect','clean','bundle'))
    p.add_argument('--platform',choices=manager.PLATFORMS,default='win_x64' if os.name=='nt' else 'linux_x64');p.add_argument('--source',type=Path)
    p.add_argument('--archive',type=Path);p.add_argument('--output',type=Path);p.add_argument('--apply',action='store_true')
    a=p.parse_args(argv);source=(a.source or ROOT/'release'/a.platform).resolve(strict=True)
    stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-'+str(os.getpid())
    try:
        if a.command=='bundle':
            m=manager.read(source/'data/release.json');out=a.output or ROOT/'dev/releases'/f"xlang-{a.platform}-{m['displayVersion']}-v{m['version']}.zip";r=package(source,a.platform,out.resolve())
        else:
            archive=(a.archive or ROOT/'dev/local-state/release-clean'/a.platform/stamp).absolute();r=clean(source,a.platform,archive,a.command=='clean' and a.apply)
        print(json.dumps(r,ensure_ascii=False,indent=2));return 0
    except (OSError,ValueError,KeyError,subprocess.SubprocessError) as e:print('ERROR:',e,file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
