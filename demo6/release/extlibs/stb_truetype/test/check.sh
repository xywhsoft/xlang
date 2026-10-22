#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-stb-truetype-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/stb_truetype/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/stb_truetype/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/stb_truetype/stb_truetype_native.c \
    release/extlibs/stb_truetype/test/test_stb_truetype.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/test_stb_truetype"
timeout 90s "$work/test_stb_truetype"

xl=./release/xl
if [[ ! -x "$xl" ]]; then
    printf 'missing Linux xlang runner: %s\n' "$xl" >&2
    exit 1
fi
timeout 90s "$xl" run test/cases/extlib_stb_truetype.xl
timeout 90s "$xl" run release/extlibs/stb_truetype/test/boundaries.xl
timeout 90s "$xl" run release/extlibs/stb_truetype/test/allocations.xl
if timeout 90s "$xl" check \
    release/extlibs/stb_truetype/test/private_bitmap_negative.xl \
    > "$work/private_bitmap.txt" 2>&1; then
    printf 'stb_truetype: raw bitmap method must be private\n' >&2
    exit 1
fi
if ! grep -Fq 'bitmapNative' "$work/private_bitmap.txt" ||
   ! grep -Fq 'access' "$work/private_bitmap.txt"; then
    cat "$work/private_bitmap.txt" >&2
    printf 'stb_truetype: expected private-access diagnostic\n' >&2
    exit 1
fi
(
    cd release
    timeout 90s ./xl run examples/extlibs/stb_truetype_c.c > "$work/c.out"
    timeout 90s ./xl run examples/extlibs/stb_truetype.xl > "$work/xlang.out"
)
diff -u "$work/c.out" "$work/xlang.out"
printf 'stb_truetype Linux C ASan/UBSan, XLang JIT and bilingual gate: PASS\n'
