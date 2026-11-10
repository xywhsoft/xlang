"""Verify that a supplied CLI/runtime matches an actual complete build report."""
from pathlib import Path
import hashlib,json,os

def verify_full_compiler(report,compiler):
    metadata=json.loads(Path(report).read_text(encoding='utf-8-sig'))
    assert metadata.get('complete') and metadata.get('passed') and metadata.get('fullTccXrtAndVfs')
    assert not metadata.get('changedInputs')
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    freeze=Path(metadata['sourceFreeze'])
    assert sha(freeze)==metadata['sourceFreezeSha256']
    names=('xl.exe','xl.dll','xlc.dll') if os.name=='nt' else ('xl','libxl.so','libxlc.so')
    compiler=Path(compiler).resolve();assert compiler.name==names[0]
    for name in names:
        path=compiler.parent/name;product=metadata['products'][name]
        assert path.stat().st_size==product['bytes'] and sha(path)==product['sha256'],path
    return metadata
