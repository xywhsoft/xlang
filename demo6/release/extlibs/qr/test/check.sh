#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-qr-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/qr/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -fsyntax-only release/extlibs/qr/test/abi_contract.c
"$cc" "${flags[@]}" -c release/extlibs/qr/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/qr/qr_native.c \
    release/extlibs/qr/test/test_qr.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_qr"
timeout 90s "$work/test_qr"

"$cc" "${flags[@]}" release/examples/extlibs/qr_c.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/qr_example"
timeout 30s "$work/qr_example" > "$work/c.txt"
timeout 90s ./release/xl run test/cases/extlib_qr.xl
timeout 90s ./release/xl run release/extlibs/qr/test/boundaries.xl
timeout 90s ./release/xl run release/examples/extlibs/qr.xl > "$work/xl.txt"
diff -u "$work/c.txt" "$work/xl.txt"
printf 'qr Linux C ASan/UBSan and XLang parity gate: PASS\n'
