#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-epub-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/epub/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -fsyntax-only -DXRT_MODULE_BUFFER \
    -DXRT_MODULE_FILE_WHOLE -DXRT_MODULE_TIME_TEXT \
    -DXRT_MODULE_UNICODE -DXRT_MODULE_VALUE_CONTAINER \
    release/extlibs/epub/epub_native.c release/extlibs/epub/epub_zip.c
"$cc" "${flags[@]}" -fsyntax-only release/extlibs/epub/test/abi_contract.c
"$cc" "${flags[@]}" -c release/extlibs/epub/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/epub/test/test_epub.c \
    release/extlibs/epub/epub_native.c release/extlibs/epub/epub_zip.c \
    "$work/xrt_runtime.o" -Wl,--wrap=clock_gettime -lm -ldl -o "$work/test_epub"
timeout 90s "$work/test_epub" "$work/book.epub"
"$cc" "${flags[@]}" release/extlibs/epub/examples/basic.c \
    release/extlibs/epub/epub_native.c release/extlibs/epub/epub_zip.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/epub_example"
timeout 30s "$work/epub_example" "$work/c.epub" > "$work/example_c.txt"

if [[ ! -x release/xl ]]; then
    printf 'missing Linux release/xl for EPUB XLang gate\n' >&2
    exit 1
fi
timeout 30s release/xl run release/extlibs/epub/examples/basic.c -- \
    "$work/c_direct.epub" > "$work/example_c_direct.txt"
diff -u "$work/example_c.txt" "$work/example_c_direct.txt"
cmp "$work/c.epub" "$work/c_direct.epub"
timeout 90s release/xl run test/cases/extlib_epub.xl
timeout 90s release/xl run test/cases/extlib_epub_boundaries.xl
timeout 90s release/xl run release/extlibs/epub/test/ffi_boundaries.xl
timeout 90s release/xl run release/examples/extlibs/epub.xl -- "$work/xlang.epub" > "$work/example_xl.txt"
diff -u "$work/example_c.txt" "$work/example_xl.txt"

if ! command -v python3 >/dev/null; then
    printf 'python3 is required for the EPUB ZIP/XML gate\n' >&2
    exit 1
fi
python3 -I release/extlibs/epub/test/check_external.py "$work/c.epub" "$work/xlang.epub"
printf 'epub Linux C ASan/UBSan, XLang and ZIP/XML gate: PASS\n'
