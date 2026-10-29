#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-srt-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/srt/include -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -fsyntax-only release/extlibs/srt/test/abi_contract.c
"$cc" "${flags[@]}" -c release/extlibs/srt/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/srt/srt_native.c \
    release/extlibs/srt/test/test_srt.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_srt"
timeout 90s "$work/test_srt"
"$cc" "${flags[@]}" release/examples/extlibs/srt_c.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/srt_example"
timeout 30s "$work/srt_example" > "$work/c_example.txt"
timeout 90s ./release/xl run test/cases/extlib_srt.xl
timeout 90s ./release/xl run release/extlibs/srt/test/boundaries.xl
timeout 90s ./release/xl run release/extlibs/srt/test/allocations.xl
timeout 90s ./release/xl run release/examples/extlibs/srt_pair.xl \
    > "$work/xl_example.txt"
diff -u "$work/c_example.txt" "$work/xl_example.txt"
printf 'srt Linux C ASan/UBSan and XLang parity gate: PASS\n'
