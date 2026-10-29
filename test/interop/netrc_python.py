import netrc
import pathlib


ROOT = pathlib.Path("tmp")


def check_xlang_netrc():
    value = netrc.netrc(str(ROOT / "xlang_interop.netrc"))
    assert value.authenticators("api.example.com") == (
        "alice", "ops", "s3cr et"
    )
    assert value.authenticators("missing.example.com") == (
        "guest", "", "guest"
    )
    assert value.macros["deploy"] == [" echo deploy\n", " echo done\n"]


def write_python_netrc():
    text = (
        'machine python.example login bob account qa password "p a s s"\n'
        "default login fallback password secret\n"
        "macdef release\n"
        " echo build\n"
        " echo publish\n"
        "\n"
    )
    (ROOT / "python_interop.netrc").write_text(text, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    ROOT.mkdir(parents=True, exist_ok=True)
    check_xlang_netrc()
    write_python_netrc()
    print("netrc_python: PASS")
