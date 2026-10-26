#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-zip-XXXXXX")
trap 'rm -rf -- "$work"' EXIT

# 从包清单读取真实编译单元，避免与发布清单维护两份源码列表。
python3 - "$root/release/extlibs" <<'PY' > "$work/sources"
import json
import pathlib
import sys

packages = pathlib.Path(sys.argv[1])
seen = set()

def visit(name):
    if name in seen:
        return
    seen.add(name)
    folder = packages / name
    manifest = json.loads((folder / "xlang.package.json").read_text(encoding="utf-8"))
    for dependency in manifest.get("dependencies", []):
        visit(dependency["name"])
    for source in manifest["artifact"].get("cSources", []):
        path = folder / source
        if not path.is_file():
            raise SystemExit(f"missing ZIP C source: {path}")
        print(path)

visit("zip")
PY

flags=(-std=c11 -D_DEFAULT_SOURCE -D_FILE_OFFSET_BITS=64 -O1 -g
    -fsanitize=address,undefined -fno-omit-frame-pointer
    -Ilib/xrt -Irelease/extlibs/zip/include
    -Irelease/extlibs/_libzip/third_party/libzip/lib
    -Irelease/extlibs/_zlib/third_party/zlib)
gcc "${flags[@]}" -c release/extlibs/zip/test/xrt_runtime.c -o "$work/runtime.o"
objects=("$work/runtime.o")
index=0
while IFS= read -r source; do
    object="$work/source_$index.o"
    if [[ "$source" == "$root/release/extlibs/zip/"* ]]; then
        gcc "${flags[@]}" -Wall -Wextra -Werror -fsyntax-only "$source"
    fi
    gcc "${flags[@]}" -c "$source" -o "$object"
    objects+=("$object")
    index=$((index + 1))
done < "$work/sources"

gcc "${flags[@]}" -Wall -Wextra -Werror -c \
    release/extlibs/zip/test/test_zip.c -o "$work/test.o"
gcc "${flags[@]}" "${objects[@]}" "$work/test.o" -lm -ldl -pthread -o "$work/test_zip"
printf '[zip] C boundary\n'
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
    timeout 120s "$work/test_zip"

printf '[zip] C example\n'
gcc "${flags[@]}" -Wall -Wextra -Werror -c \
    release/examples/extlibs/zip_c.c -o "$work/example.o"
gcc "${flags[@]}" "${objects[@]}" "$work/example.o" -lm -ldl -pthread -o "$work/example_zip"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
    timeout 30s "$work/example_zip" > "$work/c.txt"
printf '[zip] Python/C ZIP interop\n'
gcc "${flags[@]}" -Wall -Wextra -Werror -c \
    release/extlibs/zip/test/interop.c -o "$work/interop.o"
gcc "${flags[@]}" "${objects[@]}" "$work/interop.o" -lm -ldl -pthread \
    -o "$work/interop_zip"
XL_ZIP_INTEROP_RUNNER="$work/interop_zip" \
    ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
    python3 release/extlibs/zip/test/interop.py
printf '[zip] XLang boundary\n'
if timeout 180s ./release/xl run release/extlibs/zip/test/boundaries.xl \
    > "$work/boundary_jit.txt" 2>&1; then
    cat "$work/boundary_jit.txt"
else
    status=$?
    cat "$work/boundary_jit.txt" >&2
    printf 'zip XLang boundary exited %s\n' "$status" >&2
    exit "$status"
fi
printf '[zip] XLang example\n'
if timeout 180s ./release/xl run release/examples/extlibs/zip.xl \
    > "$work/xl.txt" 2> "$work/xl_error.txt"; then
    :
else
    status=$?
    cat "$work/xl_error.txt" >&2
    printf 'zip XLang example exited %s\n' "$status" >&2
    exit "$status"
fi
printf '[zip] C/XLang output\n'
printf 'zip C/XL example: PASS\n' > "$work/expected.txt"
cmp "$work/expected.txt" "$work/c.txt"
cmp "$work/expected.txt" "$work/xl.txt"
printf '[zip] private native access\n'
if timeout 90s ./release/xl check \
    release/extlibs/zip/test/private_native_negative.xl \
    > "$work/private_native.txt" 2>&1; then
    printf 'zip native add must be private\n' >&2
    exit 1
fi
grep -Fq 'addNative' "$work/private_native.txt"
grep -Fq 'access' "$work/private_native.txt"
printf '[zip] independent EXE\n'
for source in \
    release/extlibs/zip/test/aot_core.xl \
    release/extlibs/zip/test/boundaries.xl \
    release/examples/extlibs/zip.xl; do
    name=$(basename "$source" .xl)
    printf '[zip] build %s\n' "$name"
    timeout 240s ./release/xl build "$source" -o "$work/$name"
    timeout 30s "$work/$name" > "$work/$name.txt"
done
test ! -s "$work/aot_core.txt"
printf 'zip boundaries: ok\n' > "$work/boundaries_expected.txt"
cmp "$work/boundaries_expected.txt" "$work/boundaries.txt"
cmp "$work/expected.txt" "$work/zip.txt"
printf '[zip] build C example\n'
timeout 240s ./release/xl build release/extlibs/zip/test/c_aot_import.xl \
    release/examples/extlibs/zip_c.c -o "$work/zip_c"
timeout 30s "$work/zip_c" > "$work/zip_c.txt"
cmp "$work/expected.txt" "$work/zip_c.txt"
printf '[zip] symlink boundary\n'
bash release/extlibs/zip/test/check_symlink.sh
printf 'zip Linux C ASan/UBSan, Python interop and bilingual AOT: PASS\n'
