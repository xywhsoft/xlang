"""Check the paired example with an XML parser independent of the package."""

from pathlib import Path
import sys
from xml.etree import ElementTree as ET


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: check_external.py C.txt XL.txt")
    c_path, xl_path = map(Path, sys.argv[1:])
    c_output = c_path.read_bytes()
    if c_output != xl_path.read_bytes():
        raise ValueError("C/XLang sitemap examples differ")
    marker = b"</urlset>"
    if c_output.count(marker) != 1:
        raise ValueError("expected one URL set")
    xml, verdict = c_output.split(marker)
    if verdict.strip() != b"false":
        raise ValueError("robots.txt must block /admin/panel")
    root = ET.fromstring(xml + marker)
    namespace = "http://www.sitemaps.org/schemas/sitemap/0.9"
    if root.tag != f"{{{namespace}}}urlset":
        raise ValueError("wrong sitemap namespace")
    locations = [node.text for node in root.findall(f"{{{namespace}}}url/{{{namespace}}}loc")]
    if locations != ["https://example.test/?a=1&b=2"]:
        raise ValueError("URL or XML escaping differs")
    print("sitemap XML external gate: PASS")


if __name__ == "__main__":
    main()
