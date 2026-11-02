"""Use Python's independent GNU MO reader; never parse PO with package code."""
import gettext
from pathlib import Path
import sys

with Path(sys.argv[1]).open("rb") as stream:
    catalog = gettext.GNUTranslations(stream)
rows = [
    ("plain", catalog.gettext("plain")),
    ("menu", catalog.pgettext("menu", "Open")),
    ("toolbar", catalog.pgettext("toolbar", "Open")),
    ("file(1)", catalog.ngettext("file", "files", 1)),
    ("file(2)", catalog.ngettext("file", "files", 2)),
    ("empty context", catalog.pgettext("", "Open")),
    ("empty id", catalog.pgettext("", "")),
    ("draft empty id", catalog.pgettext("menu", "")),
    ("untranslated empty id", catalog.pgettext("toolbar", "")),
]
actual = "".join(f"{label}: {text or '[]'}\n" for label, text in rows)
expected = Path(sys.argv[2]).read_text(encoding="utf8")
assert actual == expected, (actual, expected)
print("gettext Python GNU MO: 9 golden queries PASS")
