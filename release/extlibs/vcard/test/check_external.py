"""用独立解析器核对 vCard 3.0/4.0 输出和长 UTF-8 折行。"""

from pathlib import Path
import sys

import vobject


def read_card(path):
    return vobject.readOne(path.read_text(encoding="utf-8"))


def main():
    if len(sys.argv) != 5:
        raise SystemExit("usage: check_external.py C.vcf XL.vcf LONG.vcf FOUR.vcf")
    c_path, xl_path, long_path, four_path = map(Path, sys.argv[1:])
    if c_path.read_bytes() != xl_path.read_bytes():
        raise ValueError("C and XLang vCard bytes differ")
    for path in (c_path, xl_path):
        card = read_card(path)
        if card.fn.value != "王小明" or len(card.email_list) != 2:
            raise ValueError(f"{path}: contact fields differ from the example")
    if read_card(long_path).note.value != "长" * 3000:
        raise ValueError("long UTF-8 note did not survive folding")
    modern = read_card(four_path)
    if (
        modern.version.value != "4.0"
        or modern.fn.value != "Alex"
        or modern.tel.value != "tel:+123"
    ):
        raise ValueError("vCard 4.0 fields differ from the generated card")
    print("vobject independent parse: PASS")


if __name__ == "__main__":
    main()
