#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-ansi-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cd "$root"

cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -Irelease/extlibs/ansi -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/ansi/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/ansi/ansi_native.c \
    release/extlibs/ansi/test/test_ansi.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_ansi"
timeout 90s "$work/test_ansi"

"$cc" "${flags[@]}" release/extlibs/ansi/ansi_native.c \
    release/extlibs/ansi/examples/ansi_c.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/ansi_example"
timeout 30s "$work/ansi_example" > "$work/c.txt"
timeout 90s ./release/xl run test/cases/extlib_ansi.xl
timeout 90s ./release/xl run release/extlibs/ansi/test/boundaries.xl
timeout 30s ./release/xl run release/extlibs/ansi/examples/ansi_c.c > "$work/direct.txt"
timeout 90s ./release/xl run release/extlibs/ansi/examples/ansi.xl > "$work/xl.txt"
diff -u "$work/c.txt" "$work/direct.txt"
diff -u "$work/c.txt" "$work/xl.txt"
printf 'ansi Linux C ASan/UBSan and XLang parity gate: PASS\n'
