#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-imagehash-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/imagehash
       -Irelease/extlibs/imagehash/include
       -ffunction-sections -fdata-sections
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/imagehash/imagehash_native.c \
    -o "$work/imagehash.o"
"$cc" "${flags[@]}" -c release/extlibs/stb_image/stb_image_native.c \
    -o "$work/stb_image.o"
"$cc" "${flags[@]}" -c release/extlibs/stb_image_resize2/stb_image_resize2_native.c \
    -o "$work/stb_image_resize2.o"
for name in test_error_threads test_imagehash; do
    definitions=(-DXRT_IMPLEMENTATION)
    if [[ "$name" == test_imagehash ]]; then definitions+=(-DXRT_MODULE_ALL); fi
    "$cc" "${flags[@]}" "${definitions[@]}" -c \
        "release/extlibs/imagehash/test/$name.c" -o "$work/$name.o"
    "$cc" -fsanitize=address,undefined -no-pie -Wl,--gc-sections \
        "$work/$name.o" "$work/imagehash.o" "$work/stb_image.o" \
        "$work/stb_image_resize2.o" -pthread -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name"
done

xl=./release/xl
if [[ ! -x "$xl" ]]; then
    printf 'missing Linux xlang runner: %s\n' "$xl" >&2
    exit 1
fi
timeout 90s "$xl" run test/cases/extlib_imagehash.xl
timeout 90s "$xl" run release/extlibs/imagehash/test/allocations.xl

"$cc" "${flags[@]}" -DXRT_IMPLEMENTATION -DXRT_MODULE_ALL -c \
    release/extlibs/imagehash/examples/imagehash.c -o "$work/example.o"
"$cc" -fsanitize=address,undefined -no-pie -Wl,--gc-sections \
    "$work/example.o" "$work/imagehash.o" "$work/stb_image.o" \
    "$work/stb_image_resize2.o" -pthread -lm -ldl -o "$work/example"
timeout 90s "$work/example" > "$work/c.out"
timeout 90s "$xl" run release/examples/extlibs/imagehash.xl > "$work/xlang.out"
diff -u "$work/c.out" "$work/xlang.out"
diff -u release/extlibs/imagehash/examples/imagehash.expected.txt "$work/c.out"
timeout 90s "$xl" run release/extlibs/imagehash/examples/imagehash.c > "$work/direct.out"
diff -u "$work/c.out" "$work/direct.out"
timeout 90s "$xl" build release/extlibs/imagehash/test/aot_core.xl -o "$work/imagehash_core"
timeout 30s "$work/imagehash_core"
timeout 90s "$xl" build release/extlibs/imagehash/test/allocations.xl -o "$work/imagehash_allocations"
timeout 30s "$work/imagehash_allocations"
timeout 90s "$xl" build release/examples/extlibs/imagehash.xl -o "$work/imagehash_xlang"
timeout 30s "$work/imagehash_xlang" > "$work/xlang_aot.out"
diff -u release/extlibs/imagehash/examples/imagehash.expected.txt "$work/xlang_aot.out"
timeout 90s "$xl" build release/extlibs/imagehash/examples/imagehash.c -o "$work/imagehash_c"
timeout 30s "$work/imagehash_c" > "$work/c_aot.out"
diff -u release/extlibs/imagehash/examples/imagehash.expected.txt "$work/c_aot.out"
printf 'imagehash Linux C ASan/UBSan, XLang JIT and bilingual AOT gate: PASS\n'
