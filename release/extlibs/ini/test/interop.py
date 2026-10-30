"""Check bidirectional interoperability with Python's independent INI parser."""

import configparser
import io
import subprocess
import sys


def run(program, mode, data=None):
    result = subprocess.run([program, mode], input=data, capture_output=True,
                            check=False, timeout=15)
    if result.returncode:
        raise AssertionError(f"{mode}: exit {result.returncode}: {result.stderr!r}")
    return result.stdout


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: interop.py <compiled-interop-program>")
    program = sys.argv[1]
    expected = {"server": {"port": "9090", "name": "\u4f60\u597d"},
                "client": {"timeout": "5"}}

    # C writer -> Python parser.
    parsed = configparser.ConfigParser(interpolation=None)
    parsed.read_string(run(program, "emit").decode("utf-8"))
    for section, values in expected.items():
        assert section in parsed, section
        for key, value in values.items():
            assert parsed[section][key] == value, (section, key)

    # Python writer -> C parser; Python deliberately inserts spaces around '='.
    written = configparser.ConfigParser(interpolation=None)
    written.read_dict(expected)
    output = io.StringIO()
    written.write(output)
    run(program, "read", output.getvalue().encode("utf-8"))

    # 非严格模式覆盖较早的键；续行归属也用独立实现的实际值核对。
    updates = ("[server]\nname=old\nport=9090\n"
               "name=\u4f60\u597d\n    next\n[client]\ntimeout=5\n")
    parsed = configparser.ConfigParser(interpolation=None, strict=False)
    parsed.read_string(updates)
    assert parsed["server"]["name"] == "\u4f60\u597d\nnext"
    assert parsed["server"]["port"] == "9090"
    assert parsed["client"]["timeout"] == "5"
    run(program, "read-updates", updates.encode("utf-8"))
    print("ini independent C/Python interoperability: PASS")


if __name__ == "__main__":
    main()
