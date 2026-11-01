"""Check the actual owned result path while retaining the const-input C adapter."""
from pathlib import Path
import re
import sys


def check(source):
    if 'xl6_xrt_take_cstr(' in source:
        raise ValueError('Obsolete pointer adoption path remains')
    bodies = re.findall(r'^static xlir_result_string \w+\([^;{]*\)\n\{.*?^\}',
                        source, flags=re.MULTILINE | re.DOTALL)
    callers = [body for body in bodies if 'xcbor_to_hex_abi(' in body]
    if len(callers) != 1 or callers[0].count('xcbor_to_hex_abi(') != 1:
        raise ValueError('Missing, repeated or ambiguous owned result call')
    body = callers[0]
    if not re.search(r'\b\w+\.Data = xcbor_to_hex_abi\(', body):
        raise ValueError('The native string is not directly adopted')
    if re.search(r'\bxlir_string_(?:copy|from_utf8|from_cstr)\(', body):
        raise ValueError('Payload copy in owned wrapper')
    print('CBOR owned codegen: direct char* adoption, no payload copy, PASS')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('usage: check_owned_codegen.py MODULE.c')
    check(Path(sys.argv[1]).read_text(encoding='utf-8-sig'))
