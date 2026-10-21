#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-roaring-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/roaring/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/roaring/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/roaring/roaring_native.c \
    release/extlibs/roaring/test/test_roaring.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_roaring"
timeout 90s "$work/test_roaring"
"$cc" "${flags[@]}" release/extlibs/roaring/roaring_native.c \
    release/extlibs/roaring/examples/roaring.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/roaring_example"
timeout 30s "$work/roaring_example" > "$work/c_example.txt"
timeout 90s ./release/xl run test/cases/extlib_roaring.xl
timeout 90s ./release/xl run test/cases/extlib_roaring_boundaries.xl
timeout 90s ./release/xl run release/extlibs/roaring/test/allocations.xl
if timeout 90s ./release/xl check \
    release/extlibs/roaring/test/private_native_negative.xl \
    > "$work/private_native.txt" 2>&1; then
    printf 'roaring: raw native add must be private\n' >&2
    exit 1
fi
if ! grep -Fq 'addNative' "$work/private_native.txt" ||
   ! grep -Fq 'access' "$work/private_native.txt"; then
    cat "$work/private_native.txt" >&2
    printf 'roaring: expected private-access diagnostic\n' >&2
    exit 1
fi
timeout 90s ./release/xl run release/examples/extlibs/roaring.xl \
    > "$work/xl_example.txt"
diff -u "$work/c_example.txt" "$work/xl_example.txt"
timeout 90s ./release/xl run release/extlibs/roaring/examples/roaring.c \
    > "$work/direct_c_example.txt"
diff -u "$work/c_example.txt" "$work/direct_c_example.txt"
printf 'roaring Linux C ASan/UBSan and XLang parity gate: PASS\n'
