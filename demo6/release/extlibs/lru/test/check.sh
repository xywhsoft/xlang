#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-lru-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/lru/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/lru/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
for name in test_lru test_threads; do
    "$cc" "${flags[@]}" release/extlibs/lru/lru_native.c \
        "release/extlibs/lru/test/$name.c" "$work/xrt_runtime.o" \
        -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name"
done

"$cc" "${flags[@]}" release/extlibs/lru/examples/lru_c.c \
    "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/lru_example"
timeout 90s "$work/lru_example" > "$work/lru_c.txt"
sed 's/\r$//' release/extlibs/lru/examples/basic.expected.txt > "$work/golden.txt"
diff -u "$work/golden.txt" "$work/lru_c.txt"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'lru Linux gate: release/xl is required\n' >&2
    exit 1
fi
timeout 90s "$xl" run release/extlibs/lru/examples/lru_c.c > \
    "$work/lru_c_direct.txt"
diff -u "$work/lru_c.txt" "$work/lru_c_direct.txt"
timeout 90s "$xl" run test/cases/extlib_lru.xl
timeout 90s "$xl" run release/extlibs/lru/test/boundaries.xl
timeout 90s "$xl" run release/extlibs/lru/test/allocations.xl
if timeout 90s "$xl" check release/extlibs/lru/test/private_native.xl > "$work/private_native.txt" 2>&1; then
    printf 'lru: native methods must not be public\n' >&2
    exit 1
fi
if ! grep -Fq 'putNative' "$work/private_native.txt" ||
   ! grep -Fq 'access' "$work/private_native.txt"; then
    cat "$work/private_native.txt" >&2
    printf 'lru: expected private-access diagnostic\n' >&2
    exit 1
fi
timeout 90s "$xl" run release/examples/extlibs/lru.xl > "$work/lru_xl.txt"
diff -u "$work/lru_c.txt" "$work/lru_xl.txt"
printf 'lru Linux C/XL ASan/UBSan gate: PASS\n'
