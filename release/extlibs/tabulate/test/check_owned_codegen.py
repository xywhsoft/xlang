"""Check the complete generated render function, not merely the extern assignment."""
import argparse
from pathlib import Path
import re


def check(text):
    bodies = re.findall(r"(?ms)^static xlir_result_string \w+\([^;\n]*\)\n\{\n.*?^\}", text)
    matches = [body for body in bodies if re.search(r"\bxtab_render\s*\(", body)]
    if len(matches) != 1:
        raise ValueError("Expected one complete native render function")
    body = matches[0]
    if not re.search(r"\.Data\s*=\s*xtab_render\s*\(", body):
        raise ValueError("Missing direct adoption")
    if re.search(r"=\s*xlir_string_copy\s*\(", body) or "xl6CStringCopyV1" in body:
        raise ValueError("Returned native payload is still copied")
    return body


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("generated", type=Path)
    parser.add_argument("--old", type=Path)
    args = parser.parse_args()
    text = args.generated.read_text(encoding="utf8")
    body = check(text)
    variants = [text.replace(body, body.replace("xtab_render(", "not_native_render(")),
                text.replace(body, body.replace("\n{\n", "\n{\n    v0 = xlir_string_copy(v0);\n", 1))]
    if args.old:
        variants.append(args.old.read_text(encoding="utf8"))
    for variant in variants:
        try:
            check(variant)
        except ValueError:
            continue
        raise AssertionError("Broken or old return path passed")
    print(f"tabulate owned render: complete function PASS, {len(variants)} rejection controls")


if __name__ == "__main__":
    main()
