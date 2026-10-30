"""独立固定完整规范输出，不能仅让两个同源写出器彼此比较。"""
from pathlib import Path
import sys


expected = ("BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:test\r\n"
            "BEGIN:VEVENT\r\nUID:one\r\nDTSTAMP:20261030T000000Z\r\n"
            "DTSTART:20261030T120000Z\r\nSUMMARY:团队例会\r\n"
            "DESCRIPTION:第一行\\n第二行\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n").encode()
if len(sys.argv) < 2:
    raise SystemExit("usage: check_folded.py output.ics ...")
for name in sys.argv[1:]:
    data = Path(name).read_bytes()
    if data != expected:
        raise ValueError(f"canonical calendar differs: {name}")
    data.decode("utf-8")
print(f"ical canonical UTF8/wire: {len(sys.argv) - 1} files PASS")
