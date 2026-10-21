#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-fix-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -Irelease/extlibs/fix -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/fix/test/xrt_runtime.c \
    -o "$work/runtime.o"
"$cc" "${flags[@]}" -c release/extlibs/fix/fix_native.c \
    -o "$work/fix_native.o"
"$cc" "${flags[@]}" release/extlibs/fix/test/test_fix.c \
    "$work/runtime.o" "$work/fix_native.o" -no-pie -lm -ldl \
    -o "$work/test_fix"
timeout 90s "$work/test_fix"
"$cc" "${flags[@]}" release/extlibs/fix/examples/fix_c.c \
    "$work/runtime.o" -no-pie -lm -ldl \
    -o "$work/fix_example"
timeout 30s "$work/fix_example" > "$work/example_c.txt"

if [[ ! -x release/xl ]]; then
    printf 'missing Linux release/xl for FIX XLang gate\n' >&2
    exit 1
fi
(
    cd release
    timeout 90s ./xl run ../test/cases/extlib_fix.xl
    timeout 90s ./xl run extlibs/fix/test/boundaries.xl
    timeout 90s ./xl run extlibs/fix/test/allocations.xl
    timeout 90s ./xl run extlibs/fix/examples/fix_c.c > "$work/example_xl_c.txt"
    timeout 90s ./xl run examples/extlibs/fix.xl > "$work/example_xl.txt"
)
diff -u "$work/example_c.txt" "$work/example_xl.txt"
diff -u "$work/example_c.txt" "$work/example_xl_c.txt"
python3 release/extlibs/fix/test/check_frame.py "$(cat "$work/example_c.txt")"
printf 'fix Linux C ASan/UBSan and XLang parity gate: PASS\n'
