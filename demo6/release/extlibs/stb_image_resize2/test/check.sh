#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-stb-resize2-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/stb_image_resize2/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

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
diff -u "$work/example_c.txt" "$work/example_xl.txt"
printf 'stb_image_resize2 Linux C ASan/UBSan and XLang parity gate: PASS\n'
