#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-money-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -Irelease/extlibs/money/include -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" -std=c11 -Wall -Wextra -Werror -DXRT_MODULE_VALUE_CONTAINER \
    -DXRT_MODULE_STRING -Ilib/xrt -Irelease/extlibs/money/include \
    -fsyntax-only release/extlibs/money/money_native.c

"$cc" "${flags[@]}" -c release/extlibs/money/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/money/money_native.c \
    release/extlibs/money/test/test_money.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_money"
timeout 90s "$work/test_money"

"$cc" "${flags[@]}" release/extlibs/money/examples/money.c \
    "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/money_example"
timeout 90s "$work/money_example" > "$work/money_c.txt"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'money Linux gate: release/xl is required\n' >&2
    exit 1
fi
timeout 90s "$xl" run test/cases/extlib_money.xl
timeout 90s "$xl" run test/cases/extlib_money_boundaries.xl
timeout 90s "$xl" run release/extlibs/money/test/error_kinds.xl
timeout 90s "$xl" run release/extlibs/money/test/allocations.xl
timeout 90s "$xl" run release/examples/extlibs/money.xl > "$work/money_xl.txt"
diff -u release/extlibs/money/examples/money.expected.txt "$work/money_c.txt"
diff -u "$work/money_c.txt" "$work/money_xl.txt"

timeout 90s "$xl" build release/extlibs/money/test/aot_core.xl -o "$work/money_core"
timeout 30s "$work/money_core"
timeout 90s "$xl" build release/examples/extlibs/money.xl -o "$work/money_xl_aot"
timeout 30s "$work/money_xl_aot" > "$work/money_xl_aot.txt"
diff -u "$work/money_c.txt" "$work/money_xl_aot.txt"
timeout 90s "$xl" build release/extlibs/money/examples/money.c -o "$work/money_c_aot"
timeout 30s "$work/money_c_aot" > "$work/money_c_aot.txt"
diff -u "$work/money_c.txt" "$work/money_c_aot.txt"
printf 'money Linux C/XL ASan/UBSan gate: PASS\n'
