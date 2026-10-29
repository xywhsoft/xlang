#!/usr/bin/env python3

"""从当前 xruntime 产品单头生成 TCC 运行时符号清单。"""

from __future__ import annotations

import argparse
from pathlib import Path
import re


IMPLEMENTATION_MARKERS = (
    "#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRUNTIME_IMPLEMENTATION_ONCE)",
    "#if defined(XRT_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION_ONCE)",
)
FUNCTION = re.compile(
    r"\bXRT_API\s+[^;{}]*?\b(xrt[A-Za-z0-9_]*)\s*\([^;{}]*\)\s*;",
    re.DOTALL,
)


def _declarations(header: Path) -> str:
    """返回去除注释和预处理指令后的产品公共声明区。"""

    text = header.read_text(encoding="utf-8")
    markers = [text.find(value) for value in IMPLEMENTATION_MARKERS]
    markers = [value for value in markers if value >= 0]
    if not markers:
        raise ValueError(f"runtime implementation marker is missing: {header}")
    marker = min(markers)
    text = text[:marker]
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.DOTALL)
    text = re.sub(r"//[^\r\n]*", " ", text)
    text = re.sub(
        r"(?m)^[ \t]*#[^\r\n]*(?:\\\r?\n[^\r\n]*)*",
        " ",
        text,
    )
    return text


def _api_names(header: Path) -> list[str]:
    """提取并校验产品单头暴露的公共函数名。"""

    names = FUNCTION.findall(_declarations(header))
    unique = sorted(set(names))
    if len(unique) != len(names):
        raise ValueError(f"duplicate XRT_API function declarations were found: {header}")
    if len(unique) < 1000:
        raise ValueError(f"too few runtime API functions were found: {len(unique)}")
    return unique



def _render(header: Path) -> str:
    """按稳定顺序渲染 TCC 运行时函数清单。"""

    unique = _api_names(header)

    lines = [
        "/* 由 tools/gen_xrt_symbol_table.py 或 .ps1 生成；不要手工修改。 */",
        f"/* 当前登记 {len(unique)} 个 XRT/xruntime 公共函数。 */",
    ]
    lines.extend(f"XL_XRT_SYMBOL({name})" for name in unique)
    return "\n".join(lines) + "\n"


def main() -> int:
    """生成清单，或验证提交的清单与当前单头一致。"""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    root = arguments.root.resolve()
    header = root / "lib" / "xruntime" / "xruntime.h"
    output = root / "src" / "api" / "xl_xrt_symbols.inc"
    expected = _render(header)

    if arguments.check:
        actual = output.read_text(encoding="utf-8") if output.is_file() else ""
        if actual != expected:
            print(f"runtime symbol table is stale: {output}")
            return 1
        print(f"runtime symbol table is current: {expected.count('XL_XRT_SYMBOL(')} functions")
        return 0

    output.write_text(expected, encoding="utf-8", newline="\n")
    print(f"generated {output}: {expected.count('XL_XRT_SYMBOL(')} functions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
