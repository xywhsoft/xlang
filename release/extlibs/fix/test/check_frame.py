"""Independently check the wire frame printed by the C/XLang examples."""

import sys


def check(output):
    prefix = "FIX message: "
    first = output.splitlines()[0]
    if not first.startswith(prefix):
        raise ValueError("FIX example did not print a frame")
    frame = first[len(prefix):].replace("|", "\x01").encode("ascii")
    fields = frame.split(b"\x01")
    if len(fields) < 4 or fields[-1] != b"" or fields[0] != b"8=FIX.4.4":
        raise ValueError("invalid FIX example envelope")
    if not fields[1].startswith(b"9=") or not fields[-2].startswith(b"10="):
        raise ValueError("FIX length/checksum fields are misplaced")
    body_start = frame.index(b"\x01", frame.index(b"\x01") + 1) + 1
    checksum_start = len(frame) - len(fields[-2]) - 1
    body_length = fields[1][2:]
    checksum = fields[-2][3:]
    if (not body_length.isdigit() or len(checksum) != 3 or
            not checksum.isdigit()):
        raise ValueError("FIX length/checksum fields are not decimal")
    if int(body_length) != checksum_start - body_start:
        raise ValueError("FIX BodyLength differs from wire bytes")
    if int(checksum) != sum(frame[:checksum_start]) % 256:
        raise ValueError("FIX CheckSum differs from wire bytes")
    tags = [int(field.split(b"=", 1)[0]) for field in fields[:-1]]
    if tags != [8, 9, 35, 49, 56, 11, 55, 54, 38, 40, 44, 10]:
        raise ValueError("FIX example reordered its application fields")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: check_frame.py EXAMPLE_OUTPUT")
    check(sys.argv[1])
    print("FIX independent frame: PASS")
