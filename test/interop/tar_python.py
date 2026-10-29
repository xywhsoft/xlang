import io
import pathlib
import tarfile


ROOT = pathlib.Path("tmp")
XLANG_ARCHIVE = ROOT / "xlang_interop.tar"
LONG_NAME = "a" * 120 + ".txt"


def check_xlang_archive():
    with tarfile.open(XLANG_ARCHIVE, "r:") as archive:
        names = archive.getnames()
        assert names == ["目录", "目录/内容.txt", LONG_NAME], names
        assert archive.extractfile("目录/内容.txt").read() == b"xlang TAR"
        assert archive.extractfile(LONG_NAME).read() == b"long path"


def add_file(archive, name, data, **values):
    info = tarfile.TarInfo(name)
    info.size = len(data)
    info.mtime = values.pop("mtime", 0)
    for key, value in values.items():
        setattr(info, key, value)
    archive.addfile(info, io.BytesIO(data))


def write_python_archives():
    with tarfile.open(ROOT / "python_ustar.tar", "w", format=tarfile.USTAR_FORMAT) as archive:
        add_file(archive, "plain.txt", b"ustar")

    with tarfile.open(ROOT / "python_gnu.tar", "w", format=tarfile.GNU_FORMAT) as archive:
        add_file(archive, LONG_NAME, b"gnu long")
        link = tarfile.TarInfo("long-link")
        link.type = tarfile.SYMTYPE
        link.linkname = LONG_NAME
        link.mode = 0o777
        archive.addfile(link)

    with tarfile.open(ROOT / "python_pax.tar", "w", format=tarfile.PAX_FORMAT) as archive:
        add_file(archive, "目录/" + LONG_NAME, b"pax", uid=1 << 30, uname="用户" * 20)


if __name__ == "__main__":
    ROOT.mkdir(parents=True, exist_ok=True)
    check_xlang_archive()
    write_python_archives()
    print("tar_python: PASS")
