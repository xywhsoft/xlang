"""用 polib 编译 MO，再由 Python 标准库独立核对查询结果。"""

from gettext import GNUTranslations
from pathlib import Path
import sys

import polib


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: check_external.py OUTPUT.mo")
    output = Path(sys.argv[1])
    catalog = polib.POFile()
    catalog.metadata = {
        "Content-Type": "text/plain; charset=UTF-8",
        "Plural-Forms": "nplurals=3; plural=n==1?0:n==2?1:2;",
        "Language": "zh_CN",
    }
    catalog.append(polib.POEntry(msgctxt="menu", msgid="Open", msgstr="打开菜单"))
    catalog.append(polib.POEntry(
        msgid="file", msgid_plural="files",
        msgstr_plural={"0": "一个文件", "1": "两个文件", "2": "多个文件"},
    ))
    catalog.append(polib.POEntry(msgid="Draft", msgstr="未审核", flags=["fuzzy"]))
    catalog.save_as_mofile(str(output))
    with output.open("rb") as source:
        reference = GNUTranslations(source)
    expected = ["打开菜单", "一个文件", "两个文件", "多个文件", "Draft"]
    actual = [
        reference.pgettext("menu", "Open"),
        reference.ngettext("file", "files", 1),
        reference.ngettext("file", "files", 2),
        reference.ngettext("file", "files", 3),
        reference.gettext("Draft"),
    ]
    if actual != expected:
        raise ValueError(f"GNUTranslations mismatch: {actual!r}")
    print("GNU MO reference: PASS")


if __name__ == "__main__":
    main()
