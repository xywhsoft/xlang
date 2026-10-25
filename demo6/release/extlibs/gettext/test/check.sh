#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-gettext-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/gettext
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" release/extlibs/gettext/test/xrt_runtime.c \
    release/extlibs/gettext/gettext_native.c \
    release/extlibs/gettext/gettext_plural.c \
    release/extlibs/gettext/test/test_gettext.c \
    -no-pie -lm -ldl -o "$work/test_gettext"
timeout 90s "$work/test_gettext" "$work"
timeout 90s ./release/xl run test/cases/extlib_gettext.xl
timeout 90s ./release/xl run release/extlibs/gettext/test/duplicates.xl
timeout 90s ./release/xl run release/extlibs/gettext/test/boundaries.xl
timeout 90s ./release/xl run release/extlibs/gettext/test/allocations.xl
timeout 90s ./release/xl run release/extlibs/gettext/test/input_limits.xl

"$cc" "${flags[@]}" release/extlibs/gettext/test/xrt_runtime.c \
    release/extlibs/gettext/examples/gettext_c.c \
    -no-pie -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/example_c.txt"
timeout 90s ./release/xl run release/extlibs/gettext/examples/gettext_c.c > "$work/example_direct_c.txt"
diff -u "$work/example_c.txt" "$work/example_direct_c.txt"
( cd release && timeout 90s ./xl run extlibs/gettext/examples/gettext.xl ) > "$work/example_xl.txt"
diff -u "$work/example_c.txt" "$work/example_xl.txt"
timeout 120s ./release/xl build release/extlibs/gettext/test/boundaries.xl \
    -o "$work/gettext_boundaries"
timeout 90s "$work/gettext_boundaries"
( cd release && timeout 120s ./xl build extlibs/gettext/examples/gettext.xl \
    -o "$work/gettext_example" )
( cd release && timeout 90s "$work/gettext_example" ) > "$work/example_aot.txt"
diff -u "$work/example_c.txt" "$work/example_aot.txt"
printf 'gettext Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
