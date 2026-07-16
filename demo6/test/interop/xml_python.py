import pathlib
import xml.dom.minidom
import xml.etree.ElementTree as ET


ROOT = pathlib.Path("tmp")


def check_xlang_xml():
    path = ROOT / "xlang_interop.xml"
    tree = ET.parse(path)
    root = tree.getroot()
    assert root.tag == "root" and root.attrib["id"] == "7"
    item = root.find("{urn:test}item")
    assert item is not None and item.text == "你好" and item.attrib["key"] == "a"
    document = xml.dom.minidom.parse(str(path))
    assert any(node.nodeType == node.PROCESSING_INSTRUCTION_NODE
               for node in document.childNodes)
    assert any(node.nodeType == node.COMMENT_NODE
               for node in document.documentElement.childNodes)


def write_python_xml():
    text = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<?python generated?>\n'
        '<catalog xmlns:p="urn:python" id="9">prefix'
        '<p:book key="b">Python &amp; XLang</p:book>'
        '<!--kept--></catalog>\n'
    )
    (ROOT / "python_interop.xml").write_text(text, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    ROOT.mkdir(parents=True, exist_ok=True)
    check_xlang_xml()
    write_python_xml()
    print("xml_python: PASS")
