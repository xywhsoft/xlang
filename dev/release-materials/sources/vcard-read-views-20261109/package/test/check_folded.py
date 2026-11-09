"""核对独立固定的规范字节，不能只让两个同源写出器互相比对。"""
from pathlib import Path
import sys


expected = ("BEGIN:VCARD\r\nVERSION:4.0\r\nFN:张三\r\n"
            "EMAIL:home@example.com\r\nNOTE:第一行\\n第二行\r\nEND:VCARD\r\n").encode()
if len(sys.argv) < 2:
    raise SystemExit("usage: check_folded.py output.vcf ...")
for name in sys.argv[1:]:
    data = Path(name).read_bytes()
    if data != expected:
        raise ValueError(f"canonical vCard differs: {name}")
    data.decode("utf-8")
print(f"vcard canonical UTF8/wire: {len(sys.argv) - 1} files PASS")
