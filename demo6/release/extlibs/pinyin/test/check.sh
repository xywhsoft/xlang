#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-pinyin-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/pinyin/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt \
    -Irelease/extlibs/pinyin/include -DXRT_MODULE_BUFFER \
    -DXRT_MODULE_UNICODE release/extlibs/pinyin/pinyin_native.c

"$cc" "${flags[@]}" -c release/extlibs/pinyin/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/pinyin/pinyin_native.c \
    release/extlibs/pinyin/test/test_pinyin.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_pinyin"
timeout 90s "$work/test_pinyin"

"$cc" "${flags[@]}" release/examples/extlibs/pinyin_c.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/pinyin_c"
timeout 90s "$work/pinyin_c" > "$work/pinyin_c.txt"

if [[ ! -x release/xl ]]; then
    printf 'missing Linux release/xl for pinyin XLang gate\n' >&2
    exit 1
fi
timeout 90s release/xl run test/cases/extlib_pinyin.xl
timeout 90s release/xl run release/extlibs/pinyin/test/boundaries.xl
timeout 90s release/xl run release/extlibs/pinyin/test/allocations.xl
timeout 90s release/xl run release/examples/extlibs/pinyin.xl > "$work/pinyin_xl.txt"
diff -u <(tr -d '\r' < "$work/pinyin_c.txt") "$work/pinyin_xl.txt"
printf 'pinyin Linux C ASan/UBSan and XLang parity gate: PASS\n'
