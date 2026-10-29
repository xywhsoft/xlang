"""用独立 iCalendar 解析器核对 C/XLang 输出与长 UTF-8 折行。"""

from pathlib import Path
import sys

from icalendar import Calendar


def event_from(path):
    events = Calendar.from_ical(path.read_bytes()).walk("VEVENT")
    if len(events) != 1:
        raise ValueError(f"{path}: expected one VEVENT, got {len(events)}")
    return events[0]


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: check_external.py C.ics XL.ics LONG.ics")
    c_path, xl_path, long_path = map(Path, sys.argv[1:])
    c_event = event_from(c_path)
    xl_event = event_from(xl_path)
    long_event = event_from(long_path)
    if c_path.read_bytes() != xl_path.read_bytes():
        raise ValueError("C and XLang calendar bytes differ")
    for event in (c_event, xl_event):
        if str(event["SUMMARY"]) != "团队例会" or str(event["UID"]) != "standup@team":
            raise ValueError("calendar fields differ from the example")
    if str(long_event["DESCRIPTION"]) != "长" * 3000:
        raise ValueError("long UTF-8 description did not survive folding")
    print("icalendar independent parse: PASS")


if __name__ == "__main__":
    main()
