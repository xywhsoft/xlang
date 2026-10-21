#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-rtree-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -fsyntax-only release/extlibs/rtree/test/abi_contract.c
"$cc" "${flags[@]}" -c release/extlibs/rtree/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
modes=(native)
if printf '%s\n' '_Static_assert(sizeof(long double) == sizeof(double), "narrow");' |
    "$cc" -std=c11 -mlong-double-64 -x c -fsyntax-only - 2>/dev/null; then
    modes+=(narrow)
fi
for mode in "${modes[@]}"; do
    extra=()
    if [[ "$mode" == narrow ]]; then extra=(-mlong-double-64); fi
    "$cc" "${flags[@]}" "${extra[@]}" -c release/extlibs/rtree/rtree_native.c \
        -o "$work/rtree_$mode.o"
    "$cc" "${flags[@]}" release/extlibs/rtree/test/test_rtree.c \
        "$work/rtree_$mode.o" "$work/xrt_runtime.o" \
        -lm -ldl -o "$work/test_$mode"
    timeout 90s "$work/test_$mode"
    "$cc" "${flags[@]}" "${extra[@]}" release/extlibs/rtree/test/split_seeds.c \
        "$work/xrt_runtime.o" -lm -ldl -o "$work/split_$mode"
    timeout 90s "$work/split_$mode"
done

"$cc" "${flags[@]}" -fsyntax-only release/examples/extlibs/rtree_c.c
"$cc" "${flags[@]}" release/examples/extlibs/rtree_c.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/rtree_example"
timeout 90s "$work/rtree_example" > "$work/rtree_c.txt"
diff -u release/extlibs/rtree/test/rtree.expected.txt "$work/rtree_c.txt"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'rtree Linux gate: release/xl is required\n' >&2
    exit 1
fi
timeout 90s "$xl" run test/cases/extlib_rtree.xl
timeout 90s "$xl" run release/extlibs/rtree/test/boundaries.xl
timeout 90s "$xl" run release/extlibs/rtree/test/allocations.xl
if timeout 90s "$xl" check release/extlibs/rtree/test/private_native_negative.xl \
    > "$work/private_native.txt" 2>&1; then
    printf 'rtree: raw native query must be private\n' >&2
    exit 1
fi
if ! grep -Fq 'searchNative' "$work/private_native.txt" ||
   ! grep -Fq 'access' "$work/private_native.txt"; then
    cat "$work/private_native.txt" >&2
    printf 'rtree: expected private-access diagnostic\n' >&2
    exit 1
fi
timeout 90s "$xl" run release/examples/extlibs/rtree.xl > "$work/rtree_xl.txt"
diff -u "$work/rtree_c.txt" "$work/rtree_xl.txt"
timeout 90s "$xl" run release/examples/extlibs/rtree_c.c \
    > "$work/rtree_direct.txt"
diff -u "$work/rtree_c.txt" "$work/rtree_direct.txt"
printf 'rtree Linux C/XL %s ASan/UBSan gate: PASS\n' "${modes[*]}"
