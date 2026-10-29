"""Use the Python standard library to inspect both generated EPUB examples."""

from pathlib import Path
import sys
from xml.etree import ElementTree as ET
from zipfile import ZIP_STORED, ZipFile


OCF = "urn:oasis:names:tc:opendocument:xmlns:container"
OPF = "http://www.idpf.org/2007/opf"
DC = "http://purl.org/dc/elements/1.1/"
XHTML = "http://www.w3.org/1999/xhtml"


def check_book(path: Path) -> None:
    with ZipFile(path) as archive:
        entries = archive.infolist()
        names = [entry.filename for entry in entries]
        assert len(names) == len(set(names)), "duplicate ZIP entries"
        assert names[0] == "mimetype", "mimetype must be first"
        assert entries[0].compress_type == ZIP_STORED and not entries[0].extra
        assert archive.read("mimetype") == b"application/epub+zip"
        assert archive.testzip() is None, "ZIP CRC mismatch"

        container = ET.fromstring(archive.read("META-INF/container.xml"))
        rootfile = container.find(f".//{{{OCF}}}rootfile")
        assert rootfile is not None
        assert rootfile.get("full-path") == "OEBPS/content.opf"
        assert rootfile.get("media-type") == "application/oebps-package+xml"

        package = ET.fromstring(archive.read("OEBPS/content.opf"))
        assert package.get("version") == "3.0"
        metadata = package.find(f"{{{OPF}}}metadata")
        assert metadata is not None
        title = metadata.find(f"{{{DC}}}title")
        assert title is not None and title.text == "xlang 电子书"
        identifier = metadata.find(f"{{{DC}}}identifier")
        assert identifier is not None and identifier.text == "urn:xlang:example:epub"
        assert package.get("unique-identifier") == identifier.get("id")

        items = package.findall(f"{{{OPF}}}manifest/{{{OPF}}}item")
        by_id = {item.get("id"): item for item in items}
        assert len(items) == len(by_id), "duplicate manifest IDs"
        nav = next(item for item in items if "nav" in (item.get("properties") or "").split())
        assert nav.get("href") == "nav.xhtml"
        for item in items:
            assert "OEBPS/" + item.get("href", "") in names
        references = package.findall(f"{{{OPF}}}spine/{{{OPF}}}itemref")
        assert [ref.get("idref") for ref in references] == ["ch1", "ch2"]
        assert all(ref.get("idref") in by_id for ref in references)

        for index, heading in enumerate(("开始", "下一步"), 1):
            chapter = ET.fromstring(archive.read(f"OEBPS/ch{index}.xhtml"))
            assert heading in "".join(chapter.itertext())
            assert chapter.tag == f"{{{XHTML}}}html"
        navigation = ET.fromstring(archive.read("OEBPS/nav.xhtml"))
        nav_text = "".join(navigation.itertext())
        assert "第一章" in nav_text and "第二章" in nav_text


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: check_external.py C.epub XL.epub")
    c_path, xl_path = map(Path, sys.argv[1:])
    assert c_path.read_bytes() == xl_path.read_bytes(), "C/XLang EPUB bytes differ"
    check_book(c_path)
    check_book(xl_path)
    print("epub ZIP/XML external gate: PASS")


if __name__ == "__main__":
    main()
