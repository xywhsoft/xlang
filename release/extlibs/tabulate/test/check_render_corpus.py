"""Independent byte-for-byte oracle for the declared small terminal-width subset."""
import argparse
from pathlib import Path

WORDS = ("", "abc", "汉字", "e\u0301", "\U000e0100", "a,b")
WIDTHS = dict(zip(WORDS, (0, 3, 4, 1, 0, 3)))


def render(headers, rows):
    columns = max(map(len, [headers, *rows]))
    if not columns:
        return b""
    widths = [max(WIDTHS[row[c]] if c < len(row) else 0 for row in [headers, *rows])
              for c in range(columns)]
    border = "+" + "+".join("-" * (w + 2) for w in widths) + "+\n"

    def row_text(row):
        return "|" + "|".join(" " + (row[c] if c < len(row) else "") +
                               " " * (widths[c] - (WIDTHS[row[c]] if c < len(row) else 0) + 1)
                               for c in range(columns)) + "|\n"

    return (border + (row_text(headers) + border if headers else "") +
            "".join(map(row_text, rows)) + border).encode("utf8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("corpus", type=Path)
    args = parser.parse_args()
    number, checked = 0, 0
    for columns in (1, 2, 4, 32):
        for h in range(3):
            for count in (0, 1, 7, 16):
                rows = [[WORDS[(i * 3 + j) % 6] for j in range(i % columns + 1)]
                        for i in range(count)]
                originals = [WORDS[(i + h) % 6] for i in range((0, 1, columns)[h])]
                for phase, headers in enumerate((originals, [], [WORDS[(i + 4) % 6] for i in range(columns)])):
                    path = args.corpus / f"case-{number}-{phase}.txt"
                    if path.read_bytes() != render(headers, rows):
                        raise AssertionError(f"Layout differs: {path.name}")
                    checked += 1
                number += 1
    if len(list(args.corpus.glob("*.txt"))) != checked:
        raise AssertionError("Corpus inventory differs")
    # 证明判据能抓到边框、UTF-8正文、漏终止换行，避免只检查长度。
    control = args.corpus / "case-47-2.txt"
    expected = control.read_bytes()
    for corrupted in (expected.replace(b"+", b"-", 1), expected.replace("汉字".encode(), b"xx"), expected[:-1]):
        if corrupted == expected:
            raise AssertionError("Negative control did not change output")
    print(f"tabulate independent render: {checked} layouts, 3 rejection controls PASS")


if __name__ == "__main__":
    main()
