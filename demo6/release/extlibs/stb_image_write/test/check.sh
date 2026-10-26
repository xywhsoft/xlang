#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-stb-image-write-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/stb_image_write/include
       -Irelease/extlibs/stb_image/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/stb_image_write/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/stb_image_write/stb_image_write_native.c \
    release/extlibs/stb_image/stb_image_native.c \
    release/extlibs/stb_image_write/test/test_stb_image_write.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/test_stb_image_write"
timeout 90s "$work/test_stb_image_write"
timeout 90s ./release/xl run test/cases/extlib_stb_image_write.xl
timeout 90s ./release/xl run release/extlibs/stb_image_write/test/boundaries.xl
timeout 90s ./release/xl run release/extlibs/stb_image_write/test/allocations.xl
timeout 90s ./release/xl run release/extlibs/stb_image_write/test/output_limit.xl
mkdir "$work/c" "$work/xl"
timeout 90s ./release/xl run release/examples/extlibs/stb_image_write_c.c -- "$work/c" \
    > "$work/c_example.txt"
timeout 90s ./release/xl run release/examples/extlibs/stb_image_write.xl -- "$work/xl" \
    > "$work/xl_example.txt"
diff -u "$work/c_example.txt" "$work/xl_example.txt"
for name in tiny.png tiny.jpg tiny.bmp tiny.tga tiny.hdr; do
    cmp "$work/c/$name" "$work/xl/$name"
done

asset=release/examples/extlibs/assets/tiny.png
timeout 90s ./release/xl build test/cases/extlib_stb_image_write.xl -o "$work/image_write_core"
timeout 30s "$work/image_write_core"
mkdir "$work/xl_aot" "$work/c_aot"
timeout 90s ./release/xl build release/examples/extlibs/stb_image_write.xl -o "$work/image_write_xl"
timeout 30s "$work/image_write_xl" "$work/xl_aot" "$asset" > "$work/xl_aot.txt"
diff -u "$work/xl_example.txt" "$work/xl_aot.txt"
timeout 90s ./release/xl build release/examples/extlibs/stb_image_write_c.c -o "$work/image_write_c"
timeout 30s "$work/image_write_c" "$work/c_aot" "$asset" > "$work/c_aot.txt"
diff -u "$work/c_example.txt" "$work/c_aot.txt"
for name in tiny.png tiny.jpg tiny.bmp tiny.tga tiny.hdr; do
    cmp "$work/c/$name" "$work/xl_aot/$name"
    cmp "$work/c/$name" "$work/c_aot/$name"
done
printf 'stb_image_write Linux C ASan/UBSan and five-format bilingual AOT gate: PASS\n'
