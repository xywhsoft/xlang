#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-cstring-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -O1 -g -Wall -Wextra -Werror
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

printf '[cString] C sanitizer gate\n'
"$cc" "${flags[@]}" release/extlibs/cString/test/test_cstring.c -o "$work/test_cstring"
timeout 30s "$work/test_cstring"
"$cc" "${flags[@]}" release/extlibs/cString/examples/cString_c.c -o "$work/example"
timeout 30s "$work/example" > "$work/c.txt"
printf '[cString] XLang JIT gate\n'
timeout 45s ./release/xl run release/extlibs/cString/examples/cString_c.c > "$work/direct.txt"
timeout 45s ./release/xl run release/extlibs/cString/test/boundaries.xl
timeout 45s ./release/xl run release/extlibs/cString/examples/cString.xl > "$work/xl.txt"
diff -u "$work/c.txt" "$work/direct.txt"
diff -u "$work/c.txt" "$work/xl.txt"
printf '[cString] XLang AOT gate\n'
timeout 120s ./release/xl build release/extlibs/cString/test/boundaries.xl \
    -o "$work/cstring_boundaries"
timeout 45s "$work/cstring_boundaries"
timeout 120s ./release/xl build release/extlibs/cString/examples/cString.xl \
    -o "$work/cstring_example"
timeout 45s "$work/cstring_example" > "$work/aot.txt"
diff -u "$work/c.txt" "$work/aot.txt"
printf 'cString Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
