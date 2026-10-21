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
timeout 90s ./release/xl run release/extlibs/stb_image_write/test/output_limit.xl
timeout 90s ./release/xl run release/examples/extlibs/stb_image_write_c.c \
    > "$work/c_example.txt"
timeout 90s ./release/xl run release/examples/extlibs/stb_image_write.xl \
    > "$work/xl_example.txt"
diff -u "$work/c_example.txt" "$work/xl_example.txt"
printf 'stb_image_write Linux C ASan/UBSan and XLang parity gate: PASS\n'
