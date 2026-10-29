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
timeout 120s ./release/xl build release/extlibs/qr/test/aot_core.xl \
    -o "$work/qr_core_aot"
timeout 45s "$work/qr_core_aot"
timeout 120s ./release/xl build release/extlibs/qr/test/boundaries.xl \
    -o "$work/qr_boundaries_aot"
timeout 45s "$work/qr_boundaries_aot"
timeout 120s ./release/xl build test/cases/extlib_qr.xl \
    -o "$work/qr_central_aot"
timeout 45s "$work/qr_central_aot"
timeout 120s ./release/xl build release/examples/extlibs/qr.xl \
    -o "$work/qr_xl_example_aot"
timeout 45s "$work/qr_xl_example_aot" > "$work/xl_aot.txt"
diff -u "$work/c.txt" "$work/xl_aot.txt"
timeout 120s ./release/xl build release/examples/extlibs/qr_c.c \
    -o "$work/qr_c_example_aot"
timeout 45s "$work/qr_c_example_aot" > "$work/c_aot.txt"
diff -u "$work/c.txt" "$work/c_aot.txt"
printf 'qr Linux C ASan/UBSan and XLang JIT/AOT parity gate: PASS\n'
