"""Validate and copy the exact source snapshots declared by a command catalog."""
import argparse
import hashlib
import json
import shutil
import stat
from pathlib import Path

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def plain(path, directory=False):
    info=path.lstat()
    if path.is_symlink() or getattr(info, 'st_file_attributes', 0)&0x400:
        raise ValueError(f'Source resource is a link or reparse point: {path}')
    if not (stat.S_ISDIR(info.st_mode) if directory else stat.S_ISREG(info.st_mode)):
        raise ValueError(f'Unexpected source resource: {path}')

def create(catalog, materials, destination):
    """Write a new complete bundle from bytes captured by the catalog generator."""
    catalog, destination=Path(catalog), Path(destination)
    if destination.exists(): raise ValueError('Source bundle destination already exists')
    data=json.loads(catalog.read_text(encoding='utf-8-sig'))
    declared={row['path']:row['sha256'] for row in data['sources']}
    used={row['source'] for row in data['entries']}|{row['source'] for row in data['documents']}
    rows=[]; payload={}
    for path in sorted(used):
        raw=materials.get(path)
        if raw is None or len(raw)>4*1024*1024 or hashlib.sha256(raw).hexdigest()!=declared.get(path):
            raise ValueError('Generated source bytes differ from catalog: '+path)
        raw.decode('utf-8-sig')
        digest=declared[path]; payload[digest+'.source']=raw
        rows.append({'path':path,'sha256':digest,'state':'available'})
    index={'format':'xlang-catalog-sources-1','catalogSha256':sha(catalog),'entries':rows}
    destination.mkdir(parents=True)
    for name,raw in payload.items(): (destination/name).write_bytes(raw)
    (destination/'index.json').write_text(json.dumps(index,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return verify(destination,catalog)

def verify(source, catalog):
    source, catalog=Path(source), Path(catalog)
    plain(source, True)
    plain(source/'index.json')
    if (source/'index.json').stat().st_size>1024*1024:
        raise ValueError('Oversize catalog source index')
    index=json.loads((source/'index.json').read_text(encoding='utf-8'))
    data=json.loads(catalog.read_text(encoding='utf-8-sig'))
    if index.get('format')!='xlang-catalog-sources-1' or index.get('catalogSha256')!=sha(catalog):
        raise ValueError('Catalog source index differs from command catalog')
    declared={row['path']: row['sha256'] for row in data['sources']}
    used={row['source'] for row in data['entries']}|{row['source'] for row in data['documents']}
    seen=set(); files={'index.json':sha(source/'index.json')}; available=0
    for row in index['entries']:
        path=row['path']; digest=row['sha256']
        if path not in used or path in seen or digest!=declared[path] or len(digest)!=64 or any(c not in '0123456789abcdef' for c in digest):
            raise ValueError(f'Invalid catalog source provenance: {path}')
        seen.add(path)
        if row['state']=='unavailable':
            if not row.get('reason'): raise ValueError(f'Missing source explanation: {path}')
            continue
        if row['state']!='available': raise ValueError(f'Unknown source state: {path}')
        leaf=digest+'.source'; file=source/leaf; plain(file)
        if file.stat().st_size>4*1024*1024 or sha(file)!=digest:
            raise ValueError(f'Catalog source content differs: {path}')
        file.read_text(encoding='utf-8-sig')
        files[leaf]=digest; available+=1
    if seen!=used: raise ValueError('Catalog source index is incomplete')
    if {p.name for p in source.iterdir()}!=set(files): raise ValueError('Unexpected catalog source payload')
    return {'files':files,'available':available,'unavailable':len(used)-available,'catalogSha256':index['catalogSha256']}

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--catalog',type=Path,required=True)
    parser.add_argument('--destination',type=Path)
    args=parser.parse_args(); result=verify(args.source,args.catalog)
    if args.destination:
        args.destination.mkdir(parents=True,exist_ok=True)
        plain(args.destination,True)
        if any(p.name not in result['files'] for p in args.destination.iterdir()): raise ValueError('Destination has unexpected source resources')
        for name in result['files']:
            target=args.destination/name
            if target.exists(): plain(target)
            shutil.copy2(args.source/name,target)
        if verify(args.destination,args.catalog)!=result: raise ValueError('Copied catalog sources differ')
    print(json.dumps({key:value for key,value in result.items() if key!='files'},ensure_ascii=False))

if __name__=='__main__': main()
