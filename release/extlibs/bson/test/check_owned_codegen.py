"""Check all three generated BSON string-return paths, not just a helper name."""
from pathlib import Path
import re
import sys


def check(source):
    targets = ('xbson_to_hex', 'xbson_gen_oid', 'xbson_to_hex_span')
    if 'xl6_xrt_take_cstr(' in source or re.search(r'\bxbson_\w+_abi\(', source):
        raise ValueError('Obsolete pointer adoption path remains')
    bodies = re.findall(r'^static xlir_result_string \w+\([^;{]*\)\n\{.*?^\}',
                        source, flags=re.MULTILINE | re.DOTALL)
    for target in targets:
        callers = [body for body in bodies if re.search(r'\b' + target + r'\(', body)]
        if len(callers) != 1:
            raise ValueError('Missing or ambiguous native result path: ' + target)
        body = callers[0]
        if len(re.findall(r'\b' + target + r'\(', body)) != 1:
            raise ValueError('Repeated native result allocation: ' + target)
        if not re.search(r'\b\w+\.Data = ' + target + r'\(', body):
            raise ValueError('Native string is not directly adopted: ' + target)
        if re.search(r'\bxlir_string_(?:copy|from_utf8|from_cstr)\(', body):
            raise ValueError('Payload copy in owned wrapper: ' + target)
    print('BSON owned codegen: 3 direct paths, no payload copies, PASS')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('usage: check_owned_codegen.py MODULE.c')
    check(Path(sys.argv[1]).read_text(encoding='utf-8-sig'))
