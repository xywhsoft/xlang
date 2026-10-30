"""Derive a pinned XRT implementation from a reviewed C-source function patch.

The source function is identical in the matched SDK and compiler assets. No
installed or upstream source is edited; only a new output header is generated.
"""
from pathlib import Path
import hashlib

BASES = {
    'a222542a4abe652ee92cd28f6de21d6fa7a76398d04c03c6b24b45329c57d1a3',
    '1fc1fc8aa38437bddcf0d909bcfcccdd32a14b9f2e52fbd9661d97c172bf2a31',
    'd8c4d3cb7da7d91c35b38cdb22be516425d3cbd2ed69ddb90b59ec5dfb5714cc',
    '05e8c00fb0eef0215f4d0c9291d240438cb37782df5de1c0e9ba8699b0dcd5eb',
}


def derive(source: Path, output: Path) -> dict:
    patch = Path(__file__).resolve().parents[1] / 'third_party/sdk_patches/xrt-process-tree-v1'
    before = patch / 'process_stop.before.inc'
    after = patch / 'process_stop.inc'
    raw = source.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest not in BASES:
        raise ValueError('XRT changed; audit process ownership, stop policy and Windows Job lifetime')
    original = before.read_text(encoding='utf-8').rstrip('\n')
    replacement = after.read_text(encoding='utf-8').rstrip('\n')
    if hashlib.sha256(original.encode()).hexdigest() != '129cb93146e40cfdf11215a74e10752ec4f0292c7a2b8446906a5dcc38384aaf':
        raise ValueError('Unexpected baseline process-stop source function')
    newline = '\r\n' if b'\r\n' in raw else '\n'
    old = original.replace('\n', newline).encode()
    new = replacement.replace('\n', newline).encode()
    if raw.count(old) != 1:
        raise ValueError('Expected exactly one shared process-stop source function')
    generated = raw.replace(old, new)
    if output.exists() or output.resolve() == source.resolve():
        raise ValueError('A fresh derived header is required')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(generated)
    return dict(source=str(source), sourceSha256=digest, compiledSource=str(output),
                compiledSha256=hashlib.sha256(generated).hexdigest(),
                change='Windows KillTree cleans the retained Job after terminal root publication; other stop kinds and POSIX unchanged',
                unchangedOutsideSourceFunction=True,
                sourceMaterials={str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                                 for p in (before, after, patch / 'process_stop.patch', Path(__file__).resolve())})
