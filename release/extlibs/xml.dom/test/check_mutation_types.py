"""Compare same-document insertion legality with Python's independent DOM implementation."""

import argparse
import hashlib
import itertools
from pathlib import Path
from xml.dom import HierarchyRequestErr
from xml.dom.minidom import Document

KINDS = (9, 1, 3, 8, 7)


def node(document, kind, name):
    if kind == 9:
        return document
    if kind == 1:
        return document.createElement(name)
    if kind == 3:
        return document.createTextNode("你好")
    if kind == 8:
        return document.createComment("note")
    if kind == 7:
        return document.createProcessingInstruction(name, "data")
    raise ValueError("invalid corpus node kind")


def expected():
    for attached, parent_kind, child_kind in itertools.product((0, 1), KINDS, KINDS):
        document = Document()
        try:
            parent = node(document, parent_kind, "parent")
            child = node(document, child_kind, "child")
            if attached:
                document.appendChild(parent if parent_kind == 1 else document.createElement("anchor"))
            try:
                parent.appendChild(child)
                accepted = 1
            except HierarchyRequestErr:
                accepted = 0
            yield attached, 0, parent_kind, child_kind, accepted
        finally:
            document.unlink()
    # 本包不自动跨文档收养；这是显式所有权合同，不冒充 minidom 的行为。
    for parent_kind, child_kind in itertools.product(KINDS, KINDS):
        yield 0, 1, parent_kind, child_kind, 0


def verify(lines):
    references = list(expected())
    if len(lines) != len(references) or len(lines) != 75:
        raise ValueError("incomplete DOM type matrix")
    for index, (line, reference) in enumerate(zip(lines, references, strict=True)):
        values = tuple(map(int, line.split("\t")))
        if values != (index, *reference):
            raise ValueError(f"independent DOM insertion differs at {index}: {values} != {(index, *reference)}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("records", type=Path)
    args = parser.parse_args()
    text = args.records.read_text(encoding="ascii")
    lines = text.splitlines()
    verify(lines)
    # 分别模拟接受文档子节点、接受文档层文本、错误拒绝合法元素的记录。
    for index in (5, 2, 6):
        changed = lines.copy()
        fields = changed[index].split("\t")
        fields[-1] = str(1 - int(fields[-1]))
        changed[index] = "\t".join(fields)
        try:
            verify(changed)
        except ValueError:
            continue
        raise ValueError("DOM oracle accepted damaged record")
    print(f"XML DOM independent types: 50 minidom / 25 ownership cases, 3 controls, PASS ({hashlib.sha256(text.replace(chr(13), '').encode()).hexdigest()})")


if __name__ == "__main__":
    main()
