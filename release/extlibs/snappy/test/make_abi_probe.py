"""把真正 cgen 的桥接原型与公开 C 头放在同一翻译单元，不手抄测试 ABI。"""
from pathlib import Path
import re
import sys

if len(sys.argv) != 3:
    raise SystemExit('usage: make_abi_probe.py generated.c output-directory')
generated, work = Path(sys.argv[1]), Path(sys.argv[2])
language = Path(__file__).resolve().parents[1] / 'src/snappy.xl'
expected = set(re.findall(r'extern "(xsnappy_[a-z_]+)"', language.read_text(encoding='utf-8')))
decls = re.findall(r'^extern [^\r\n;]*\bxsnappy_[a-z_]+\([^;\r\n]*\);',
    generated.read_text(encoding='utf-8'), re.M)
actual = {re.search(r'\bxsnappy_[a-z_]+(?=\()', declaration)[0] for declaration in decls}
if len(decls) != len(expected) or actual != expected:
    raise ValueError(f'generated ABI coverage: expected={sorted(expected)} actual={sorted(actual)}')
body = '#include "snappy_native.h"\n' + '\n'.join(decls)
wrong = re.sub(r'^extern [^\r\n;]*\bxsnappy_version\(', 'extern int xsnappy_version(', body, flags=re.M)
if body == wrong:
    raise ValueError('ABI negative did not mutate a declaration')
(work / 'abi.c').write_text(body, encoding='utf-8')
(work / 'abi_wrong.c').write_text(wrong, encoding='utf-8')
print(f'snappy actual generated ABI: {len(decls)} declarations')
