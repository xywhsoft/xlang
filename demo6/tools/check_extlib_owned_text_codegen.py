"""Verify that HTTP builder results reach string returns without payload copies."""
import argparse
import hashlib
import json
import re
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("module", type=Path, help="saved EXE module.c for xlir_extlib_owned_http_text.xl")
parser.add_argument("--receipt", type=Path)
args = parser.parse_args()
source = args.module.read_text(encoding="utf-8-sig")
functions = {}
for match in re.finditer(
    r"^static xlir_result_string (xlir_fn_\d+)\([^;{]*\)\n\{.*?^\}",
    source, re.MULTILINE | re.DOTALL,
):
    functions[match.group(1)] = match.group(0)

targets = (
    "xhttp_protocol_headers_build_owned",
    "xhttp_protocol_cookies_build_owned",
    "xhttp_protocol_set_cookie_build_owned",
    "xhttp_protocol_media_build_owned",
    "xhttp_params_build_owned",
    "xhttp_form_encode_text_owned",
    "xhttp_form_encode_owned_abi",
)
copy = re.compile(r"\bxlir_string_(?:copy|from_utf8|from_cstr)\s*\(")
checks = []
for target in targets:
    callers = [(name, body) for name, body in functions.items()
               if re.search(r"\b" + target + r"\s*\(", body)]
    if not callers:
        raise SystemExit(f"Missing language string-return path for {target}")
    for name, body in callers:
        if copy.search(body):
            raise SystemExit(f"Payload copy in {target} language wrapper {name}")
        adopters = []
        for callee in set(re.findall(r"\b(xlir_fn_\d+)\s*\(", body)) - {name}:
            child = functions.get(callee, "")
            if "xl6_xrt_take_cstr(" not in child:
                continue
            if copy.search(child):
                raise SystemExit(f"Payload copy in {target} adoption helper {callee}")
            adopters.append(callee)
        if not adopters:
            raise SystemExit(f"Missing direct adoption helper for {target}")
        checks.append({"native": target, "language": name, "adopters": sorted(adopters)})

record = {"outcome": "passed", "module": str(args.module.resolve()),
          "sha256": hashlib.sha256(args.module.read_bytes()).hexdigest(), "checks": checks}
if args.receipt:
    args.receipt.write_text(json.dumps(record, indent=2), encoding="utf-8")
print(f"HTTP owned output: {len(checks)} string-return paths and their adoption helpers contain no payload copies")
