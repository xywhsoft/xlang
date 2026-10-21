#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-barcode-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -Irelease/extlibs/barcode/include -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/barcode/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/barcode/barcode_native.c \
    release/extlibs/barcode/test/test_barcode.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_barcode"
timeout 90s "$work/test_barcode"

xl=./release/xl
if [[ ! -x "$xl" ]]; then
    printf 'missing Linux xlang runner: %s\n' "$xl" >&2
    exit 1
fi
timeout 90s "$xl" run test/cases/extlib_barcode.xl
timeout 90s "$xl" run test/cases/extlib_barcode_boundaries.xl
timeout 90s "$xl" run release/extlibs/barcode/test/recovery.xl
timeout 90s "$xl" run release/extlibs/barcode/test/allocations.xl

"$cc" "${flags[@]}" release/extlibs/barcode/barcode_native.c \
    release/extlibs/barcode/examples/barcode.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/barcode_example"
timeout 90s "$work/barcode_example" > "$work/c.out"
timeout 90s "$xl" run release/extlibs/barcode/examples/barcode.c > "$work/direct_c.out"
timeout 90s "$xl" run release/examples/extlibs/barcode.xl > "$work/xlang.out"
diff -u "$work/c.out" "$work/direct_c.out"
diff -u "$work/c.out" "$work/xlang.out"
printf 'barcode Linux C ASan/UBSan, XLang JIT and bilingual gate: PASS\n'
