#!/usr/bin/env python3
"""Verify the exact archive membership and every decompressed source SHA; no extraction or code execution."""
from pathlib import Path
import hashlib,json,tarfile
root=Path(__file__).resolve().parent
manifest=json.loads((root/'source-manifest.json').read_text(encoding='utf-8'))
want={x['path']:x for x in manifest['files']};seen=set()
with tarfile.open(root/'xge3-native-r3-host-image-r2.tar.gz','r|gz') as tar:
    for m in tar:
        if not m.isfile() or m.name not in want or m.name in seen:raise SystemExit('Unexpected/duplicate member: '+m.name)
        row=want[m.name]
        if m.size!=row['bytes']:raise SystemExit('Size differs: '+m.name)
        h=hashlib.sha256();f=tar.extractfile(m)
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
        if h.hexdigest()!=row['sha256']:raise SystemExit('SHA differs: '+m.name)
        seen.add(m.name)
if seen!=set(want):raise SystemExit('Missing source members')
print(json.dumps({'passed':True,'checkedFiles':len(seen),'sourceFiles':manifest['sourceFiles'],'sourceBytes':manifest['sourceBytes']}))
