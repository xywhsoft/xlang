"""用 Python zipfile 与包的 C API 双向核对标准 ZIP 线格式。"""

import io
import os
import subprocess
import sys
import zipfile
from pathlib import Path


def main():
    if sys.platform != "win32":
        raise SystemExit("this gate uses the Windows package C runner")
    root = Path(__file__).resolve().parents[4]
    command = [
        "powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
        "tools/run_extlib_c.ps1", "-Package", "zip", "-Source",
        "release/extlibs/zip/test/interop.c",
    ]

    def run(mode, hex_data=""):
        env = os.environ.copy()
        env["XL_ZIP_INTEROP_MODE"] = mode
        env["XL_ZIP_INTEROP_HEX"] = hex_data
        result = subprocess.run(command, cwd=root, env=env, capture_output=True)
        if result.returncode != 0:
            raise RuntimeError(f"C ZIP {mode} returned {result.returncode}: "
                               f"{result.stderr.decode(errors='replace')}")
        return result.stdout.decode("ascii").strip()

    def require(condition, message):
        if not condition:
            raise RuntimeError(message)

    # 两端分别创建归档，不使用同一实现的往返结果当作独立证据。
    wire = bytes.fromhex(run("emit"))
    with zipfile.ZipFile(io.BytesIO(wire)) as archive:
        require(archive.namelist() == [
            "docs/", "docs/hello.txt", "data.bin", "docs/你好.txt",
        ], "C archive entry names differ")
        require(archive.read("docs/hello.txt") == b"hello ZIP", "C text differs")
        require(archive.read("data.bin") == b"\x00\xff", "C binary differs")
        require(archive.read("docs/你好.txt") == b"world", "C UTF-8 entry differs")
        require(archive.getinfo("data.bin").compress_type == zipfile.ZIP_STORED,
                "C stored method differs")
        require(archive.getinfo("docs/hello.txt").compress_type == zipfile.ZIP_DEFLATED,
                "C deflate method differs")
        require(archive.testzip() is None, "C archive CRC check failed")

    output = io.BytesIO()
    with zipfile.ZipFile(output, "w") as archive:
        archive.writestr("docs/", b"")
        archive.writestr("docs/hello.txt", b"hello ZIP",
                         compress_type=zipfile.ZIP_DEFLATED)
        archive.writestr("data.bin", b"\x00\xff",
                         compress_type=zipfile.ZIP_STORED)
        archive.writestr("docs/你好.txt", b"world",
                         compress_type=zipfile.ZIP_DEFLATED)
    wire = output.getvalue()
    require(run("read", wire.hex()) == "zip independent interop: PASS",
            "C reader did not accept Python archive")
    with zipfile.ZipFile(io.BytesIO(wire)) as archive:
        local = archive.getinfo("data.bin").header_offset
    name_length = int.from_bytes(wire[local + 26:local + 28], "little")
    extra_length = int.from_bytes(wire[local + 28:local + 30], "little")
    payload = local + 30 + name_length + extra_length
    require(wire[payload:payload + 2] == b"\x00\xff",
            "stored fixture payload moved")
    damaged = bytearray(wire)
    damaged[payload] ^= 1
    require(run("reject", damaged.hex()) == "zip damaged CRC: PASS",
            "C reader did not reject damaged CRC")
    print("zip Python/C interop: PASS")


if __name__ == "__main__":
    main()
