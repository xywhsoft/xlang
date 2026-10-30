"""用独立 ZIP 实现检查实际正文、CRC、STORE 和注释，不比较可变 mtime。"""
from pathlib import Path
import sys
import zipfile

if len(sys.argv) < 2:
    raise SystemExit("usage: check_memory_wire.py output.zip ...")
for filename in sys.argv[1:]:
    with zipfile.ZipFile(Path(filename)) as archive:
        if archive.namelist() != ["payload.txt"]:
            raise ValueError("wrong entries")
        info = archive.getinfo("payload.txt")
        if info.compress_type != zipfile.ZIP_STORED or info.comment != b"stable":
            raise ValueError("wrong method/comment")
        if archive.read(info) != b"z" * 131072 or archive.testzip() is not None:
            raise ValueError("wrong payload/CRC")
print(f"zip independent memory wire: {len(sys.argv) - 1} files PASS")
