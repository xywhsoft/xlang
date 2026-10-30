"""用 python-snappy 独立解码标准空块；C 错误退出必须保留，不能当作空正文。"""
import subprocess
import sys
import snappy

if len(sys.argv) != 2 or snappy.__version__ != '0.7.3':
    raise SystemExit('usage: python-snappy 0.7.3 check_empty_interop.py interop-executable')
identifier = bytes.fromhex('ff060000734e61507059')
empty = [bytes.fromhex(value) for value in ('01040000d8ea82a2', '00050000d8ea82a200')]
abc = bytes.fromhex('010700006e57f121616263')
cases = 0
for mode in range(8):
    wire, plain = identifier, b''
    if mode & 2:
        wire += abc
        plain += b'abc'
    wire += empty[mode & 1] + bytes.fromhex('fe000000')
    if mode & 4:
        wire += identifier + abc
        plain += b'abc'
    decoded = snappy.StreamDecompressor().decompress(wire)
    if decoded != plain:
        raise ValueError('independent decoder disagrees with fixture')
    result = subprocess.run([sys.argv[1], 'decode'], input=wire, capture_output=True, timeout=20)
    if result.returncode or result.stdout != plain:
        raise RuntimeError(f'C empty chunk {mode}: exit={result.returncode} {result.stderr!r}')
    cases += 1
print(f'snappy empty chunk independent interop: {cases} PASS')
