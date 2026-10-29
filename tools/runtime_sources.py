"""Read the shared physical runtime translation-unit list used by release builds."""
import argparse
import re
from pathlib import Path


OBJECT = re.compile(r'^"%XL6_RELEASE_CC%"[^\n]* -c ([^\n]+\.c) -o "%XL6_BUILD_TMP%\\([A-Za-z0-9_]+\.o)"\s*$', re.M)


def source_names(root):
    batch = (root / "build_dll.bat").read_text(encoding="utf-8-sig")
    template = (root / "tools/runtime_dll.rsp.in").read_text(encoding="utf-8-sig")
    shared = [match.group(1) for match in OBJECT.finditer(batch)]
    direct = re.findall(r"\bsrc[\\/][^\s\"']+\.c\b", template)
    if not shared or not direct:
        raise ValueError("missing shared objects or runtime sources")
    return [entry.replace("\\", "/") for entry in shared + direct]


def sources(root):
    entries = source_names(root)
    result = []
    seen = set()
    for entry in entries:
        relative = entry.replace("\\", "/")
        if relative in seen:
            raise ValueError(f"duplicate runtime translation unit: {relative}")
        seen.add(relative)
        path = root / relative
        if not path.is_file():
            raise ValueError(f"missing runtime translation unit: {relative}")
        result.append(path.resolve())
    if not result:
        raise ValueError("empty runtime translation-unit list")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    for source in sources(args.root.resolve()):
        print(source.as_posix())
