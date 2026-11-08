"""Check product version independently of the embedding ABI version."""
from pathlib import Path
import argparse
import ctypes
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from runtime_layout import runtime_directory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--products", type=Path)
    parser.add_argument("--directory", type=Path)
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc"))
    args = parser.parse_args()
    root = args.root.resolve()
    products = (args.products or runtime_directory(root)).resolve()
    sdk_source = products / "include"
    if not sdk_source.is_dir():
        sdk_source = runtime_directory(root) / "include"
    version_text = json.loads((root / "ide/data/version.json").read_text(encoding="utf-8"))["displayVersion"]
    if not re.fullmatch(r"\d+\.\d+\.\d+", version_text):
        raise RuntimeError("invalid product displayVersion")
    major, minor, patch = map(int, version_text.split("."))
    if major > 65535 or minor > 255 or patch > 255:
        raise RuntimeError("product version does not fit its version code")
    version_code = (major << 16) | (minor << 8) | patch
    version_hex = f"0x{version_code:08x}"
    directory = (args.directory or root / "build/test/release-version").resolve()
    directory.mkdir(parents=True, exist_ok=True)
    report = {"complete": False, "checks": [], "products": [],
              "expected": {"version": version_text, "versionCode": version_code,
                           "versionCodeHex": version_hex}}

    def record(name):
        report["checks"].append(name)
        print(name, flush=True)

    header = (sdk_source / "xl_version.h").read_text(encoding="utf-8")
    expected = {"MAJOR": str(major), "MINOR": str(minor), "PATCH": str(patch),
                "TEXT": json.dumps(version_text), "CODE": version_hex + "u"}
    for key, value in expected.items():
        if not re.search(r"(?m)^#define XL_VERSION_" + key + r"\s+" + re.escape(value) + r"\s*$", header):
            raise RuntimeError("product version header mismatch: " + key)
    for name in ["xl.h", "xlc.h"]:
        text = (sdk_source / name).read_text(encoding="utf-8")
        macro = "XLC_API_VERSION" if name == "xlc.h" else "XL_API_VERSION"
        pattern = r"1u" if name == "xl.h" else r"XL_API_VERSION"
        if not re.search(r"#define\s+" + macro + r"\s+" + pattern + r"\b", text):
            raise RuntimeError("product version changed the embedding ABI: " + macro)
    record("product header and independent ABI versions")
    for name in ("version.json", "release.json"):
        metadata = products / "data" / name
        if metadata.is_file():
            if json.loads(metadata.read_text(encoding="utf-8"))["displayVersion"] != version_text:
                raise RuntimeError("runtime displayVersion mismatch: " + str(metadata))
            record("runtime " + name + " displayVersion")
    env = dict(os.environ)
    env["PATH"] = str(products) + os.pathsep + env.get("PATH", "")
    cli = products / ("xl.exe" if os.name == "nt" else "xl")
    for arguments, expected_exit in [(["--version"], 0), (["--version", "--json"], 0),
                                     (["--version", "unexpected"], 1),
                                     (["--version", "--json", "unexpected"], 1)]:
        result = subprocess.run([str(cli), *arguments], env=env, capture_output=True, timeout=30)
        if result.returncode != expected_exit:
            raise RuntimeError("version command exit mismatch: " + repr(arguments))
        if expected_exit == 0:
            text = result.stdout.decode("utf-8").strip()
            if "--json" in arguments:
                if json.loads(text) != report["expected"]:
                    raise RuntimeError("version JSON mismatch")
            elif text != f"xlang {version_text} ({version_hex})":
                raise RuntimeError("version text mismatch")
        record("CLI " + " ".join(arguments))

    libraries = [products / name for name in
                 (["xl.dll", "xlc.dll"] if os.name == "nt" else ["libxl.so", "libxlc.so"])]
    # A separate interpreter prevents DLL initialization or TLS state from a
    # previously loaded product from masking a candidate's actual export.
    probe = """import ctypes,sys
library=ctypes.CDLL(sys.argv[1])
library.xlGetVersionString.restype=ctypes.c_char_p
library.xlGetVersionCode.restype=ctypes.c_uint
assert library.xlGetVersionString()==sys.argv[2].encode('ascii')
assert library.xlGetVersionCode()==int(sys.argv[3])
"""
    for library in libraries:
        subprocess.run([sys.executable, "-c", probe, str(library), version_text, str(version_code)],
                       env=env, check=True, timeout=30)
        record(library.name + " version exports before runtime initialization")

    if os.name == "nt":
        from ctypes import wintypes
        version = ctypes.WinDLL("version", use_last_error=True)
        version.GetFileVersionInfoSizeW.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(wintypes.DWORD)]
        version.GetFileVersionInfoSizeW.restype = wintypes.DWORD
        version.GetFileVersionInfoW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
        version.VerQueryValueW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR,
                                          ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(wintypes.UINT)]
        ide = products / "xlang_ide.exe"
        for path in [cli, *libraries, *([ide] if ide.is_file() else [])]:
            ignored = wintypes.DWORD()
            size = version.GetFileVersionInfoSizeW(str(path), ctypes.byref(ignored))
            if not size:
                raise ctypes.WinError(ctypes.get_last_error())
            data = ctypes.create_string_buffer(size)
            if not version.GetFileVersionInfoW(str(path), 0, size, data):
                raise ctypes.WinError(ctypes.get_last_error())
            pointer, count = ctypes.c_void_p(), wintypes.UINT()
            if not version.VerQueryValueW(data, "\\", ctypes.byref(pointer), ctypes.byref(count)) or count.value < 24:
                raise RuntimeError("missing VERSIONINFO")
            fields = ctypes.cast(pointer, ctypes.POINTER(wintypes.DWORD))
            ms, ls = (major << 16) | minor, patch << 16
            if tuple(fields[i] for i in (0, 2, 3, 4, 5)) != (0xFEEF04BD, ms, ls, ms, ls):
                raise RuntimeError("PE numeric version mismatch: " + str(path))
            for name in ["FileVersion", "ProductVersion"]:
                key = "\\StringFileInfo\\040904b0\\" + name
                if not version.VerQueryValueW(data, key, ctypes.byref(pointer), ctypes.byref(count)):
                    raise RuntimeError("missing version text: " + key)
                if ctypes.wstring_at(pointer) != version_text:
                    raise RuntimeError("PE text version mismatch: " + str(path))
            record(path.name + " Windows numeric/text VERSIONINFO")

    sdk = directory / "include"
    sdk.mkdir(exist_ok=True)
    for path in sdk_source.glob("*.h"):
        shutil.copyfile(path, sdk / path.name)
    source = directory / "version_host.c"
    source.write_text('''#include "xl.h"
#include "xlc.h"
#include <string.h>
_Static_assert(XL_VERSION_CODE == PRODUCT_CODE, "product code");
_Static_assert(XL_API_VERSION == 1u && XLC_API_VERSION == 1u, "embedding ABI");
int main(void) {
    return xlGetVersionCode()!=XL_VERSION_CODE || strcmp(xlGetVersionString(),XL_VERSION_TEXT);
}
'''.replace("PRODUCT_CODE", version_hex + "u"), encoding="utf-8")
    for library in libraries:
        binary = directory / (library.stem + ("_sdk.exe" if os.name == "nt" else "_sdk"))
        link = library
        command = [args.cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(sdk),
                   str(source), str(link), "-o", str(binary)]
        if os.name != "nt":
            command += ["-Wl,-rpath," + str(products)]
        subprocess.run(command, check=True, cwd=directory, timeout=60)
        subprocess.run([str(binary)], check=True, env=env, timeout=30)
        record(library.name + " source-free C SDK compile/link/run")

    for path in [cli, *libraries]:
        report["products"].append({"path": str(path), "bytes": path.stat().st_size,
                                   "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    report["complete"] = True
    (directory / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
