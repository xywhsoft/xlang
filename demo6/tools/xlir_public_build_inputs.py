"""Verify a reusable full source build against the selected consumer tree.
No release compiler fallback, object override or shipping acceptance claim.
"""
from pathlib import Path
import hashlib,json,os

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def verify_public_build(root,public):
    root=Path(root).resolve(); public=Path(public).resolve()
    manifest=public/'build.json'
    build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build.get('Complete',os.name=='nt') and build['SourceXrt'] and not build['RuntimeOnly']
    assert build['EnableExeCompiler']==1 and len(build['Objects'])==len(build['Sources'])
    driver=Path(build['Driver']); assert sha(driver)==build['DriverSha256'].lower()
    name='xlir_public_source_driver.c' if os.name=='nt' else 'xlir_public_source_linux_driver.c'
    sources=[Path(row['Path']) for row in build['Inputs'] if Path(row['Path']).name==name]
    assert len(sources)==1; compiled_root=sources[0].parents[2]
    compiled={Path(name) if Path(name).is_absolute() else compiled_root/name for name in build['Sources']}
    pinned={manifest,driver,Path(__file__).resolve()}; compared={}
    for row in build['Inputs']+build['Headers']+build['Objects']:
        path=Path(row['Path']); assert sha(path)==row['Sha256'].lower(),str(path); pinned.add(path)
        if not path.is_relative_to(compiled_root): continue
        relative=path.relative_to(compiled_root)
        if path not in compiled and relative.parts[0] not in ('src','lib','res','include') and relative.parts[:2]!=('release','include'): continue
        selected=root/relative
        assert path.read_bytes().replace(b'\r\n',b'\n')==selected.read_bytes().replace(b'\r\n',b'\n'),str(selected)
        pinned.add(selected); compared[relative.as_posix()]=sha(selected)
    assert 'src/ir/xl6_ir.c' in compared and 'tools/xrt_single_header_impl.c' in compared
    return build,pinned,compared
