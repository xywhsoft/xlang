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
    output = c_output.decode("utf-8")
    url_xml, url_end, rest = output.partition("</urlset>\n")
    parsed_line, parsed_end, rest = rest.partition("\n")
    index_xml, index_end, rest = rest.partition("</sitemapindex>\n")
    index_line, index_line_end, verdict = rest.partition("\n")
    if not all((url_end, parsed_end, index_end, index_line_end)):
        raise ValueError("expected URL set and sitemap index")
    if verdict.strip() != "false":
        raise ValueError("robots.txt must block /admin/panel")
    namespace = "http://www.sitemaps.org/schemas/sitemap/0.9"
    root = ET.fromstring(url_xml + "</urlset>")
    if root.tag != f"{{{namespace}}}urlset":
        raise ValueError("wrong sitemap namespace")
    locations = [node.text for node in root.findall(f"{{{namespace}}}url/{{{namespace}}}loc")]
    if locations != ["https://example.test/?a=1&b=2"]:
        raise ValueError("URL or XML escaping differs")
    if parsed_line != f"parsed= {locations[0]}":
        raise ValueError("URL set parse differs")
    index = ET.fromstring(index_xml + "</sitemapindex>")
    if index.tag != f"{{{namespace}}}sitemapindex":
        raise ValueError("wrong sitemap index namespace")
    index_locations = [node.text for node in index.findall(
        f"{{{namespace}}}sitemap/{{{namespace}}}loc")]
    if index_locations != ["https://example.test/site.xml"]:
        raise ValueError("sitemap index differs")
    if index_line != f"index= {index_locations[0]}":
        raise ValueError("sitemap index parse differs")
    print("sitemap XML external gate: PASS")


if __name__ == "__main__":
    main()
