#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-units-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -Irelease/extlibs/units/include -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -fsyntax-only release/extlibs/units/test/abi_contract.c
"$cc" "${flags[@]}" -c release/extlibs/units/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" -c release/extlibs/units/units_native.c \
    -o "$work/units_native.o"
objects=("$work/units_native.o" "$work/xrt_runtime.o")
"$cc" "${flags[@]}" release/extlibs/units/test/test_units.c \
    "${objects[@]}" -lm -ldl -o "$work/test_units"
"$cc" "${flags[@]}" release/extlibs/units/test/test_threads.c \
    "${objects[@]}" -lm -ldl -o "$work/test_threads"
"$cc" "${flags[@]}" release/extlibs/units/examples/units_c.c \
    "${objects[@]}" -lm -ldl -o "$work/example_units"
timeout 90s "$work/test_units"
timeout 90s "$work/test_threads"
"$work/example_units" > "$work/c.txt"
./release/xl run release/extlibs/units/examples/units_c.c > "$work/c_direct.txt"
diff -u "$work/c.txt" "$work/c_direct.txt"

./release/xl run test/cases/extlib_units.xl
./release/xl run release/extlibs/units/test/boundaries.xl
./release/xl run release/extlibs/units/test/allocations.xl
./release/xl run release/examples/extlibs/units.xl > "$work/xl.txt"
diff -u "$work/c.txt" "$work/xl.txt"
printf 'units Linux C ASan/UBSan, threads and XLang gate: PASS\n'
