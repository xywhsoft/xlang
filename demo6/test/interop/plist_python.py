import datetime
import pathlib
import plistlib


ROOT = pathlib.Path("tmp")


def check_xlang_plists():
    with (ROOT / "xlang_interop.plist").open("rb") as stream:
        xml_value = plistlib.load(stream)
    assert xml_value["name"] == "xlang 你好"
    assert xml_value["count"] == 42
    assert xml_value["created"] == datetime.datetime(2026, 7, 16, 8, 9, 10)
    assert xml_value["payload"] == b"\x00\xffXL"

    with (ROOT / "xlang_interop.bplist").open("rb") as stream:
        binary_value = plistlib.load(stream)
    assert binary_value["uid"] == plistlib.UID(2**64 - 1)
    assert binary_value["none"] is None
    assert binary_value["items"] == ["alpha", "beta"]


def write_python_plists():
    common = {
        "name": "python 你好",
        "count": -7,
        "ratio": 2.5,
        "enabled": False,
        "created": datetime.datetime(2025, 1, 2, 3, 4, 5),
        "payload": b"\x00\x01\xfe\xff",
        "items": [1, "two", True],
    }
    with (ROOT / "python_interop.plist").open("wb") as stream:
        plistlib.dump(common, stream, fmt=plistlib.FMT_XML, sort_keys=True)
    common["uid"] = plistlib.UID(2**64 - 1)
    common["none"] = None
    with (ROOT / "python_interop.bplist").open("wb") as stream:
        plistlib.dump(common, stream, fmt=plistlib.FMT_BINARY, sort_keys=True)


if __name__ == "__main__":
    ROOT.mkdir(parents=True, exist_ok=True)
    check_xlang_plists()
    write_python_plists()
    print("plist_python: PASS")
