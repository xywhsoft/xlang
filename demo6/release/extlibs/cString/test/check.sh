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

"$cc" "${flags[@]}" release/extlibs/cString/test/test_cstring.c -o "$work/test_cstring"
timeout 30s "$work/test_cstring"
"$cc" "${flags[@]}" release/extlibs/cString/examples/cString_c.c -o "$work/example"
timeout 30s "$work/example" > "$work/c.txt"
timeout 45s ./release/xl run release/extlibs/cString/examples/cString_c.c > "$work/direct.txt"
timeout 45s ./release/xl run release/extlibs/cString/test/boundaries.xl
timeout 45s ./release/xl run release/extlibs/cString/examples/cString.xl > "$work/xl.txt"
diff -u "$work/c.txt" "$work/direct.txt"
diff -u "$work/c.txt" "$work/xl.txt"
printf 'cString Linux C/XLang gate: PASS\n'
