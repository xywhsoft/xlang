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

timeout 180s "$xl" build release/extlibs/stb_truetype/test/boundaries.xl \
    -o "$work/stb_truetype_boundaries"
timeout 90s "$work/stb_truetype_boundaries"
timeout 180s "$xl" build release/examples/extlibs/stb_truetype.xl \
    -o "$work/stb_truetype_xlang"
timeout 90s "$work/stb_truetype_xlang" \
    "$root/release/extlibs/stb_truetype/test/assets/DejaVuSans-test.ttf" \
    > "$work/xlang_aot.out"
diff -u "$work/c.out" "$work/xlang_aot.out"
timeout 180s "$xl" build release/examples/extlibs/stb_truetype_c.c \
    -o "$work/stb_truetype_c"
timeout 90s "$work/stb_truetype_c" \
    "$root/release/extlibs/stb_truetype/test/assets/DejaVuSans-test.ttf" \
    > "$work/c_aot.out"
diff -u "$work/c.out" "$work/c_aot.out"
printf 'stb_truetype Linux C ASan/UBSan, XLang JIT/AOT and bilingual gate: PASS\n'

access_failures=0
for type in font bitmap; do
    diagnostic="$work/private_${type}.txt"
    if timeout 90s "$xl" check \
        "release/extlibs/stb_truetype/test/private_${type}_constructor_negative.xl" \
        > "$diagnostic" 2>&1; then
        printf 'stb_truetype: private %s constructor was accepted\n' "$type" >&2
        cat "$diagnostic" >&2
        access_failures=$((access_failures + 1))
        continue
    fi
    if ! grep -Fiq "$type" "$diagnostic" ||
       ! grep -Fq 'access' "$diagnostic"; then
        cat "$diagnostic" >&2
        printf 'stb_truetype: expected private %s access diagnostic\n' "$type" >&2
        access_failures=$((access_failures + 1))
    fi
done
if (( access_failures != 0 )); then
    exit 1
fi
printf 'stb_truetype Linux private constructor gate: PASS\n'
