"""Prepare exact audited frontend bytes from known checkout EOL counterparts.

Only transformations that reproduce the registered SHA256 are accepted. The
working tree and dependency metadata are never rewritten. Historical frontend
cohorts retain their existing preparation and admission rules.
"""
from pathlib import Path
import base64
import hashlib
import zlib
from native_c_interop_profile import COHORT, PINS, pins_for, revision


_MIXED = {
    # Captured Lemon output combines template and grammar-action line endings.
    'src/parser/xl6_parser.c': ('df3eb92cc4db3fd11b561370a20ab6b22ade6287054e3daa0a13767ce2b963b8',7846,'eNrdUz1OxjAMdWXpC0PVMHaI0itk7FDRq/QIHTMgkoiLmZv4CB4zVOrngEAgIdj4JLzk+e/5JVYAgM/bGvxvu/vTafefHcGfi7ubPMj4ANLll6/D6dUNIWxEijMC04knJk5sD3t0ux441bKKxoyvuEriBgHBjfPOtIo9NP3OO2jXOdWkMMI8Ru4kwaOyUGIlWWWqhFOEcan0XFSOl6LJguYyDN7NIbypyWhnQcSLgmXnYnwUhf2yb0xUCloX2TyppnERUkBaJ0RoBtdklcTFutok+agX0/pZ6GMzfb9xayjK2LraDILvfyVANpB/39oVEdL51Q=='),
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def recover(data,relative,pins=None):
    expected=(PINS if pins is None else pins)[relative]
    if sha(data)==expected:return data,'exact'
    lf=data.replace(b'\r\n',b'\n')
    for candidate,operation in ((lf,'restore-LF'),(lf.replace(b'\n',b'\r\n'),'restore-CRLF')):
        if sha(candidate)==expected:return candidate,operation
    if relative in _MIXED:
        digest,lines,encoded=_MIXED[relative]
        if digest!=expected:return None
        parts=lf.split(b'\n')
        pattern=zlib.decompress(base64.b64decode(encoded))
        if len(parts)==lines+1 and len(pattern)==(lines+7)//8:
            result=bytearray(parts[0])
            for i,part in enumerate(parts[1:]):
                result+=b'\r\n' if pattern[i//8]&(1<<(i%8)) else b'\n'
                result+=part
            candidate=bytes(result)
            if sha(candidate)==expected:return candidate,'restore-captured-EOL'
    return None


def prepare(frontend: Path, destination: Path):
    frontend=frontend.resolve(strict=True)
    import native_final_c_interop_profile as final_profile
    if final_profile.selected(frontend):return final_profile.prepare(frontend,destination)
    pins=pins_for(frontend)
    metadata=dict(status='unchanged-other-cohort',sourceRoot=str(frontend),
                  frontendRoot=str(frontend),originalInputs={},transformations=[])
    # Runtime identity alone is shared with an older semantic cohort.
    for relative in ('src/ast/xl6_ast.h','lib/xrt/xrt.h'):
        path=frontend/relative
        if not path.is_file() or recover(path.read_bytes(),relative,pins) is None:
            return frontend,metadata
    metadata.update(status='exact',cohort=COHORT,sourceRevision=revision(frontend))
    for relative,expected in pins.items():
        path=frontend/relative
        if not path.is_file():raise ValueError('audited frontend material is missing: '+relative)
        original=path.read_bytes();recovered=recover(original,relative,pins)
        if recovered is None:
            raise ValueError('frontend content changed beyond an audited EOL counterpart;review source contracts: '+relative)
        metadata['originalInputs'][str(path)]=sha(original)
        if recovered[1]!='exact':
            metadata['transformations'].append(dict(path=relative,sourceSha256=sha(original),
                preparedSha256=expected,operation=recovered[1]))
    prepared=frontend
    if metadata['transformations']:
        prepared=destination.resolve();prepared.mkdir(parents=True,exist_ok=False)
        for relative,expected in pins.items():
            path=frontend/relative;original=path.read_bytes()
            if sha(original)!=metadata['originalInputs'][str(path)]:
                raise ValueError('frontend changed during EOL preparation: '+relative)
            recovered=recover(original,relative,pins)
            if recovered is None or sha(recovered[0])!=expected:
                raise ValueError('cannot reproduce the exact audited frontend: '+relative)
            target=prepared/relative;target.parent.mkdir(parents=True,exist_ok=True)
            target.write_bytes(recovered[0])
        metadata.update(status='canonicalized',frontendRoot=str(prepared))
    for path,digest in metadata['originalInputs'].items():
        if sha(Path(path).read_bytes())!=digest:
            raise ValueError('frontend changed during EOL preparation: '+path)
    return prepared,metadata
