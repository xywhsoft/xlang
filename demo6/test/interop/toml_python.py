import pathlib
import tomllib


ROOT = pathlib.Path("tmp")


def check_xlang_toml():
    with (ROOT / "xlang_interop.toml").open("rb") as stream:
        value = tomllib.load(stream)
    assert value["title"] == "xlang"
    assert value["count"] == 42
    assert value["enabled"] is True
    assert value["local"].microsecond == 123456
    assert value["items"] == ["alpha", "beta"]


def write_python_toml():
    # tomllib intentionally has no writer; write a standards-compliant fixture.
    text = (
        'title = "python"\n'
        'count = 7\n'
        'date = 2026-07-16\n\n'
        '[owner]\n'
        'name = "tester"\n'
    )
    (ROOT / "python_interop.toml").write_text(text, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    ROOT.mkdir(parents=True, exist_ok=True)
    check_xlang_toml()
    write_python_toml()
    print("toml_python: PASS")
