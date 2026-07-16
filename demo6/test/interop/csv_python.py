import csv
import pathlib


ROOT = pathlib.Path("tmp")


def check_xlang_csv():
    with (ROOT / "xlang_interop.csv").open("r", encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.reader(stream))
    assert rows == [
        ["name", "city", "note"],
        ["Alice", "上海", 'comma, quote " and newline\nare safe'],
        ["Bob", "深圳", ""],
    ], rows


def write_python_csv():
    with (ROOT / "python_interop.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, dialect="excel")
        writer.writerow(["name", "value", "empty"])
        writer.writerow(["测试", "a,b", ""])
        writer.writerow(["quoted", 'q"q', "line1\r\nline2"])


if __name__ == "__main__":
    ROOT.mkdir(parents=True, exist_ok=True)
    check_xlang_csv()
    write_python_csv()
    print("csv_python: PASS")
