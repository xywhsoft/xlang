#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-diff-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cd "$root"

cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -Irelease/extlibs/diff -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -DXRT_MODULE_ALL -DXRT_IMPLEMENTATION \
    -x c -include xrt.h -c /dev/null -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/diff/diff_native.c \
    release/extlibs/diff/test/test_diff.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_diff"
timeout 90s "$work/test_diff"
"$cc" "${flags[@]}" release/extlibs/diff/diff_native.c \
    -DNDEBUG release/extlibs/diff/test/test_unterminated.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_unterminated"
timeout 90s "$work/test_unterminated"
"$cc" "${flags[@]}" release/extlibs/diff/diff_native.c \
    release/extlibs/diff/test/test_threads.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_threads"
timeout 90s "$work/test_threads"
"$cc" "${flags[@]}" release/extlibs/diff/diff_native.c \
    release/extlibs/diff/examples/diff_c.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/diff_example"
timeout 30s "$work/diff_example" > "$work/c_example.txt"
diff -u release/extlibs/diff/test/diff.expected.txt "$work/c_example.txt"
timeout 30s ./release/xl run release/extlibs/diff/examples/diff_c.c \
    > "$work/c_direct.txt"
diff -u "$work/c_example.txt" "$work/c_direct.txt"
timeout 90s ./release/xl run test/cases/extlib_diff.xl
timeout 90s ./release/xl run release/extlibs/diff/test/boundaries.xl
timeout 90s ./release/xl run release/examples/extlibs/diff.xl \
    > "$work/xl_example.txt"
diff -u "$work/c_example.txt" "$work/xl_example.txt"
timeout 120s ./release/xl build release/extlibs/diff/test/boundaries.xl \
    -o "$work/diff_boundaries_aot"
timeout 45s "$work/diff_boundaries_aot" > "$work/boundaries_aot.txt"
grep -Fxq 'diff boundaries: PASS' "$work/boundaries_aot.txt"
timeout 120s ./release/xl build release/examples/extlibs/diff.xl \
    -o "$work/diff_example_xl_aot"
timeout 45s "$work/diff_example_xl_aot" > "$work/xl_example_aot.txt"
diff -u "$work/c_example.txt" "$work/xl_example_aot.txt"
timeout 120s ./release/xl build release/extlibs/diff/examples/diff_c.c \
    -o "$work/diff_example_c_aot"
timeout 45s "$work/diff_example_c_aot" > "$work/c_example_aot.txt"
diff -u "$work/c_example.txt" "$work/c_example_aot.txt"
printf 'diff Linux C ASan/UBSan and XLang parity gate: PASS\n'
