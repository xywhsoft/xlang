#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-geohash-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -Irelease/extlibs/geohash/include -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt \
    -Irelease/extlibs/geohash/include -DXRT_MODULE_STRING \
    -DXRT_MODULE_STRING_FORMAT -DXRT_MODULE_NUMBER_FLOAT \
    release/extlibs/geohash/geohash_native.c

"$cc" "${flags[@]}" -c release/extlibs/geohash/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/geohash/geohash_native.c \
    release/extlibs/geohash/test/test_geohash.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_geohash"
timeout 90s "$work/test_geohash"

"$cc" "${flags[@]}" release/extlibs/geohash/geohash_native.c \
    release/extlibs/geohash/examples/geohash_c.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/geohash_example"
timeout 30s "$work/geohash_example" > "$work/c.txt"
diff -u release/extlibs/geohash/examples/geohash.expected.txt "$work/c.txt"
timeout 30s ./release/xl run release/extlibs/geohash/examples/geohash_c.c > "$work/c_direct.txt"
diff -u "$work/c.txt" "$work/c_direct.txt"
timeout 90s ./release/xl run test/cases/extlib_geohash.xl
timeout 90s ./release/xl run release/extlibs/geohash/test/boundaries.xl
timeout 90s ./release/xl run release/extlibs/geohash/test/allocations.xl
timeout 90s ./release/xl run release/examples/extlibs/geohash.xl > "$work/xl.txt"
diff -u release/extlibs/geohash/examples/geohash.expected.txt "$work/xl.txt"
printf 'geohash Linux C ASan/UBSan and XLang parity gate: PASS\n'
