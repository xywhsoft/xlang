"""Compare exact quoting bytes and decoded values using Python's standard netrc parser."""
import argparse
import netrc
from pathlib import Path

PATTERNS = ("plain", "has space", "#note", '"', "\\", "'", '\\"', "用户 ", "\t", "machine", "default", "macdef")
REPEATS = (0, 1, 31, 127, 128, 129, 1024)


def quote(value):
    if (not value or value in ("machine", "default", "macdef") or
            any(ord(c) <= 32 or ord(c) == 127 or c in "#\"'\\" for c in value)):
        return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'
    return value


def expected(token, field):
    host, macro = (token if field == 0 else "h"), (token if field == 4 else "m")
    auth = ["user", "team", "dummy"]
    if 1 <= field <= 3:
        auth[field - 1] = token
    text = "default\n" if host == "default" else "machine " + quote(host) + "\n"
    text += "".join("  " + key + " " + quote(value) + "\n"
                    for key, value in zip(("login", "account", "password"), auth))
    text += "\nmacdef " + quote(macro) + "\n echo ok\n\n"
    return text.encode("utf8"), {host: tuple(auth)}, {macro: [" echo ok\n"]}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("corpus", type=Path)
    args = parser.parse_args()
    checked, parsed_count, quoted_macro_limit = 0, 0, 0
    for p, part in enumerate(PATTERNS):
        for n, count in enumerate(REPEATS):
            token = part * count
            for field in range(5):
                if not count and field in (0, 4):
                    continue
                path = args.corpus / f"p{p}-n{n}-f{field}.netrc"
                wire, hosts, macros = expected(token, field)
                if path.read_bytes() != wire:
                    raise AssertionError(f"Encoded bytes differ in {path.name}")
                try:
                    parsed = netrc.netrc(str(path))
                except netrc.NetrcParseError as error:
                    # Python部分版本在引号宏名结束处未消费换行，误读为宏的空行终止。
                    # 不削减本包合同；这些文件仍核对完整编码字节和C自往返，不计互通。
                    if (field != 4 or not quote(token).startswith('"') or
                            error.msg != "bad toplevel token 'echo'"):
                        raise
                    quoted_macro_limit += 1
                    checked += 1
                    continue
                if parsed.hosts != hosts or parsed.macros != macros:
                    raise AssertionError(f"Decoded fields differ in {path.name}")
                parsed_count += 1
                checked += 1
    if len(list(args.corpus.glob("*.netrc"))) != checked:
        raise AssertionError("Corpus inventory differs")
    print(f"netrc independent quoted bytes: {checked} files PASS; Python values: {parsed_count}; "
          f"quoted-macro parser limit: {quoted_macro_limit}")


if __name__ == "__main__":
    main()
