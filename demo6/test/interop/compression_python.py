from __future__ import annotations

import bz2
import gzip
from pathlib import Path
import zipfile


ROOT = Path(__file__).resolve().parents[2] / "tmp"
PAYLOAD = b"xlang compression interop\x00\xff"


assert gzip.decompress((ROOT / "xlang_interop.gz").read_bytes()) == PAYLOAD
assert bz2.decompress((ROOT / "xlang_interop.bz2").read_bytes()) == PAYLOAD
with zipfile.ZipFile(ROOT / "xlang_interop.zip", "r") as archive:
    assert archive.read("docs/hello.txt") == b"hello from xlang"
    assert archive.read("payload.bin") == PAYLOAD

(ROOT / "python_interop.gz").write_bytes(gzip.compress(PAYLOAD, compresslevel=9, mtime=0))
(ROOT / "python_interop.bz2").write_bytes(bz2.compress(PAYLOAD, compresslevel=9))
with zipfile.ZipFile(ROOT / "python_interop.zip", "w") as archive:
    archive.writestr("docs/hello.txt", "hello from python", compress_type=zipfile.ZIP_DEFLATED)
    archive.writestr("payload.bin", PAYLOAD, compress_type=zipfile.ZIP_STORED)

print("compression Python interop: PASS")
