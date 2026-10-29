"""用标准库独立检查双语范例的 iCalendar 文本和 UTF-8 折行。"""

from pathlib import Path
import sys


def require(condition, message):
    if not condition:
        raise ValueError(message)


def lines(path):
    data = path.read_bytes()
    text = data.decode("utf-8")
    require(text.endswith("\r\n"), f"{path}: missing final CRLF")
    physical = text.split("\r\n")[:-1]
    require(all("\r" not in line and "\n" not in line for line in physical),
            f"{path}: invalid line ending")
    require(all(len(line.encode("utf-8")) <= 75 for line in physical),
            f"{path}: line exceeds 75 bytes")
    unfolded = []
    for line in physical:
        if line.startswith((" ", "\t")):
            require(bool(unfolded), f"{path}: orphan fold")
            unfolded[-1] += line[1:]
        else:
            unfolded.append(line)
    return unfolded


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: check_wire.py C.ics XL.ics LONG.ics")
    c_path, xl_path, long_path = map(Path, sys.argv[1:])
    require(c_path.read_bytes() == xl_path.read_bytes(), "C/XLang calendars differ")
    example = lines(c_path)
    require(bool(example) and example[0] == "BEGIN:VCALENDAR" and
            example[-1] == "END:VCALENDAR", "calendar envelope differs")
    require(example.count("BEGIN:VEVENT") == example.count("END:VEVENT") == 1,
            "example must contain one event")
    for field in ("VERSION:2.0", "UID:standup@team", "SUMMARY:团队例会",
                  "RRULE:FREQ=WEEKLY;BYDAY=MO,WE"):
        require(field in example, f"missing field: {field}")
    long_event = lines(long_path)
    require("DESCRIPTION:" + "长" * 3000 in long_event,
            "long UTF-8 description did not survive folding")
    print("ical wire and C/XLang parity: PASS")


if __name__ == "__main__":
    main()
