#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-langdetect-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/langdetect/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -fsyntax-only release/extlibs/langdetect/test/abi_contract.c
"$cc" "${flags[@]}" -c release/extlibs/langdetect/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/langdetect/langdetect_native.c \
    release/extlibs/langdetect/test/test_langdetect.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_langdetect"
timeout 90s "$work/test_langdetect"

"$cc" "${flags[@]}" release/extlibs/langdetect/langdetect_native.c \
    release/extlibs/langdetect/examples/langdetect.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/example_c.txt"
xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'langdetect: Linux xl runner is missing: %s\n' "$xl" >&2
    exit 1
fi
timeout 90s "$xl" run release/extlibs/langdetect/test/analysis_boundaries.xl
timeout 90s "$xl" run test/cases/extlib_langdetect.xl
timeout 90s "$xl" run test/cases/extlib_langdetect_boundaries.xl
timeout 90s "$xl" run release/examples/extlibs/langdetect.xl > "$work/example_xl.txt"
diff -u "$work/example_c.txt" "$work/example_xl.txt"
timeout 90s "$xl" run release/extlibs/langdetect/examples/langdetect.c \
    > "$work/direct_c.txt"
diff -u "$work/example_c.txt" "$work/direct_c.txt"
printf 'langdetect Linux C ASan/UBSan and XLang parity gate: PASS\n'
