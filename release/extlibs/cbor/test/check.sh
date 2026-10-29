#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-cbor-XXXXXX")
cleanup() {
    rm -f -- "$work"/xrt_runtime.o "$work"/test_cbor \
        "$work"/test_boundaries "$work"/test_allocations \
        "$work"/cbor_example "$work"/example.txt \
        "$work"/xl_example.txt "$work"/xl_c_example.txt
    rmdir -- "$work"
}
trap cleanup EXIT

cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/cbor
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/cbor/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
for case_name in test_cbor test_boundaries test_allocations; do
    "$cc" "${flags[@]}" release/extlibs/cbor/cbor_native.c \
        "release/extlibs/cbor/test/$case_name.c" "$work/xrt_runtime.o" \
        -lm -ldl -o "$work/$case_name"
    timeout 120s "$work/$case_name"
done
"$cc" "${flags[@]}" release/extlibs/cbor/examples/cbor_c.c \
    "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/cbor_example"
timeout 30s "$work/cbor_example" > "$work/example.txt"
diff -u release/extlibs/cbor/examples/basic.expected.txt "$work/example.txt"
timeout 45s ./release/xl run release/extlibs/cbor/test/boundaries.xl
timeout 45s ./release/xl run release/extlibs/cbor/test/allocations.xl
timeout 45s ./release/xl run release/extlibs/cbor/examples/cbor_c.c > "$work/xl_c_example.txt"
timeout 45s ./release/xl run release/extlibs/cbor/examples/cbor.xl > "$work/xl_example.txt"
diff -u "$work/example.txt" "$work/xl_example.txt"
diff -u "$work/example.txt" "$work/xl_c_example.txt"
printf 'cbor Linux ASan/UBSan gate: PASS\n'
