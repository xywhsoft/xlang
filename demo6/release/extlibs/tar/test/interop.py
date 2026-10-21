"""Use Python's XZ and TAR readers as an independent format oracle."""
import io
import lzma
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile


def run_xlang(xl, release, mode, archive):
    result = subprocess.run(
        [xl, "run", "extlibs/tar/test/interop.xl", "--", mode, str(archive)],
        cwd=release, capture_output=True, text=True, encoding="utf-8"
    )
    if result.returncode != 0 or "tar xz interop: PASS" not in result.stdout:
        raise AssertionError(f"{mode}: {result.stdout}\n{result.stderr}")


def main(release):
    xl = str(release / ("xl.exe" if os.name == "nt" else "xl"))
    text = "你好 tar".encode("utf-8")
    with tempfile.TemporaryDirectory(prefix="xlang-tar-") as temporary:
        archive = Path(temporary) / "interop.tar.xz"
        run_xlang(xl, release, "write", archive)
        payload = lzma.decompress(archive.read_bytes(), format=lzma.FORMAT_XZ)
        with tarfile.open(fileobj=io.BytesIO(payload), mode="r:") as reader:
            names = reader.getnames()
            if names != ["interop.txt"]:
                raise AssertionError(f"unexpected TAR entries: {names}")
            entry = reader.extractfile(names[0])
            if entry is None or entry.read() != text:
                raise AssertionError("XLang archive content differs from Python")

        content = io.BytesIO()
        with tarfile.open(fileobj=content, mode="w:") as writer:
            entry = tarfile.TarInfo("interop.txt")
            entry.size = len(text)
            writer.addfile(entry, io.BytesIO(text))
        archive.write_bytes(lzma.compress(content.getvalue(), format=lzma.FORMAT_XZ))
        run_xlang(xl, release, "read", archive)
    print("tar .xz Python/XLang bidirectional interop: PASS")


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve())
