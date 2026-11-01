"""Use independent integer arithmetic and Python datetime to check BSON Date records."""
from datetime import datetime
from pathlib import Path
import struct
import sys


def check(path):
    epoch = (datetime(1970, 1, 1) - datetime(1, 1, 1)).days * 86400000
    minimum, maximum = -(1 << 63), (1 << 63) - 1
    cases = {minimum, -epoch-1, -epoch, -1000, -500, -1, 0, 1, 500, 1000,
             253402300799999, maximum-epoch, maximum-epoch+1, maximum}
    seen = set()
    lines = Path(path).read_text(encoding='utf-8-sig').splitlines()
    for line in lines[:-1]:
        prefix, api, unix, observed, hex_wire = line.split('\t')
        api, unix, observed = int(api), int(unix), int(observed)
        key = api, unix
        if prefix != 'date' or api not in range(4) or unix not in cases or key in seen:
            raise ValueError('Unexpected or repeated Date record: ' + line)
        wire = bytes.fromhex(hex_wire)
        if len(wire) != 19 or wire[:10] != b'\x13\0\0\0\x09when\0' or wire[-1] != 0:
            raise ValueError('Invalid independent BSON frame')
        if struct.unpack_from('<q', wire, 10)[0] != unix:
            raise ValueError('BSON Unix milliseconds differ')
        expected = unix + epoch
        if observed != (expected if minimum <= expected <= maximum else 0):
            raise ValueError('XRT AD milliseconds differ')
        seen.add(key)
    if seen != {(api, unix) for api in range(4) for unix in cases}:
        raise ValueError('Incomplete Date record matrix')
    if lines[-1:] != ['bson date epochs: 56 decode paths, 2 encode rejections, PASS']:
        raise ValueError('Missing encode-overflow checks')
    print('BSON Date independent oracle: 56 records, PASS')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('usage: check_date_epochs.py RECORDS.tsv')
    check(sys.argv[1])
