#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-consistent-hash-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/consistent_hash
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/consistent_hash/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
for test in test_consistent_hash test_threads; do
    "$cc" "${flags[@]}" release/extlibs/consistent_hash/consistent_hash_native.c \
        "release/extlibs/consistent_hash/test/${test}.c" "$work/xrt_runtime.o" \
        -lm -ldl -o "$work/$test"
    timeout 90s "$work/$test"
done

"$cc" "${flags[@]}" release/extlibs/consistent_hash/examples/consistent_hash_c.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/consistent_hash_example"
timeout 90s "$work/consistent_hash_example" > "$work/consistent_hash_c.txt"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'consistent_hash Linux gate: release/xl is required\n' >&2
    exit 1
fi
timeout 90s "$xl" run \
    release/extlibs/consistent_hash/examples/consistent_hash_c.c > \
    "$work/consistent_hash_c_direct.txt"
diff -u "$work/consistent_hash_c.txt" "$work/consistent_hash_c_direct.txt"
timeout 90s "$xl" run test/cases/extlib_consistent_hash.xl
timeout 90s "$xl" run release/extlibs/consistent_hash/test/boundaries.xl
timeout 90s "$xl" run release/extlibs/consistent_hash/test/allocations.xl
timeout 90s "$xl" run release/extlibs/consistent_hash/test/layout.xl
if timeout 90s "$xl" check release/extlibs/consistent_hash/test/private_native.xl > "$work/private_native.txt" 2>&1; then
    printf 'consistent_hash: native methods must not be public\n' >&2
    exit 1
fi
if ! grep -Fq 'addNative' "$work/private_native.txt" ||
   ! grep -Fq 'access' "$work/private_native.txt"; then
    cat "$work/private_native.txt" >&2
    printf 'consistent_hash: expected private-access diagnostic\n' >&2
    exit 1
fi
timeout 90s "$xl" run release/examples/extlibs/consistent_hash.xl > \
    "$work/consistent_hash_xl.txt"
diff -u "$work/consistent_hash_c.txt" "$work/consistent_hash_xl.txt"
printf 'consistent_hash Linux C/XL ASan/UBSan gate: PASS\n'
