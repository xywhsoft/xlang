#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-statistics-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)

"$cc" -std=c11 -Wall -Wextra -Werror -DXRT_MODULE_VALUE_CONTAINER \
    -Ilib/xrt -fsyntax-only release/extlibs/statistics/statistics_native.c
"$cc" "${flags[@]}" -fsyntax-only release/extlibs/statistics/test/abi_contract.c

"$cc" "${flags[@]}" release/extlibs/statistics/test/test_statistics.c \
    release/extlibs/statistics/statistics_native.c \
    release/extlibs/statistics/test/xrt_runtime.c \
    -lm -ldl -o "$work/test_statistics"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    timeout 120s "$work/test_statistics"

"$cc" "${flags[@]}" release/extlibs/statistics/test/size_guard.c \
    release/extlibs/statistics/test/xrt_runtime.c \
    -lm -ldl -o "$work/size_guard"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    timeout 30s "$work/size_guard"

"$cc" "${flags[@]}" release/extlibs/statistics/examples/statistics_c.c \
    release/extlibs/statistics/test/xrt_runtime.c \
    -lm -ldl -o "$work/example"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    timeout 30s "$work/example" > "$work/example_c.txt"
diff -u release/extlibs/statistics/examples/basic.expected.txt "$work/example_c.txt"

"$cc" "${flags[@]}" release/extlibs/statistics/examples/trend_c.c \
    release/extlibs/statistics/test/xrt_runtime.c \
    -lm -ldl -o "$work/trend"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    timeout 30s "$work/trend" > "$work/trend_c.txt"
diff -u release/extlibs/statistics/examples/trend.expected.txt "$work/trend_c.txt"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'statistics: Linux xl runner is missing: %s\n' "$xl" >&2
    exit 1
fi
timeout 90s "$xl" run release/extlibs/statistics/test/boundaries.xl
timeout 90s "$xl" run release/extlibs/statistics/test/allocations.xl
timeout 90s "$xl" run release/extlibs/statistics/examples/basic.xl > "$work/example_xl.txt"
diff -u release/extlibs/statistics/examples/basic.expected.txt "$work/example_xl.txt"
diff -u "$work/example_c.txt" "$work/example_xl.txt"
timeout 90s "$xl" run release/extlibs/statistics/examples/statistics_c.c > "$work/example_direct_c.txt"
diff -u "$work/example_c.txt" "$work/example_direct_c.txt"
timeout 90s "$xl" run release/extlibs/statistics/examples/trend.xl > "$work/trend_xl.txt"
diff -u release/extlibs/statistics/examples/trend.expected.txt "$work/trend_xl.txt"
diff -u "$work/trend_c.txt" "$work/trend_xl.txt"
timeout 90s "$xl" run release/extlibs/statistics/examples/trend_c.c > "$work/trend_direct_c.txt"
diff -u "$work/trend_c.txt" "$work/trend_direct_c.txt"

"$cc" "${flags[@]}" release/extlibs/statistics/examples/distribution_c.c \
    release/extlibs/statistics/test/xrt_runtime.c \
    -lm -ldl -o "$work/distribution"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    timeout 30s "$work/distribution" > "$work/distribution_c.txt"
diff -u release/extlibs/statistics/examples/distribution.expected.txt "$work/distribution_c.txt"
timeout 90s "$xl" run release/extlibs/statistics/examples/distribution.xl > "$work/distribution_xl.txt"
diff -u "$work/distribution_c.txt" "$work/distribution_xl.txt"
timeout 90s "$xl" run release/extlibs/statistics/examples/distribution_c.c > "$work/distribution_direct_c.txt"
diff -u "$work/distribution_c.txt" "$work/distribution_direct_c.txt"
printf 'statistics Linux C ASan/UBSan and XLang parity gate: PASS\n'
