#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-hcl-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -fsyntax-only release/extlibs/hcl/test/abi_contract.c
"$cc" "${flags[@]}" -DXRT_IMPLEMENTATION -c \
    release/extlibs/hcl/test/test_hcl.c -o "$work/test_hcl.o"
"$cc" "${flags[@]}" -c release/extlibs/hcl/hcl_native.c \
    -o "$work/hcl_native.o"
"$cc" "${flags[@]}" -no-pie "$work/test_hcl.o" "$work/hcl_native.o" \
    -lm -ldl -o "$work/test_hcl"
timeout 90s "$work/test_hcl"

"$cc" "${flags[@]}" -DXRT_IMPLEMENTATION -c \
    release/examples/extlibs/hcl_c.c -o "$work/hcl_example.o"
"$cc" "${flags[@]}" -no-pie "$work/hcl_example.o" \
    -lm -ldl -o "$work/hcl_example"
timeout 90s "$work/hcl_example" > "$work/hcl_c.txt"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'hcl Linux gate: release/xl is required\n' >&2
    exit 1
fi
timeout 90s "$xl" run test/cases/extlib_hcl.xl
timeout 90s "$xl" run release/extlibs/hcl/test/boundaries.xl
timeout 90s "$xl" run release/extlibs/hcl/test/allocations.xl
timeout 90s "$xl" run release/examples/extlibs/hcl.xl > "$work/hcl_xl.txt"
timeout 90s "$xl" run release/examples/extlibs/hcl_c.c > "$work/hcl_direct_c.txt"
diff -u "$work/hcl_c.txt" "$work/hcl_xl.txt"
diff -u "$work/hcl_c.txt" "$work/hcl_direct_c.txt"
printf 'hcl Linux C/XL ASan/UBSan gate: PASS\n'
