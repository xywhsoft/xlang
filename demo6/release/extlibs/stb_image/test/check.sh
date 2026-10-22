#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-stb-image-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -DXRT_MODULE_FILE_WHOLE \
    -c release/extlibs/stb_image/stb_image_native.c -o "$work/stb_image.o"
for name in test_stb_image test_stb_image_threads; do
    "$cc" "${flags[@]}" -DXRT_IMPLEMENTATION \
        -c "release/extlibs/stb_image/test/$name.c" -o "$work/$name.o"
    "$cc" -fsanitize=address,undefined -no-pie "$work/$name.o" \
        "$work/stb_image.o" -pthread -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name"
done

xl=./release/xl
if [[ ! -x "$xl" ]]; then
    printf 'missing Linux xlang runner: %s\n' "$xl" >&2
    exit 1
fi
timeout 90s "$xl" run test/cases/extlib_stb_image.xl
timeout 90s "$xl" run release/extlibs/stb_image/test/boundaries.xl
timeout 90s "$xl" run release/extlibs/stb_image/test/allocations.xl
if timeout 45s "$xl" check \
    release/extlibs/stb_image/test/private_native_status_negative.xl \
    > "$work/private.txt" 2>&1; then
    printf 'stb_image: Native status methods remain accessible\n' >&2
    exit 1
else
    status=$?
    if [[ $status -ne 1 ]] || ! grep -q 'validNative' "$work/private.txt" ||
        ! grep -q 'floatNative' "$work/private.txt" ||
        ! grep -q 'access' "$work/private.txt"; then
        cat "$work/private.txt" >&2
        exit 1
    fi
fi
(
    cd release
    timeout 90s ./xl run examples/extlibs/stb_image_c.c > "$work/c.out"
    timeout 90s ./xl run examples/extlibs/stb_image.xl > "$work/xlang.out"
)
diff -u "$work/c.out" "$work/xlang.out"
printf 'stb_image Linux C ASan/UBSan, XLang JIT and bilingual gate: PASS\n'
