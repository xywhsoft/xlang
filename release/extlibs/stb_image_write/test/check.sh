#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-stb-image-write-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    if ((result == 0)); then rm -rf -- "$work"
    else printf '[stb write] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
    fi
    exit "$result"
}
trap cleanup EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/stb_image_write/include
       -Irelease/extlibs/stb_image/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
mapfile -d '' inputs < <(
    { find release/extlibs/stb_image_write release/extlibs/stb_image -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        release/include/xrt_decl.h release/include/xruntime_decl.h release/include/xl_extlib_abi.h \
        test/cases/extlib_stb_image_write.xl release/examples/extlibs/stb_image_write.xl \
        release/examples/extlibs/stb_image_write_c.c release/examples/extlibs/assets/tiny.png
    } | sort -zu
)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"

# 确定超限应在输出分配前拒绝；普通分配失败仍是 MEMORY，且立即可重试。
timeout 120s "$cc" "${flags[@]}" -DXRT_IMPLEMENTATION \
    release/extlibs/stb_image_write/test/test_bmp_budget.c -lm -ldl -o "$work/bmp_budget"
timeout 90s "$work/bmp_budget"
timeout 90s release/xl run release/extlibs/stb_image_write/test/test_bmp_budget.c
timeout 90s release/xl build release/extlibs/stb_image_write/test/test_bmp_budget.c -o "$work/bmp_budget.aot"
timeout 90s "$work/bmp_budget.aot"
for source in bmp_headers output_limit; do
    timeout 90s release/xl run "release/extlibs/stb_image_write/test/$source.xl"
    timeout 90s release/xl build "release/extlibs/stb_image_write/test/$source.xl" -o "$work/$source.aot"
    timeout 90s "$work/$source.aot"
done
printf 'gray BMP: 58 bytes, pixels at 54\nRGB BMP: 70 bytes, pixels at 54\nRGBA BMP: 126 bytes, pixels at 122\nsource unchanged: true\nstb write BMP layout: OK\n' > "$work/layout.golden"
timeout 120s "$cc" "${flags[@]}" -DXRT_IMPLEMENTATION \
    release/extlibs/stb_image_write/examples/bmp_layout_c.c -lm -ldl -o "$work/layout_gcc"
timeout 90s "$work/layout_gcc" > "$work/layout_gcc.txt"
diff -u "$work/layout.golden" "$work/layout_gcc.txt"
for source in bmp_layout.xl bmp_layout_c.c; do
    timeout 90s release/xl run "release/extlibs/stb_image_write/examples/$source" > "$work/$source.jit.txt"
    timeout 90s release/xl build "release/extlibs/stb_image_write/examples/$source" -o "$work/$source.aot"
    timeout 90s "$work/$source.aot" > "$work/$source.aot.txt"
    diff -u "$work/layout.golden" "$work/$source.jit.txt"
    diff -u "$work/layout.golden" "$work/$source.aot.txt"
done

timeout 120s "$cc" "${flags[@]}" -c release/extlibs/stb_image_write/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
timeout 120s "$cc" "${flags[@]}" release/extlibs/stb_image_write/stb_image_write_native.c \
    release/extlibs/stb_image/stb_image_native.c \
    release/extlibs/stb_image_write/test/test_stb_image_write.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/test_stb_image_write"
timeout 90s "$work/test_stb_image_write"
timeout 90s ./release/xl run test/cases/extlib_stb_image_write.xl
timeout 90s ./release/xl run release/extlibs/stb_image_write/test/boundaries.xl
timeout 90s ./release/xl run release/extlibs/stb_image_write/test/allocations.xl
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
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.checked.txt"
printf 'stb_image_write Linux C ASan/UBSan, BMP budgets and five-format bilingual AOT gate: PASS\n'
