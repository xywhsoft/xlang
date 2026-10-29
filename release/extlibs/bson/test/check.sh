#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-bson-XXXXXX")
cleanup() {
    rm -rf -- "$work"
}
trap cleanup EXIT

cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/include -Irelease/extlibs/bson
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -fsyntax-only release/extlibs/bson/test/abi_contract.c
"$cc" "${flags[@]}" -c release/extlibs/bson/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
for case_name in test_bson_smoke test_boundaries; do
    link_flags=()
    if [[ "$case_name" == test_boundaries ]]; then
        link_flags=(-Wl,--wrap=clock_gettime)
    fi
    "$cc" "${flags[@]}" release/extlibs/bson/bson_native.c \
        "release/extlibs/bson/test/$case_name.c" "$work/xrt_runtime.o" \
        "${link_flags[@]}" -lm -ldl -o "$work/$case_name"
    if [[ "$case_name" == test_boundaries ]]; then
        timeout 120s "$work/$case_name" "$work"
    else
        timeout 120s "$work/$case_name"
    fi
done
"$cc" "${flags[@]}" release/extlibs/bson/examples/bson_c.c \
    "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/bson_example"
timeout 30s "$work/bson_example" > "$work/example.txt"
diff -u release/extlibs/bson/examples/basic.expected.txt "$work/example.txt"
timeout 30s "$work/bson_example" "$work/c.bson" > "$work/c_file.txt"
timeout 45s ./release/xl run release/extlibs/bson/test/boundaries.xl
timeout 45s ./release/xl run release/extlibs/bson/test/bson_test.xl
timeout 45s ./release/xl run release/extlibs/bson/test/allocations.xl
timeout 45s ./release/xl run release/extlibs/bson/examples/bson_c.c > "$work/xl_c_example.txt"
timeout 45s ./release/xl run release/extlibs/bson/examples/bson.xl > "$work/xl_example.txt"
timeout 45s ./release/xl run release/extlibs/bson/examples/bson.xl -- \
    "$work/xlang.bson" > "$work/xl_file.txt"
diff -u "$work/example.txt" "$work/xl_example.txt"
diff -u "$work/example.txt" "$work/xl_c_example.txt"
{ cat release/extlibs/bson/examples/basic.expected.txt; printf 'file=ok\n'; } > "$work/file_expected.txt"
diff -u "$work/file_expected.txt" "$work/c_file.txt"
diff -u "$work/file_expected.txt" "$work/xl_file.txt"
cmp "$work/c.bson" "$work/xlang.bson"
printf 'bson Linux ASan/UBSan gate: PASS\n'
