#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-stb-resize2-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    if ((result == 0)); then rm -rf -- "$work"
    else printf '[stb resize] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
    fi
    exit "$result"
}
trap cleanup EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/stb_image_resize2/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
mapfile -d '' inputs < <(
    { find release/extlibs/stb_image_resize2 release/extlibs/stb_image -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        release/include/xrt_decl.h release/include/xruntime_decl.h release/include/xl_extlib_abi.h \
        test/cases/extlib_stb_image_resize2.xl release/examples/extlibs/stb_image_resize2.xl \
        release/examples/extlibs/stb_image_resize2_c.c release/examples/extlibs/assets/tiny.png
    } | sort -zu
)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"

# ASan 自动启用逐块分配；同时保留普通构建，不能靠停用故障路径验收。
for name in test_stride test_workspace_context; do
    timeout 120s "$cc" "${flags[@]}" -DXRT_IMPLEMENTATION \
        "release/extlibs/stb_image_resize2/test/$name.c" -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name"
    timeout 90s release/xl run "release/extlibs/stb_image_resize2/test/$name.c"
    timeout 90s release/xl build "release/extlibs/stb_image_resize2/test/$name.c" -o "$work/$name.aot"
    timeout 90s "$work/$name.aot"
done
timeout 120s "$cc" -std=c11 -D_GNU_SOURCE -O2 -Wall -Wextra -Werror -DXRT_IMPLEMENTATION \
    -Ilib/xrt -pthread release/extlibs/stb_image_resize2/test/test_stride.c -lm -ldl -o "$work/stride_plain"
timeout 90s "$work/stride_plain"
timeout 90s release/xl run release/extlibs/stb_image_resize2/test/stride.xl
timeout 90s release/xl build release/extlibs/stb_image_resize2/test/stride.xl -o "$work/stride_xl"
timeout 90s "$work/stride_xl"

timeout 120s "$cc" "${flags[@]}" -DXRT_IMPLEMENTATION \
    release/extlibs/stb_image_resize2/examples/padded_rows_c.c -lm -ldl -o "$work/padded_gcc"
printf 'source bytes: 5\nsRGB midpoint: 188\nlinear midpoint: 128\nshort stride rejected\nsource unchanged: true\nstb resize padded rows: OK\n' > "$work/padded.golden"
timeout 90s "$work/padded_gcc" > "$work/padded_gcc.txt"
diff -u "$work/padded.golden" "$work/padded_gcc.txt"
for source in padded_rows.xl padded_rows_c.c; do
    timeout 90s release/xl run "release/extlibs/stb_image_resize2/examples/$source" > "$work/$source.jit.txt"
    timeout 90s release/xl build "release/extlibs/stb_image_resize2/examples/$source" -o "$work/$source.aot"
    timeout 90s "$work/$source.aot" > "$work/$source.aot.txt"
    diff -u "$work/padded.golden" "$work/$source.jit.txt"
    diff -u "$work/padded.golden" "$work/$source.aot.txt"
done

"$cc" "${flags[@]}" -c release/extlibs/stb_image_resize2/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/stb_image_resize2/stb_image_resize2_native.c \
    release/extlibs/stb_image_resize2/test/test_stb_image_resize2.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/test_stb_image_resize2"
timeout 90s "$work/test_stb_image_resize2"

if [[ ! -x release/xl ]]; then
    printf 'missing Linux release/xl for stb_image_resize2 XLang gate\n' >&2
    exit 1
fi
timeout 90s release/xl run test/cases/extlib_stb_image_resize2.xl
timeout 90s release/xl run release/extlibs/stb_image_resize2/test/boundaries.xl
timeout 90s release/xl run release/extlibs/stb_image_resize2/test/allocations.xl
timeout 90s release/xl run release/examples/extlibs/stb_image_resize2_c.c \
    > "$work/example_c.txt"
timeout 90s release/xl run release/examples/extlibs/stb_image_resize2.xl \
    > "$work/example_xl.txt"
printf 'sRGB bytes:  64\nlinear bytes:  36\ngray-alpha bytes:  2\nstb_image_resize2 C/XL example: PASS\n' \
    > "$work/expected.txt"
diff -u "$work/expected.txt" "$work/example_c.txt"
diff -u "$work/example_c.txt" "$work/example_xl.txt"

timeout 90s release/xl build test/cases/extlib_stb_image_resize2.xl -o "$work/resize_core"
timeout 30s "$work/resize_core"
timeout 90s release/xl build release/examples/extlibs/stb_image_resize2.xl -o "$work/resize_xl_aot"
timeout 30s "$work/resize_xl_aot" release/examples/extlibs/assets/tiny.png > "$work/xl_aot.txt"
diff -u "$work/expected.txt" "$work/xl_aot.txt"
timeout 90s release/xl build release/examples/extlibs/stb_image_resize2_c.c -o "$work/resize_c_aot"
timeout 30s "$work/resize_c_aot" release/examples/extlibs/assets/tiny.png > "$work/c_aot.txt"
diff -u "$work/expected.txt" "$work/c_aot.txt"
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.checked.txt"
printf 'stb_image_resize2 Linux C ASan/UBSan, pixel and bilingual AOT gate: PASS\n'
