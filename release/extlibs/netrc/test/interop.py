"""Check C and XLang netrc output with an independent standard parser."""

import netrc
from pathlib import Path
import sys


def check(path):
    catalog = netrc.netrc(str(path))
    expected = {
        "api.example.com": ("alice", "", "secret"),
        "default": ("guest", "", "fallback"),
    }
    if (catalog.hosts != expected or "deploy" not in catalog.macros or
            len(catalog.macros) != 1 or
            "".join(catalog.macros["deploy"]).strip() != "echo deploy" or
            catalog.authenticators("unknown.example") != expected["default"]):
        raise AssertionError(f"unexpected netrc entries in {path}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: interop.py C_FILE XL_FILE")
    check(Path(sys.argv[1]))
    check(Path(sys.argv[2]))
    print("netrc Python/C/XLang file interop: PASS")
