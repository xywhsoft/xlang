#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-slugify-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cd "$root"

cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -Irelease/extlibs/slugify -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt \
    -Irelease/extlibs/slugify -DXRT_MODULE_BUFFER -DXRT_MODULE_UNICODE \
    release/extlibs/slugify/slugify_native.c

"$cc" "${flags[@]}" -c release/extlibs/slugify/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/slugify/slugify_native.c \
    release/extlibs/slugify/test/test_slugify.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_slugify"
timeout 90s "$work/test_slugify"
"$cc" "${flags[@]}" release/extlibs/slugify/slugify_native.c \
    release/extlibs/slugify/test/test_threads.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_threads"
timeout 90s "$work/test_threads"
"$cc" "${flags[@]}" release/extlibs/slugify/slugify_native.c \
    release/extlibs/slugify/examples/slugify_c.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/example_c"
timeout 30s "$work/example_c" > "$work/example_c.txt"
timeout 30s release/xl run release/extlibs/slugify/examples/slugify_c.c > "$work/example_c_direct.txt"
diff -u "$work/example_c.txt" "$work/example_c_direct.txt"

if [[ ! -x release/xl ]]; then
    printf 'missing Linux release/xl for slugify XLang gate\n' >&2
    exit 1
fi
timeout 90s release/xl run test/cases/extlib_slugify.xl
timeout 90s release/xl run release/extlibs/slugify/test/boundaries.xl
timeout 30s release/xl run release/examples/extlibs/slugify.xl > "$work/example_xl.txt"
diff -u "$work/example_c.txt" "$work/example_xl.txt"
printf 'slugify Linux C ASan/UBSan, threads and XLang gate: PASS\n'
