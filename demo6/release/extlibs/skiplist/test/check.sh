#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-skiplist-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/skiplist/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -fsyntax-only \
    release/extlibs/skiplist/test/abi_contract.c
"$cc" "${flags[@]}" -c release/extlibs/skiplist/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/skiplist/skiplist_native.c \
    release/extlibs/skiplist/test/test_skiplist.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_skiplist"
timeout 90s "$work/test_skiplist"
"$cc" "${flags[@]}" release/extlibs/skiplist/test/entropy_failure.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/entropy_failure"
timeout 30s "$work/entropy_failure"
"$cc" "${flags[@]}" release/examples/extlibs/skiplist_c.c \
    "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/skiplist_example"
timeout 30s "$work/skiplist_example" > "$work/c_example.txt"
diff -u release/extlibs/skiplist/test/skiplist.expected.txt "$work/c_example.txt"
timeout 90s ./release/xl run test/cases/extlib_skiplist.xl
timeout 90s ./release/xl run release/extlibs/skiplist/test/allocations.xl
if timeout 90s ./release/xl check release/extlibs/skiplist/test/private_native.xl > "$work/private_native.txt" 2>&1; then
    printf 'skiplist: native methods must not be public\n' >&2
    exit 1
fi
if ! grep -Fq 'setNative' "$work/private_native.txt" ||
   ! grep -Fq 'access' "$work/private_native.txt"; then
    cat "$work/private_native.txt" >&2
    printf 'skiplist: expected private-access diagnostic\n' >&2
    exit 1
fi
timeout 90s ./release/xl run release/examples/extlibs/skiplist.xl \
    > "$work/xl_example.txt"
diff -u "$work/c_example.txt" "$work/xl_example.txt"
for source in release/extlibs/skiplist/test/aot_core.xl \
    test/cases/extlib_skiplist.xl; do
    name=$(basename "$source" .xl)
    printf 'skiplist Linux AOT: %s\n' "$source"
    timeout 120s ./release/xl build "$source" -o "$work/$name"
    timeout 90s "$work/$name"
done
printf 'skiplist Linux AOT: XLang example\n'
timeout 120s ./release/xl build release/examples/extlibs/skiplist.xl \
    -o "$work/skiplist_xl"
timeout 30s "$work/skiplist_xl" > "$work/skiplist_xl.txt"
diff -u "$work/c_example.txt" "$work/skiplist_xl.txt"
printf 'skiplist Linux AOT: C example\n'
timeout 120s ./release/xl build release/examples/extlibs/skiplist_c.c \
    -o "$work/skiplist_c"
timeout 30s "$work/skiplist_c" > "$work/skiplist_c.txt"
diff -u "$work/c_example.txt" "$work/skiplist_c.txt"
printf 'skiplist Linux AOT: top-level allocation boundaries\n'
timeout 120s ./release/xl build release/extlibs/skiplist/test/allocations.xl \
    -o "$work/allocations"
timeout 90s "$work/allocations"
printf 'skiplist Linux C ASan/UBSan and bilingual AOT gate: PASS\n'
