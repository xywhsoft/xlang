"""Refresh only the XACME vendor snapshot; never the host XRT runtime."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--xrt", type=Path, required=True)
parser.add_argument("--check", action="store_true")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
target = root / "release/extlibs/xacme/upstream"
revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=args.xrt, text=True).strip()
files = {
    "xacme.h": "single/extlibs/xacme.h",
    "xacme_decl.h": "single/extlibs/xacme_decl.h",
    "modules.json": "extlibs/xacme/config/modules.json",
}
contents = {name: (args.xrt / source).read_bytes().replace(b"\r\n", b"\n") for name, source in files.items()}
wait = (args.xrt / "include/xrt/wait.h").read_text(encoding="utf-8")
contents["wait-arithmetic.h"] = ("/* Extracted shared arithmetic from upstream include/xrt/wait.h. */\n"
    + "#define XRT_WAIT_FOREVER INT64_C(-1)\n"
    + wait[wait.index("/* Shared arithmetic"):wait.rfind("#endif")]).encode()
features = sorted(set(re.findall(rb"defined\((XRT_FEATURE_[A-Z0-9_]+)\)", contents["xacme.h"])))
contents["host-features.h"] = (b"/* Declaration-only hosts already expose the complete API. */\n" +
    b"".join(b"#ifndef " + f + b"\n#define " + f + b" 1\n#endif\n" for f in features))
metadata = {"repository": "xrt", "revision": revision,
    "normalization": "CRLF converted to LF before comparison and SHA-256", "files": {
    name: {"source": files.get(name, "include/xrt/wait.h (arithmetic only)" if name=="wait-arithmetic.h" else "single/extlibs/xacme.h (feature guards only)"),
           "sha256": hashlib.sha256(data).hexdigest()} for name, data in contents.items()}}
contents["source.json"] = (json.dumps(metadata, ensure_ascii=False, indent=2) + "\n").encode()
for name, data in contents.items():
    path = target / name
    if args.check:
        if not path.exists() or path.read_bytes().replace(b"\r\n", b"\n") != data:
            raise SystemExit(f"stale XACME snapshot: {name}")
    else:
        path.write_bytes(data)
print("XACME snapshot verified" if args.check else "XACME snapshot updated")
