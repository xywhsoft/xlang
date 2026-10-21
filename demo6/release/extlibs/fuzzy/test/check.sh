#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-fuzzy-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cd "$root"

cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -Irelease/extlibs/fuzzy -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -fsyntax-only release/extlibs/fuzzy/test/abi_contract.c
"$cc" "${flags[@]}" -DXRT_MODULE_ALL -DXRT_IMPLEMENTATION \
    -x c -include xrt.h -c /dev/null -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/fuzzy/fuzzy_native.c \
    release/extlibs/fuzzy/test/test_fuzzy.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_fuzzy"
timeout 90s "$work/test_fuzzy"

"$cc" "${flags[@]}" release/extlibs/fuzzy/fuzzy_native.c \
    release/extlibs/fuzzy/test/test_unterminated.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_unterminated"
timeout 90s "$work/test_unterminated"

"$cc" "${flags[@]}" release/extlibs/fuzzy/fuzzy_native.c \
    release/extlibs/fuzzy/test/test_threads.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_threads"
timeout 90s "$work/test_threads"

"$cc" "${flags[@]}" release/extlibs/fuzzy/fuzzy_native.c \
    release/extlibs/fuzzy/examples/fuzzy_c.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/example_c.txt"
xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'fuzzy: Linux xl runner is missing: %s\n' "$xl" >&2
    exit 1
fi
timeout 90s "$xl" run release/extlibs/fuzzy/test/boundaries.xl
timeout 30s "$xl" run release/extlibs/fuzzy/examples/fuzzy_c.c > "$work/example_direct.txt"
timeout 90s "$xl" run release/examples/extlibs/fuzzy.xl > "$work/example_xl.txt"
diff -u "$work/example_c.txt" "$work/example_direct.txt"
diff -u "$work/example_c.txt" "$work/example_xl.txt"
printf 'fuzzy Linux C ASan/UBSan and XLang parity gate: PASS\n'
