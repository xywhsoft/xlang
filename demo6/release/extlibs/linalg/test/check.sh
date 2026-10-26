#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-linalg-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/linalg
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)

"$cc" "${flags[@]}" release/extlibs/linalg/test/xrt_runtime.c \
    release/extlibs/linalg/linalg_native.c \
    release/extlibs/linalg/test/test_linalg.c \
    -no-pie -lm -ldl -o "$work/test_linalg"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    timeout 120s "$work/test_linalg"
"$cc" "${flags[@]}" release/extlibs/linalg/test/xrt_runtime.c \
    release/extlibs/linalg/linalg_native.c \
    release/examples/extlibs/linalg_c.c \
    -no-pie -lm -ldl -o "$work/linalg_example"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    timeout 30s "$work/linalg_example" > "$work/c_example.txt"
diff -u release/extlibs/linalg/test/linalg.expected.txt "$work/c_example.txt"
timeout 90s ./release/xl run release/extlibs/linalg/test/boundaries.xl
timeout 90s ./release/xl run release/extlibs/linalg/test/allocations.xl
timeout 90s ./release/xl run test/cases/extlib_linalg.xl
timeout 90s ./release/xl run release/examples/extlibs/linalg.xl \
    > "$work/xl_example.txt"
diff -u "$work/c_example.txt" "$work/xl_example.txt"
timeout 90s ./release/xl run release/examples/extlibs/linalg_c.c \
    > "$work/c_direct.txt"
diff -u "$work/c_example.txt" "$work/c_direct.txt"
timeout 90s ./release/xl build release/extlibs/linalg/test/aot_core.xl -o "$work/linalg_core"
timeout 30s "$work/linalg_core"
timeout 90s ./release/xl build release/examples/extlibs/linalg.xl -o "$work/linalg_xl_aot"
timeout 30s "$work/linalg_xl_aot" > "$work/xl_aot.txt"
diff -u "$work/c_example.txt" "$work/xl_aot.txt"
timeout 90s ./release/xl build release/examples/extlibs/linalg_c.c -o "$work/linalg_c_aot"
timeout 30s "$work/linalg_c_aot" > "$work/c_aot.txt"
diff -u "$work/c_example.txt" "$work/c_aot.txt"
printf 'linalg Linux C ASan/UBSan, XLang parity and bilingual AOT gate: PASS\n'
